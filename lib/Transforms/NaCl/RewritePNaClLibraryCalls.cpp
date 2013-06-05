//===- RewritePNaClLibraryCalls.cpp - PNaCl library calls to intrinsics ---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass replaces calls to known library functions with calls to intrinsics
// that are part of the PNaCl stable bitcode ABI.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/Twine.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

namespace {
  class RewritePNaClLibraryCalls : public ModulePass {
  public:
    static char ID;
    RewritePNaClLibraryCalls() :
        ModulePass(ID), TheModule(NULL) {
      // This is a module pass because it may have to introduce
      // intrinsic declarations into the module and modify a global function.
      initializeRewritePNaClLibraryCallsPass(*PassRegistry::getPassRegistry());
    }

    virtual bool runOnModule(Module &M);
  private:
    /// Rewrites the given \p Call of setjmp to a direct intrinsic call.
    void rewriteSetjmpCall(CallInst *Call);

    /// Rewrites the given \p Call of longjmp to a direct intrinsic call.
    void rewriteLongjmpCall(CallInst *Call);

    /// Populates the body of longjmp as a wrapper of the intrinsic call.
    /// Should only be called once. Modifies the given \p LongjmpFunc.
    void populateLongjmpWrapper(Function *LongjmpFunc);

    /// Sanity check that the types of these functions are what we expect them
    /// to be.
    void sanityCheckSetjmpFunc(Function *SetjmpFunc);
    void sanityCheckLongjmpFunc(Function *LongjmpFunc);

    /// The module this pass runs on.
    Module *TheModule;
  };
}

char RewritePNaClLibraryCalls::ID = 0;
INITIALIZE_PASS(RewritePNaClLibraryCalls, "rewrite-pnacl-library-calls",
                "Rewrite PNaCl library calls to stable intrinsics",
                false, false)

bool RewritePNaClLibraryCalls::runOnModule(Module &M) {
  TheModule = &M;
  bool Changed = false;

  // Iterate over all uses of the setjmp, if it exists in the module with
  // external linkage. If it exists but the linkage is not external, this may
  // come from code that defines its own function named setjmp and doesn't
  // include <setjmp.h>. In such a case we leave the code as it is.
  //
  // The calls are replaced with intrinsics. All other uses of setjmp are
  // disallowed (taking the address of setjmp is disallowed in C and C++).
  Function *SetjmpFunc = TheModule->getFunction("setjmp");

  if (SetjmpFunc && SetjmpFunc->hasExternalLinkage()) {
    sanityCheckSetjmpFunc(SetjmpFunc);

    for (Value::use_iterator UI = SetjmpFunc->use_begin(),
                             UE = SetjmpFunc->use_end(); UI != UE;) {
      Value *Use = *UI++;
      if (CallInst *Call = dyn_cast<CallInst>(Use)) {
        rewriteSetjmpCall(Call);
        Changed = true;
      } else {
        report_fatal_error("Taking the address of setjmp is invalid");
      }
    }
    SetjmpFunc->eraseFromParent();
  }

  // For longjmp things are a little more complicated, since longjmp's address
  // can be taken. Therefore, longjmp can appear in a variety of Uses. The
  // common case is still a direct call and we want that to be as efficient as
  // possible, so we rewrite it into a direct intrinsic call. If there are other
  // uses, the actual body of longjmp is populated with a wrapper that calls
  // the intrinsic.
  Function *LongjmpFunc = TheModule->getFunction("longjmp");

  if (LongjmpFunc && LongjmpFunc->hasExternalLinkage()) {
    sanityCheckLongjmpFunc(LongjmpFunc);

    for (Value::use_iterator UI = LongjmpFunc->use_begin(),
                             UE = LongjmpFunc->use_end(); UI != UE;) {
      Value *Use = *UI++;
      if (CallInst *Call = dyn_cast<CallInst>(Use)) {
        rewriteLongjmpCall(Call);
        Changed = true;
      }
    }

    // If additional uses remain, these aren't calls; populate the wrapper.
    if (LongjmpFunc->use_empty()) {
      LongjmpFunc->eraseFromParent();
    } else {
      populateLongjmpWrapper(LongjmpFunc);
      Changed = true;
    }
  }

  return Changed;
}

