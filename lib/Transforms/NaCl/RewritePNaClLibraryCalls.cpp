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
#include <cstdarg>

using namespace llvm;

namespace {
  class RewritePNaClLibraryCalls : public ModulePass {
  public:
    static char ID;
    RewritePNaClLibraryCalls() :
        ModulePass(ID), TheModule(NULL), Context(NULL),
        LongjmpIntrinsic(NULL), MemcpyIntrinsic(NULL),
        MemmoveIntrinsic(NULL), MemsetIntrinsic(NULL) {
      // This is a module pass because it may have to introduce
      // intrinsic declarations into the module and modify a global function.
      initializeRewritePNaClLibraryCallsPass(*PassRegistry::getPassRegistry());
    }

    virtual bool runOnModule(Module &M);
  private:
    typedef void (RewritePNaClLibraryCalls::*SanityCheckFunc)(Function *);
    typedef void (RewritePNaClLibraryCalls::*RewriteCallFunc)(CallInst *);
    typedef void (RewritePNaClLibraryCalls::*PopulateWrapperFunc)(Function *);

    /// Handles a certain pattern of library function -> intrinsic rewrites.
    /// Currently all library functions this pass knows how to rewrite fall into
    /// this pattern.
    /// RewriteLibraryCall performs the rewrite for a single library function
    /// and is customized by a number of method pointers that collectively
    /// handle one of the supported library functions.
    ///
    /// \p LibraryFunctionName Name of the library function to look for.
    /// \p SanityChecker Method that makes sure the library function has the
    ///    signature we expect it to have.
    /// \p CallRewriter Method that rewrites the library function call into an
    ///    intrinsic call.
    /// \p OnlyCallsAllowed True iff only calls are allowed to this library
    ///    function.
    /// \p WrapperPopulator If not only calls are allowed, this method is
    ///    called to populate the body of the library function with a wrapped
    ///    intrinsic call. If only calls are allowed, this parameter may be set
    ///    to NULL.
    bool RewriteLibraryCall(
        const char *LibraryFunctionName,
        SanityCheckFunc SanityChecker,
        RewriteCallFunc CallRewriter,
        bool OnlyCallsAllowed,
        PopulateWrapperFunc WrapperPopulator);

    void sanityCheckSetjmpFunc(Function *SetjmpFunc);
    void sanityCheckLongjmpFunc(Function *LongjmpFunc);
    void sanityCheckMemcpyFunc(Function *MemcpyFunc);
    void sanityCheckMemmoveFunc(Function *MemmoveFunc);
    void sanityCheckMemsetFunc(Function *MemsetFunc);

    void rewriteSetjmpCall(CallInst *Call);
    void rewriteLongjmpCall(CallInst *Call);
    void rewriteMemcpyCall(CallInst *Call);
    void rewriteMemmoveCall(CallInst *Call);
    void rewriteMemsetCall(CallInst *Call);

    void populateLongjmpWrapper(Function *LongjmpFunc);
    void populateMemcpyWrapper(Function *MemcpyFunc);
    void populateMemmoveWrapper(Function *MemmoveFunc);
    void populateMemsetWrapper(Function *MemsetFunc);

    /// Generic implementation of populating a wrapper function.
    /// Initially, the function exists in the module as a declaration with
    /// unnamed arguments. This method is called with a NULL-terminated list
    /// of argument names that get assigned in the generated IR for
    /// readability.
    void populateWrapperCommon(
        Function *Func,
        StringRef FuncName,
        RewriteCallFunc CallRewriter,
        bool CallCannotReturn,
        ...);

    /// Find and cache known intrinsics.
    Function *findLongjmpIntrinsic();
    Function *findMemcpyIntrinsic();
    Function *findMemmoveIntrinsic();
    Function *findMemsetIntrinsic();

    /// Cached data that remains the same throughout a module run.
    Module *TheModule;
    LLVMContext *Context;

    /// These are cached but computed lazily.
    Function *LongjmpIntrinsic;
    Function *MemcpyIntrinsic;
    Function *MemmoveIntrinsic;
    Function *MemsetIntrinsic;
  };
}

char RewritePNaClLibraryCalls::ID = 0;
INITIALIZE_PASS(RewritePNaClLibraryCalls, "rewrite-pnacl-library-calls",
                "Rewrite PNaCl library calls to stable intrinsics",
                false, false)

