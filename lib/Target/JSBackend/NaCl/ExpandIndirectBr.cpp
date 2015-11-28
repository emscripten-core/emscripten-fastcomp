//===- ExpandIndirectBr.cpp - Expand out indirectbr and blockaddress-------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass expands out indirectbr instructions and blockaddress
// ConstantExprs, which are not currently supported in PNaCl's stable
// ABI.  indirectbr is used to implement computed gotos (a GNU
// extension to C).  This pass replaces indirectbr instructions with
// switch instructions.
//
// The resulting use of switches might not be as fast as the original
// indirectbrs.  If you are compiling a program that has a
// compile-time option for using computed gotos, it's possible that
// the program will run faster with the option turned off than with
// using computed gotos + ExpandIndirectBr (for example, if the
// program does extra work to take advantage of computed gotos).
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

namespace {
  // This is a ModulePass so that it can expand out blockaddress
  // ConstantExprs inside global variable initializers.
  class ExpandIndirectBr : public ModulePass {
  public:
    static char ID; // Pass identification, replacement for typeid
    ExpandIndirectBr() : ModulePass(ID) {
      initializeExpandIndirectBrPass(*PassRegistry::getPassRegistry());
    }

    virtual bool runOnModule(Module &M);
  };
}

char ExpandIndirectBr::ID = 0;
INITIALIZE_PASS(ExpandIndirectBr, "expand-indirectbr",
                "Expand out indirectbr and blockaddress (computed gotos)",
                false, false)

static bool convertFunction(Function *Func) {
  bool Changed = false;
  IntegerType *I32 = Type::getInt32Ty(Func->getContext());

  // Skip zero in case programs treat a null pointer as special.
  uint32_t NextNum = 1;
  DenseMap<BasicBlock *, ConstantInt *> LabelNums;
  BasicBlock *DefaultBB = NULL;

  // Replace each indirectbr with a switch.
  //
  // If there are multiple indirectbr instructions in the function,
  // this could be expensive.  While an indirectbr is usually
  // converted to O(1) machine instructions, the switch we generate
  // here will be O(n) in the number of target labels.
  //
  // However, Clang usually generates just a single indirectbr per
  // function anyway when compiling C computed gotos.
  //
  // We could try to generate one switch to handle all the indirectbr
  // instructions in the function, but that would be complicated to
  // implement given that variables that are live at one indirectbr
  // might not be live at others.
  for (llvm::Function::iterator BB = Func->begin(), E = Func->end();
       BB != E; ++BB) {
    if (IndirectBrInst *Br = dyn_cast<IndirectBrInst>(BB->getTerminator())) {
      Changed = true;

      if (!DefaultBB) {
        DefaultBB = BasicBlock::Create(Func->getContext(),
                                       "indirectbr_default", Func);
        new UnreachableInst(Func->getContext(), DefaultBB);
      }

      // An indirectbr can list the same target block multiple times.
      // Keep track of the basic blocks we've handled to avoid adding
      // the same case multiple times.
      DenseSet<BasicBlock *> BlocksSeen;

      Value *Cast = new PtrToIntInst(Br->getAddress(), I32,
                                     "indirectbr_cast", Br);
      unsigned Count = Br->getNumSuccessors();
      SwitchInst *Switch = SwitchInst::Create(Cast, DefaultBB, Count, Br);
      for (unsigned I = 0; I < Count; ++I) {
        BasicBlock *Dest = Br->getSuccessor(I);
        if (!BlocksSeen.insert(Dest).second) {
          // Remove duplicated entries from phi nodes.
          for (BasicBlock::iterator Inst = Dest->begin(); ; ++Inst) {
            PHINode *Phi = dyn_cast<PHINode>(Inst);
            if (!Phi)
              break;
            Phi->removeIncomingValue(Br->getParent());
          }
          continue;
        }
        ConstantInt *Val;
        if (LabelNums.count(Dest) == 0) {
          Val = ConstantInt::get(I32, NextNum++);
          LabelNums[Dest] = Val;

          BlockAddress *BA = BlockAddress::get(Func, Dest);
          Value *ValAsPtr = ConstantExpr::getIntToPtr(Val, BA->getType());
          BA->replaceAllUsesWith(ValAsPtr);
          BA->destroyConstant();
        } else {
          Val = LabelNums[Dest];
        }
        Switch->addCase(Val, Br->getSuccessor(I));
      }
      Br->eraseFromParent();
    }
  }

  // If there are any blockaddresses that are never used by an
  // indirectbr, replace them with dummy values.
  SmallVector<Value *, 20> Users(Func->user_begin(), Func->user_end());
  for (auto U : Users) {
    if (BlockAddress *BA = dyn_cast<BlockAddress>(U)) {
      Changed = true;
      Value *DummyVal = ConstantExpr::getIntToPtr(ConstantInt::get(I32, ~0L),
                                                  BA->getType());
      BA->replaceAllUsesWith(DummyVal);
      BA->destroyConstant();
    }
  }
  return Changed;
}

bool ExpandIndirectBr::runOnModule(Module &M) {
  bool Changed = false;
  for (Module::iterator Func = M.begin(), E = M.end(); Func != E; ++Func) {
    Changed |= convertFunction(&*Func);
  }
  return Changed;
}

ModulePass *llvm::createExpandIndirectBrPass() {
  return new ExpandIndirectBr();
}
