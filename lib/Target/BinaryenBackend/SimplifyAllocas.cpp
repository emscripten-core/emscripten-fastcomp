//===-- SimplifyAllocas.cpp - Alloca optimization ---------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===-----------------------------------------------------------------------===//
//
// There shouldn't be any opportunities for this pass to do anything if the
// regular LLVM optimizer passes are run. However, it does make things nicer
// at -O0.
//
//===-----------------------------------------------------------------------===//

#include "OptPasses.h"

#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Constants.h"

#ifdef NDEBUG
#undef assert
#define assert(x) { if (!(x)) report_fatal_error(#x); }
#endif

namespace llvm {

/*
 * Find cases where an alloca is used only to load and store a single value,
 * even though it is bitcast. Then replace it with a direct alloca of that
 * simple type, and avoid the bitcasts.
 */

struct SimplifyAllocas : public FunctionPass {
  static char ID; // Pass identification, replacement for typeid
  SimplifyAllocas() : FunctionPass(ID) {}
    // XXX initialize..(*PassRegistry::getPassRegistry()); }

  bool runOnFunction(Function &Func) override;

  const char *getPassName() const override { return "SimplifyAllocas"; }
};

char SimplifyAllocas::ID = 0;

bool SimplifyAllocas::runOnFunction(Function &Func) {
  bool Changed = false;
  Type *i32 = Type::getInt32Ty(Func.getContext());
  std::vector<Instruction*> ToRemove; // removing can invalidate our iterators, so do it all at the end
  for (Function::iterator B = Func.begin(), E = Func.end(); B != E; ++B) {
    for (BasicBlock::iterator BI = B->begin(), BE = B->end(); BI != BE; ) {
      Instruction *I = &*BI++;
      AllocaInst *AI = dyn_cast<AllocaInst>(I);
      if (!AI) continue;
      if (!isa<ConstantInt>(AI->getArraySize())) continue;
      bool Fail = false;
      Type *ActualType = NULL;
      #define CHECK_TYPE(TT) {              \
        Type *T = TT;                       \
        if (!ActualType) {                  \
          ActualType = T;                   \
        } else {                            \
          if (T != ActualType) Fail = true; \
        }                                   \
      }
      std::vector<Instruction*> Aliases; // the bitcasts of this alloca
      for (Instruction::user_iterator UI = AI->user_begin(), UE = AI->user_end(); UI != UE && !Fail; ++UI) {
        Instruction *U = cast<Instruction>(*UI);
        if (U->getOpcode() != Instruction::BitCast) { Fail = true; break; }
        // bitcasting just to do loads and stores is ok
        for (Instruction::user_iterator BUI = U->user_begin(), BUE = U->user_end(); BUI != BUE && !Fail; ++BUI) {
          Instruction *BU = cast<Instruction>(*BUI);
          if (BU->getOpcode() == Instruction::Load) {
            CHECK_TYPE(BU->getType());
            break;
          }
          if (BU->getOpcode() != Instruction::Store) { Fail = true; break; }
          CHECK_TYPE(BU->getOperand(0)->getType());
          if (BU->getOperand(0) == U) { Fail = true; break; }
        }
        if (!Fail) Aliases.push_back(U);
      }
      if (!Fail && Aliases.size() > 0 && ActualType) {
        // success, replace the alloca and the bitcast aliases with a single simple alloca
        AllocaInst *NA = new AllocaInst(ActualType, ConstantInt::get(i32, 1), "", I);
        NA->takeName(AI);
        NA->setAlignment(AI->getAlignment());
        NA->setDebugLoc(AI->getDebugLoc());
        for (unsigned i = 0; i < Aliases.size(); i++) {
          Aliases[i]->replaceAllUsesWith(NA);
          ToRemove.push_back(Aliases[i]);
        }
        ToRemove.push_back(AI);
        Changed = true;
      }
    }
  }
  for (unsigned i = 0; i < ToRemove.size(); i++) {
    ToRemove[i]->eraseFromParent();
  }
  return Changed;
}

//

extern FunctionPass *createEmscriptenSimplifyAllocasPass() {
  return new SimplifyAllocas();
}

} // End llvm namespace
