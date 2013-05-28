//===- ResolvePNaClIntrinsics.cpp - Resolve calls to PNaCl intrinsics ----====//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass resolves calls to PNaCl stable bitcode intrinsics. It is
// normally run in the PNaCl translator.
//
// Running AddPNaClExternalDeclsPass is a precondition for running this pass.
// They are separate because one is a ModulePass and the other is a
// FunctionPass.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

namespace {
  class ResolvePNaClIntrinsics : public FunctionPass {
  public:
    ResolvePNaClIntrinsics() : FunctionPass(ID) {
      initializeResolvePNaClIntrinsicsPass(*PassRegistry::getPassRegistry());
    }

    static char ID;
    virtual bool runOnFunction(Function &F);
  private:
    // Some intrinsic calls are resolved simply by replacing the call with a
    // call to an alternative function with exactly the same type.
    bool resolveSimpleCall(Function &F, Intrinsic::ID IntrinsicID,
                           const char *TargetFunctionName);
  };
}

bool ResolvePNaClIntrinsics::resolveSimpleCall(Function &F,
                                               Intrinsic::ID IntrinsicID,
                                               const char *TargetFunctionName) {
  Module *M = F.getParent();
  bool Changed = false;
  Function *IntrinsicFunction = Intrinsic::getDeclaration(M, IntrinsicID);

  if (!IntrinsicFunction) {
    return false;
  }

  // Expect to find the target function for this intrinsic already declared
  Function *TargetFunction = M->getFunction(TargetFunctionName);
  if (!TargetFunction) {
    report_fatal_error(
        std::string("Expected to find external declaration of ") +
        TargetFunctionName);
  }

  for (Value::use_iterator UI = IntrinsicFunction->use_begin(),
                           UE = IntrinsicFunction->use_end(); UI != UE;) {
    // At this point, the only uses of the intrinsic can be calls, since
    // we assume this pass runs on bitcode that passed ABI verification.
    CallInst *Call = dyn_cast<CallInst>(*UI++);

    if (!Call) {
      report_fatal_error(
          std::string("Expected use of intrinsic to be a call: ") +
          Intrinsic::getName(IntrinsicID));
    }

    // To be a well-behaving FunctionPass, don't touch uses in other
    // functions. These will be handled when the pass manager gets to those
    // functions.
    if (Call->getParent()->getParent() == &F) {
      Call->setCalledFunction(TargetFunction);
      Changed = true;
    }
  }

  return Changed;
}

bool ResolvePNaClIntrinsics::runOnFunction(Function &F) {
  bool Changed = resolveSimpleCall(F, Intrinsic::nacl_setjmp, "setjmp");
  Changed |= resolveSimpleCall(F, Intrinsic::nacl_longjmp, "longjmp");
  return Changed;
}

char ResolvePNaClIntrinsics::ID = 0;
INITIALIZE_PASS(ResolvePNaClIntrinsics, "resolve-pnacl-intrinsics",
                "Resolve PNaCl intrinsic calls", false, false)

FunctionPass *llvm::createResolvePNaClIntrinsicsPass() {
  return new ResolvePNaClIntrinsics();
}