bool RewritePNaClLibraryCalls::RewriteLibraryCall(
    const char *LibraryFunctionName,
    SanityCheckFunc SanityChecker,
    RewriteCallFunc CallRewriter,
    bool OnlyCallsAllowed,
    PopulateWrapperFunc WrapperPopulator) {
  bool Changed = false;

  Function *LibFunc = TheModule->getFunction(LibraryFunctionName);

  // Iterate over all uses of this function, if it exists in the module with
  // external linkage. If it exists but the linkage is not external, this may
  // come from code that defines its own private function with the same name
  // and doesn't actually include the standard libc header declaring it.
  // In such a case we leave the code as it is.
  if (LibFunc && LibFunc->hasExternalLinkage()) {
    (this->*(SanityChecker))(LibFunc);

    // Handle all uses that are calls. These are simply replaced with
    // equivalent intrinsic calls.
    for (Value::use_iterator UI = LibFunc->use_begin(),
                             UE = LibFunc->use_end(); UI != UE;) {
      Value *Use = *UI++;
      if (CallInst *Call = dyn_cast<CallInst>(Use)) {
        (this->*(CallRewriter))(Call);
        Changed = true;
      }
    }

    if (LibFunc->use_empty()) {
      LibFunc->eraseFromParent();
    } else if (OnlyCallsAllowed) {
      // If additional uses remain, these aren't calls.
      report_fatal_error(Twine("Taking the address of ") +
                         LibraryFunctionName + " is invalid");
    } else {
      // If non-call uses remain and allowed for this function, populate it
      // with a wrapper.
      (this->*(WrapperPopulator))(LibFunc);
      LibFunc->setLinkage(Function::InternalLinkage);
      Changed = true;
    }
  }

  return Changed;
}

