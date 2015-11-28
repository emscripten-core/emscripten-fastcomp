//===- InsertDivideCheck.cpp - Add divide by zero checks ------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass adds a check for divide by zero before every integer DIV or REM.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "add-divide-check"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

namespace {
  class InsertDivideCheck : public FunctionPass {
  public:
    static char ID;
    InsertDivideCheck() : FunctionPass(ID) {
      initializeInsertDivideCheckPass(*PassRegistry::getPassRegistry());
    }

    bool runOnFunction(Function &F);
  };
}

static BasicBlock *CreateTrapBlock(Function &F, DebugLoc dl) {
  BasicBlock *TrapBlock = BasicBlock::Create(F.getContext(), "divrem.by.zero",
                                             &F);
  Value *TrapFn = Intrinsic::getDeclaration(F.getParent(), Intrinsic::trap);
  CallInst::Create(TrapFn, "", TrapBlock)->setDebugLoc(dl);
  (new UnreachableInst(F.getContext(), TrapBlock))->setDebugLoc(dl);
  return TrapBlock;
}

bool InsertDivideCheck::runOnFunction(Function &F) {
  SmallPtrSet<Instruction*, 8> GuardedDivs;
  // If the pass finds a DIV/REM that needs to be checked for zero denominator,
  // it will insert a new "trap" block, and split the block that contains the
  // DIV/REM into two blocks.  The new BasicBlocks are added after the current
  // BasicBlock, so that if there is more than one DIV/REM in the same block,
  // all are visited.
  for (Function::iterator I = F.begin(); I != F.end(); I++) {
    BasicBlock *BB = &*I;

    for (BasicBlock::iterator BI = BB->begin(), BE = BB->end();
         BI != BE; BI++) {
      BinaryOperator *DivInst = dyn_cast<BinaryOperator>(BI);
      if (!DivInst || (GuardedDivs.count(DivInst) != 0))
        continue;
      unsigned Opcode = DivInst->getOpcode();
      if (Opcode != Instruction::SDiv && Opcode != Instruction::UDiv &&
          Opcode != Instruction::SRem && Opcode != Instruction::URem)
        continue;
      Value *Denominator = DivInst->getOperand(1);
      if (!Denominator->getType()->isIntegerTy())
        continue;
      DebugLoc dl = DivInst->getDebugLoc();
      if (ConstantInt *DenomConst = dyn_cast<ConstantInt>(Denominator)) {
        // Divides by constants do not need a denominator test.
        if (DenomConst->isZero()) {
          // For explicit divides by zero, insert a trap before DIV/REM
          Value *TrapFn = Intrinsic::getDeclaration(F.getParent(),
                                                    Intrinsic::trap);
          CallInst::Create(TrapFn, "", DivInst)->setDebugLoc(dl);
        }
        continue;
      }
      // Create a trap block.
      BasicBlock *TrapBlock = CreateTrapBlock(F, dl);
      // Move instructions in BB from DivInst to BB's end to a new block.
      BasicBlock *Successor = BB->splitBasicBlock(BI, "guarded.divrem");
      // Remove the unconditional branch inserted by splitBasicBlock.
      BB->getTerminator()->eraseFromParent();
      // Remember that DivInst was already processed, so that when we process
      // inserted blocks later, we do not attempt to again guard it.
      GuardedDivs.insert(DivInst);
      // Compare the denominator with zero.
      Value *Zero = ConstantInt::get(Denominator->getType(), 0);
      Value *DenomIsZero = new ICmpInst(*BB, ICmpInst::ICMP_EQ, Denominator,
                                        Zero, "");
      // Put in a condbranch to the trap block.
      BranchInst::Create(TrapBlock, Successor, DenomIsZero, BB);
      // BI is invalidated when we split.  Stop the BasicBlock iterator.
      break;
    }
  }

  return false;
}

char InsertDivideCheck::ID = 0;
INITIALIZE_PASS(InsertDivideCheck, "insert-divide-check",
                "Insert divide by zero checks", false, false)

FunctionPass *llvm::createInsertDivideCheckPass() {
  return new InsertDivideCheck();
}