void RewritePNaClLibraryCalls::rewriteSetjmpCall(CallInst *Call) {
  // Find the intrinsic function.
  Function *NaClSetjmpFunc = Intrinsic::getDeclaration(TheModule,
                                                       Intrinsic::nacl_setjmp);
  // Cast the jmp_buf argument to the type NaClSetjmpCall expects.
  Type *PtrTy = NaClSetjmpFunc->getFunctionType()->getParamType(0);
  BitCastInst *JmpBufCast = new BitCastInst(Call->getArgOperand(0), PtrTy,
                                            "jmp_buf_i8", Call);
  const DebugLoc &DLoc = Call->getDebugLoc();
  JmpBufCast->setDebugLoc(DLoc);

  // Emit the updated call.
  SmallVector<Value *, 1> Args;
  Args.push_back(JmpBufCast);
  CallInst *NaClSetjmpCall = CallInst::Create(NaClSetjmpFunc, Args, "", Call);
  NaClSetjmpCall->setDebugLoc(DLoc);
  NaClSetjmpCall->takeName(Call);

  // Replace the original call.
  Call->replaceAllUsesWith(NaClSetjmpCall);
  Call->eraseFromParent();
}

void RewritePNaClLibraryCalls::sanityCheckLongjmpFunc(Function *LongjmpFunc) {
  FunctionType *FTy = LongjmpFunc->getFunctionType();
  if (!(FTy->getNumParams() == 2 &&
        FTy->getReturnType()->isVoidTy() &&
        FTy->getParamType(0)->isPointerTy() &&
        FTy->getParamType(1)->isIntegerTy())) {
    report_fatal_error("Wrong signature of longjmp");
  }
}

void RewritePNaClLibraryCalls::sanityCheckSetjmpFunc(Function *SetjmpFunc) {
  FunctionType *FTy = SetjmpFunc->getFunctionType();
  if (!(FTy->getNumParams() == 1 &&
        FTy->getReturnType()->isIntegerTy() &&
        FTy->getParamType(0)->isPointerTy())) {
    report_fatal_error("Wrong signature of setjmp");
  }
}

void RewritePNaClLibraryCalls::rewriteLongjmpCall(CallInst *Call) {
  // Find the intrinsic function.
  Function *NaClLongjmpFunc = Intrinsic::getDeclaration(
      TheModule, Intrinsic::nacl_longjmp);
  // Cast the jmp_buf argument to the type NaClLongjmpCall expects.
  Type *PtrTy = NaClLongjmpFunc->getFunctionType()->getParamType(0);
  BitCastInst *JmpBufCast = new BitCastInst(Call->getArgOperand(0), PtrTy,
                                            "jmp_buf_i8", Call);
  const DebugLoc &DLoc = Call->getDebugLoc();
  JmpBufCast->setDebugLoc(DLoc);

  // Emit the call.
  SmallVector<Value *, 2> Args;
  Args.push_back(JmpBufCast);
  Args.push_back(Call->getArgOperand(1));
  CallInst *NaClLongjmpCall = CallInst::Create(NaClLongjmpFunc, Args, "", Call);
  NaClLongjmpCall->setDebugLoc(DLoc);
  // No takeName here since longjmp is a void call that does not get assigned to
  // a value.

  // Remove the original call. There's no need for RAUW because longjmp
  // returns void.
  Call->eraseFromParent();
}

void RewritePNaClLibraryCalls::populateLongjmpWrapper(Function *LongjmpFunc) {
  assert(LongjmpFunc->size() == 0 &&
      "Expected to be called when longjmp has an empty body");

  // Populate longjmp with code.
  LLVMContext &Context = TheModule->getContext();
  BasicBlock *BB = BasicBlock::Create(Context, "entry", LongjmpFunc);

  Function::arg_iterator LongjmpArgs = LongjmpFunc->arg_begin();
  Value *EnvArg = LongjmpArgs++;
  EnvArg->setName("env");
  Value *ValArg = LongjmpArgs++;
  ValArg->setName("val");

  // Find the intrinsic function.
  Function *NaClLongjmpFunc = Intrinsic::getDeclaration(
      TheModule, Intrinsic::nacl_longjmp);
  // Cast the jmp_buf argument to the type NaClLongjmpCall expects.
  Type *PtrTy = NaClLongjmpFunc->getFunctionType()->getParamType(0);
  BitCastInst *JmpBufCast = new BitCastInst(EnvArg,  PtrTy, "jmp_buf_i8", BB);

  // Emit the call.
  SmallVector<Value *, 2> Args;
  Args.push_back(JmpBufCast);
  Args.push_back(ValArg);
  CallInst::Create(NaClLongjmpFunc, Args, "", BB);

  // Insert an unreachable instruction to terminate this function since longjmp
  // does not return.
  new UnreachableInst(Context, BB);

  // Finally, set the linkage to internal
  LongjmpFunc->setLinkage(Function::InternalLinkage);
}

ModulePass *llvm::createRewritePNaClLibraryCallsPass() {
  return new RewritePNaClLibraryCalls();
}