bool RewritePNaClLibraryCalls::runOnModule(Module &M) {
  TheModule = &M;
  Context = &TheModule->getContext();
  bool Changed = false;

  Changed |= RewriteLibraryCall(
      "setjmp",
      &RewritePNaClLibraryCalls::sanityCheckSetjmpFunc,
      &RewritePNaClLibraryCalls::rewriteSetjmpCall,
      true,
      NULL);
  Changed |= RewriteLibraryCall(
      "longjmp",
      &RewritePNaClLibraryCalls::sanityCheckLongjmpFunc,
      &RewritePNaClLibraryCalls::rewriteLongjmpCall,
      false,
      &RewritePNaClLibraryCalls::populateLongjmpWrapper);
  Changed |= RewriteLibraryCall(
      "memset",
      &RewritePNaClLibraryCalls::sanityCheckMemsetFunc,
      &RewritePNaClLibraryCalls::rewriteMemsetCall,
      false,
      &RewritePNaClLibraryCalls::populateMemsetWrapper);
  Changed |= RewriteLibraryCall(
      "memcpy",
      &RewritePNaClLibraryCalls::sanityCheckMemcpyFunc,
      &RewritePNaClLibraryCalls::rewriteMemcpyCall,
      false,
      &RewritePNaClLibraryCalls::populateMemcpyWrapper);
  Changed |= RewriteLibraryCall(
      "memmove",
      &RewritePNaClLibraryCalls::sanityCheckMemmoveFunc,
      &RewritePNaClLibraryCalls::rewriteMemmoveCall,
      false,
      &RewritePNaClLibraryCalls::populateMemmoveWrapper);

  return Changed;
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

void RewritePNaClLibraryCalls::sanityCheckMemsetFunc(Function *MemsetFunc) {
  FunctionType *FTy = MemsetFunc->getFunctionType();
  if (!(FTy->getNumParams() == 3 &&
        FTy->getReturnType()->isPointerTy() &&
        FTy->getParamType(0)->isPointerTy() &&
        FTy->getParamType(1)->isIntegerTy() &&
        FTy->getParamType(2)->isIntegerTy())) {
    report_fatal_error("Wrong signature of memset");
  }
}

void RewritePNaClLibraryCalls::sanityCheckMemcpyFunc(Function *MemcpyFunc) {
  FunctionType *FTy = MemcpyFunc->getFunctionType();
  if (!(FTy->getNumParams() == 3 &&
        FTy->getReturnType()->isPointerTy() &&
        FTy->getParamType(0)->isPointerTy() &&
        FTy->getParamType(1)->isPointerTy() &&
        FTy->getParamType(2)->isIntegerTy())) {
    report_fatal_error("Wrong signature of memcpy");
  }
}

void RewritePNaClLibraryCalls::sanityCheckMemmoveFunc(Function *MemmoveFunc) {
  FunctionType *FTy = MemmoveFunc->getFunctionType();
  if (!(FTy->getNumParams() == 3 &&
        FTy->getReturnType()->isPointerTy() &&
        FTy->getParamType(0)->isPointerTy() &&
        FTy->getParamType(1)->isPointerTy() &&
        FTy->getParamType(2)->isIntegerTy())) {
    report_fatal_error("Wrong signature of memmove");
  }
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
  Value *Args[] = { JmpBufCast };
  CallInst *NaClSetjmpCall = CallInst::Create(NaClSetjmpFunc, Args, "", Call);
  NaClSetjmpCall->setDebugLoc(DLoc);
  NaClSetjmpCall->takeName(Call);

  // Replace the original call.
  Call->replaceAllUsesWith(NaClSetjmpCall);
  Call->eraseFromParent();
}

void RewritePNaClLibraryCalls::rewriteLongjmpCall(CallInst *Call) {
  // Find the intrinsic function.
  Function *NaClLongjmpFunc = findLongjmpIntrinsic();
  // Cast the jmp_buf argument to the type NaClLongjmpCall expects.
  Type *PtrTy = NaClLongjmpFunc->getFunctionType()->getParamType(0);
  BitCastInst *JmpBufCast = new BitCastInst(Call->getArgOperand(0), PtrTy,
                                            "jmp_buf_i8", Call);
  const DebugLoc &DLoc = Call->getDebugLoc();
  JmpBufCast->setDebugLoc(DLoc);

  // Emit the call.
  Value *Args[] = { JmpBufCast, Call->getArgOperand(1) };
  CallInst *NaClLongjmpCall = CallInst::Create(NaClLongjmpFunc, Args, "", Call);
  NaClLongjmpCall->setDebugLoc(DLoc);
  // No takeName here since longjmp is a void call that does not get assigned to
  // a value.

  // Remove the original call. There's no need for RAUW because longjmp
  // returns void.
  Call->eraseFromParent();
}

void RewritePNaClLibraryCalls::rewriteMemcpyCall(CallInst *Call) {
  Function *MemcpyIntrinsic = findMemcpyIntrinsic();
  // dest, src, len, align, isvolatile
  Value *Args[] = { Call->getArgOperand(0),
                    Call->getArgOperand(1),
                    Call->getArgOperand(2),
                    ConstantInt::get(Type::getInt32Ty(*Context), 1),
                    ConstantInt::get(Type::getInt1Ty(*Context), 0) };
  CallInst *MemcpyIntrinsicCall = CallInst::Create(MemcpyIntrinsic,
                                                   Args, "", Call);
  MemcpyIntrinsicCall->setDebugLoc(Call->getDebugLoc());

  // libc memcpy returns the source pointer, but the LLVM intrinsic doesn't; if
  // the return value has actual uses, just replace them with the dest
  // argument itself.
  Call->replaceAllUsesWith(Call->getArgOperand(0));
  Call->eraseFromParent();
}

void RewritePNaClLibraryCalls::rewriteMemmoveCall(CallInst *Call) {
  Function *MemmoveIntrinsic = findMemmoveIntrinsic();
  // dest, src, len, align, isvolatile
  Value *Args[] = { Call->getArgOperand(0),
                    Call->getArgOperand(1),
                    Call->getArgOperand(2),
                    ConstantInt::get(Type::getInt32Ty(*Context), 1),
                    ConstantInt::get(Type::getInt1Ty(*Context), 0) };
  CallInst *MemmoveIntrinsicCall = CallInst::Create(MemmoveIntrinsic,
                                                    Args, "", Call);
  MemmoveIntrinsicCall->setDebugLoc(Call->getDebugLoc());

  // libc memmove returns the source pointer, but the LLVM intrinsic doesn't; if
  // the return value has actual uses, just replace them with the dest
  // argument itself.
  Call->replaceAllUsesWith(Call->getArgOperand(0));
  Call->eraseFromParent();
}

void RewritePNaClLibraryCalls::rewriteMemsetCall(CallInst *Call) {
  Function *MemsetIntrinsic = findMemsetIntrinsic();
  // libc memset has 'int c' for the filler byte, but the LLVM intrinsic uses
  // a i8; truncation is required.
  TruncInst *ByteTrunc = new TruncInst(Call->getArgOperand(1),
                                       Type::getInt8Ty(*Context),
                                       "trunc_byte", Call);

  const DebugLoc &DLoc = Call->getDebugLoc();
  ByteTrunc->setDebugLoc(DLoc);

  // dest, val, len, align, isvolatile
  Value *Args[] = { Call->getArgOperand(0),
                    ByteTrunc,
                    Call->getArgOperand(2),
                    ConstantInt::get(Type::getInt32Ty(*Context), 1),
                    ConstantInt::get(Type::getInt1Ty(*Context), 0) };
  CallInst *MemsetIntrinsicCall = CallInst::Create(MemsetIntrinsic,
                                                   Args, "", Call);
  MemsetIntrinsicCall->setDebugLoc(DLoc);

  // libc memset returns the source pointer, but the LLVM intrinsic doesn't; if
  // the return value has actual uses, just replace them with the dest
  // argument itself.
  Call->replaceAllUsesWith(Call->getArgOperand(0));
  Call->eraseFromParent();
}

void RewritePNaClLibraryCalls::populateWrapperCommon(
      Function *Func,
      StringRef FuncName,
      RewriteCallFunc CallRewriter,
      bool CallCannotReturn,
      ...) {
  if (!Func->isDeclaration()) {
    report_fatal_error(Twine("Expected ") + FuncName +
                       " to be declared, not defined");
  }

  // Populate the function body with code.
  BasicBlock *BB = BasicBlock::Create(*Context, "entry", Func);

  // Collect and name the function arguments.
  Function::arg_iterator FuncArgs = Func->arg_begin();
  SmallVector<Value *, 4> Args;
  va_list ap;
  va_start(ap, CallCannotReturn);
  while (true) {
    // Iterate over the varargs until a terminated NULL is encountered.
    const char *ArgName = va_arg(ap, const char *);
    if (!ArgName)
      break;
    Value *Arg = FuncArgs++;
    Arg->setName(ArgName);
    Args.push_back(Arg);
  }
  va_end(ap);

  // Emit a call to self, and then call CallRewriter to rewrite it to the
  // intrinsic. This is done in order to keep the call rewriting logic in a
  // single place.
  CallInst *SelfCall = CallInst::Create(Func, Args, "", BB);

  if (CallCannotReturn) {
    new UnreachableInst(*Context, BB);
  } else if (Func->getReturnType()->isVoidTy()) {
    ReturnInst::Create(*Context, BB);
  } else {
    ReturnInst::Create(*Context, SelfCall, BB);
  }

  (this->*(CallRewriter))(SelfCall);
}

void RewritePNaClLibraryCalls::populateLongjmpWrapper(Function *LongjmpFunc) {
  populateWrapperCommon(
      /* Func             */ LongjmpFunc,
      /* FuncName         */ "longjmp",
      /* CallRewriter     */ &RewritePNaClLibraryCalls::rewriteLongjmpCall,
      /* CallCannotReturn */ true,
      /* ...              */ "env", "val", NULL);
}

void RewritePNaClLibraryCalls::populateMemcpyWrapper(Function *MemcpyFunc) {
  populateWrapperCommon(
      /* Func             */ MemcpyFunc,
      /* FuncName         */ "memcpy",
      /* CallRewriter     */ &RewritePNaClLibraryCalls::rewriteMemcpyCall,
      /* CallCannotReturn */ false,
      /* ...              */ "dest", "src", "len", NULL);
}

void RewritePNaClLibraryCalls::populateMemmoveWrapper(Function *MemmoveFunc) {
  populateWrapperCommon(
      /* Func             */ MemmoveFunc,
      /* FuncName         */ "memmove",
      /* CallRewriter     */ &RewritePNaClLibraryCalls::rewriteMemmoveCall,
      /* CallCannotReturn */ false,
      /* ...              */ "dest", "src", "len", NULL);
}

void RewritePNaClLibraryCalls::populateMemsetWrapper(Function *MemsetFunc) {
  populateWrapperCommon(
      /* Func             */ MemsetFunc,
      /* FuncName         */ "memset",
      /* CallRewriter     */ &RewritePNaClLibraryCalls::rewriteMemsetCall,
      /* CallCannotReturn */ false,
      /* ...              */ "dest", "val", "len", NULL);
}

Function *RewritePNaClLibraryCalls::findLongjmpIntrinsic() {
  if (!LongjmpIntrinsic) {
    LongjmpIntrinsic = Intrinsic::getDeclaration(
        TheModule, Intrinsic::nacl_longjmp);
  }
  return LongjmpIntrinsic;
}

Function *RewritePNaClLibraryCalls::findMemcpyIntrinsic() {
  if (!MemcpyIntrinsic) {
    Type *Tys[] = { Type::getInt8PtrTy(*Context),
                    Type::getInt8PtrTy(*Context),
                    Type::getInt32Ty(*Context) };
    MemcpyIntrinsic = Intrinsic::getDeclaration(
        TheModule, Intrinsic::memcpy, Tys);
  }
  return MemcpyIntrinsic;
}

Function *RewritePNaClLibraryCalls::findMemmoveIntrinsic() {
  if (!MemmoveIntrinsic) {
    Type *Tys[] = { Type::getInt8PtrTy(*Context),
                    Type::getInt8PtrTy(*Context),
                    Type::getInt32Ty(*Context) };
    MemmoveIntrinsic = Intrinsic::getDeclaration(
        TheModule, Intrinsic::memmove, Tys);
  }
  return MemmoveIntrinsic;
}

Function *RewritePNaClLibraryCalls::findMemsetIntrinsic() {
  if (!MemsetIntrinsic) {
    Type *Tys[] = { Type::getInt8PtrTy(*Context), Type::getInt32Ty(*Context) };
    MemsetIntrinsic = Intrinsic::getDeclaration(
        TheModule, Intrinsic::memset, Tys);
  }
  return MemsetIntrinsic;
}

ModulePass *llvm::createRewritePNaClLibraryCallsPass() {
  return new RewritePNaClLibraryCalls();
}
