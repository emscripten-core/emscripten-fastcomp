//===- RewriteLLVMIntrinsics.cpp - Rewrite LLVM intrinsics to other values ===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass replaces calls to LLVM intrinsics that are *not* part of the
// PNaCl stable bitcode ABI into simpler values.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

namespace {
  class RewriteLLVMIntrinsics : public ModulePass {
  public:
    static char ID;
    RewriteLLVMIntrinsics() : ModulePass(ID) {
      // This is a module pass because this makes it easier to access uses
      // of global intrinsic functions.
      initializeRewriteLLVMIntrinsicsPass(*PassRegistry::getPassRegistry());
    }

    virtual bool runOnModule(Module &M);
  };
}

char RewriteLLVMIntrinsics::ID = 0;
INITIALIZE_PASS(RewriteLLVMIntrinsics, "rewrite-llvm-intrinsic-calls",
                "Rewrite LLVM intrinsic calls to simpler expressions",
                false, false)

bool RewriteLLVMIntrinsics::runOnModule(Module &M) {
  bool Changed = false;

  // Iterate over all uses of the llvm.flt.rounds, and replace it with
  // the constant "1" (round-to-nearest).  Until we add a second intrinsic
  // like llvm.set.flt.round it is impossible to have a rounding mode
  // that is not the initial rounding mode (round-to-nearest).
  // We can remove this rewrite after adding a set() intrinsic.
  Function *FltRounds = Intrinsic::getDeclaration(&M, Intrinsic::flt_rounds);
  Type *RetType = FltRounds->getFunctionType()->getReturnType();
  for (Value::use_iterator UI = FltRounds->use_begin(),
           UE = FltRounds->use_end(); UI != UE;) {
    Value *Use = *UI++;
    if (CallInst *Call = dyn_cast<CallInst>(Use)) {
      Constant *C = ConstantInt::get(RetType, 1);
      Call->replaceAllUsesWith(C);
      Call->eraseFromParent();
      Changed = true;
    } else {
      report_fatal_error("Taking the address of llvm.flt.rounds is invalid");
    }
  }
  FltRounds->eraseFromParent();

  return Changed;
}

ModulePass *llvm::createRewriteLLVMIntrinsicsPass() {
  return new RewriteLLVMIntrinsics();
}
