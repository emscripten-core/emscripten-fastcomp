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
#include "llvm/IR/Constants.h"
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
        ModulePass(ID), TheModule(NULL), Context(NULL), SetjmpIntrinsic(NULL),
        LongjmpIntrinsic(NULL), MemcpyIntrinsic(NULL),
        MemmoveIntrinsic(NULL), MemsetIntrinsic(NULL) {
      // This is a module pass because it may have to introduce
      // intrinsic declarations into the module and modify globals.
      initializeRewritePNaClLibraryCallsPass(*PassRegistry::getPassRegistry());
    }

    virtual bool runOnModule(Module &M);
  private:
    typedef void (RewritePNaClLibraryCalls::*RewriteCallFunc)(CallInst *);
    typedef void (RewritePNaClLibraryCalls::*PopulateWrapperFunc)(Function *);

    /// Handles a certain pattern of library function -> intrinsic rewrites.
    /// Currently all library functions this pass knows how to rewrite fall into
    /// this pattern.
    /// RewriteLibraryCall performs the rewrite for a single library function
    /// and is customized by its arguments.
    ///
    /// \p LibraryFunctionName Name of the library function to look for.
    /// \p CorrectFunctionType is the correct type of this library function.
    /// \p CallRewriter Method that rewrites the library function call into an
    ///    intrinsic call.
    /// \p OnlyCallsAllowed Only calls to this library function are allowed.
    /// \p WrapperPopulator called to populate the body of the library function
    ///    with a wrapped intrinsic call.
    bool RewriteLibraryCall(
        const char *LibraryFunctionName,
        FunctionType *CorrectFunctionType,
        RewriteCallFunc CallRewriter,
        bool OnlyCallsAllowed,
        PopulateWrapperFunc WrapperPopulator);

    /// Two function types are compatible if they have compatible return types
    /// and the same number of compatible parameters. Return types and
    /// parameters are compatible if they are exactly the same type or both are
    /// pointer types.
    static bool compatibleFunctionTypes(FunctionType *FTy1, FunctionType *FTy2);
    static bool compatibleParamOrRetTypes(Type *Ty1, Type *Ty2);

    void rewriteSetjmpCall(CallInst *Call);
    void rewriteLongjmpCall(CallInst *Call);
    void rewriteMemcpyCall(CallInst *Call);
    void rewriteMemmoveCall(CallInst *Call);
    void rewriteMemsetCall(CallInst *Call);

    void populateSetjmpWrapper(Function *SetjmpFunc);
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
    Function *findSetjmpIntrinsic();
    Function *findLongjmpIntrinsic();
    Function *findMemcpyIntrinsic();
    Function *findMemmoveIntrinsic();
    Function *findMemsetIntrinsic();

    /// Cached data that remains the same throughout a module run.
    Module *TheModule;
    LLVMContext *Context;

    /// These are cached but computed lazily.
    Function *SetjmpIntrinsic;
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
    FunctionType *CorrectFunctionType,
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
  //
  // Another case we need to handle here is this function having the wrong
  // prototype (incompatible with the C library function prototype, and hence
  // incompatible with the intrinsic). In general, this is undefined behavior,
  // but we can't fail compilation because some workflows rely on it
  // compiling correctly (for example, autoconf). The solution is:
  // When the declared type of the function in the module is not correct, we
  // re-create the function with the correct prototype and replace all calls
  // to this new function (casted to the old function type). Effectively this
  // delays the undefined behavior until run-time.
  if (LibFunc && LibFunc->hasExternalLinkage()) {
    if (!compatibleFunctionTypes(LibFunc->getFunctionType(),
                                 CorrectFunctionType)) {
      // Use the RecreateFunction utility to create a new function with the
      // correct prototype. RecreateFunction also RAUWs the function with
      // proper bitcasts.
      //
      // One interesting case that may arise is when the original module had
      // calls to both a correct and an incorrect version of the library
      // function. Depending on the linking order, either version could be
      // selected as the global declaration in the module, so even valid calls
      // could end up being bitcast-ed from the incorrect to the correct
      // function type. The RecreateFunction call below will eliminate such
      // bitcasts (because the new type matches the call type), but dead
      // constant expressions may be left behind.
      // These are cleaned up with removeDeadConstantUsers.
      Function *NewFunc = RecreateFunction(LibFunc, CorrectFunctionType);
      LibFunc->eraseFromParent();
      NewFunc->setLinkage(Function::InternalLinkage);
      Changed = true;
      NewFunc->removeDeadConstantUsers();
      LibFunc = NewFunc;
    }

    // Handle all uses that are calls. These are simply replaced with
    // equivalent intrinsic calls.
    {
      SmallVector<CallInst *, 32> Calls;
      for (User *U : LibFunc->users())
        // users() will also provide call instructions in which the used value
        // is an argument, and not the value being called. Make sure we rewrite
        // only actual calls to LibFunc here.
        if (CallInst *Call = dyn_cast<CallInst>(U))
          if (Call->getCalledValue() == LibFunc)
            Calls.push_back(Call);

      for (auto Call : Calls)
        (this->*(CallRewriter))(Call);

      Changed |= !Calls.empty();
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

  Type *Int8PtrTy = Type::getInt8PtrTy(*Context);
  Type *Int64PtrTy = Type::getInt64PtrTy(*Context);
  Type *Int32Ty = Type::getInt32Ty(*Context);
  Type *VoidTy = Type::getVoidTy(*Context);

  Type *SetjmpParams[] = { Int64PtrTy };
  FunctionType *SetjmpFunctionType = FunctionType::get(Int32Ty, SetjmpParams,
                                                       false);
  Changed |= RewriteLibraryCall(
      "setjmp",
      SetjmpFunctionType,
      &RewritePNaClLibraryCalls::rewriteSetjmpCall,
      true,
      &RewritePNaClLibraryCalls::populateSetjmpWrapper);

  Type *LongjmpParams[] = { Int64PtrTy, Int32Ty };
  FunctionType *LongjmpFunctionType = FunctionType::get(VoidTy, LongjmpParams,
                                                        false);
  Changed |= RewriteLibraryCall(
      "longjmp",
      LongjmpFunctionType,
      &RewritePNaClLibraryCalls::rewriteLongjmpCall,
      false,
      &RewritePNaClLibraryCalls::populateLongjmpWrapper);

  Type *MemsetParams[] = { Int8PtrTy, Int32Ty, Int32Ty };
  FunctionType *MemsetFunctionType = FunctionType::get(Int8PtrTy, MemsetParams,
                                                       false);
  Changed |= RewriteLibraryCall(
      "memset",
      MemsetFunctionType,
      &RewritePNaClLibraryCalls::rewriteMemsetCall,
      false,
      &RewritePNaClLibraryCalls::populateMemsetWrapper);

  Type *MemcpyParams[] = { Int8PtrTy, Int8PtrTy, Int32Ty };
  FunctionType *MemcpyFunctionType = FunctionType::get(Int8PtrTy, MemcpyParams,
                                                       false);
  Changed |= RewriteLibraryCall(
      "memcpy",
      MemcpyFunctionType,
      &RewritePNaClLibraryCalls::rewriteMemcpyCall,
      false,
      &RewritePNaClLibraryCalls::populateMemcpyWrapper);

  Type *MemmoveParams[] = { Int8PtrTy, Int8PtrTy, Int32Ty };
  FunctionType *MemmoveFunctionType = FunctionType::get(Int8PtrTy,
                                                        MemmoveParams,
                                                        false);
  Changed |= RewriteLibraryCall(
      "memmove",
      MemmoveFunctionType,
      &RewritePNaClLibraryCalls::rewriteMemmoveCall,
      false,
      &RewritePNaClLibraryCalls::populateMemmoveWrapper);

  return Changed;
}

bool RewritePNaClLibraryCalls::compatibleFunctionTypes(FunctionType *FTy1,
                                                       FunctionType *FTy2) {
  if (FTy1->getNumParams() != FTy2->getNumParams()) {
    return false;
  }

  if (!compatibleParamOrRetTypes(FTy1->getReturnType(),
                                 FTy2->getReturnType())) {
    return false;
  }

  for (unsigned I = 0, End = FTy1->getNumParams(); I != End; ++I) {
    if (!compatibleParamOrRetTypes(FTy1->getParamType(I), 
                                   FTy2->getParamType(I))) {
      return false;
    }
  }

  return true;
}

bool RewritePNaClLibraryCalls::compatibleParamOrRetTypes(Type *Ty1,
                                                         Type *Ty2) {
  return (Ty1 == Ty2 || (Ty1->isPointerTy() && Ty2->isPointerTy()));
}

void RewritePNaClLibraryCalls::rewriteSetjmpCall(CallInst *Call) {
  // Find the intrinsic function.
  Function *NaClSetjmpFunc = findSetjmpIntrinsic();
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
    Value *Arg = &*FuncArgs++;
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

void RewritePNaClLibraryCalls::populateSetjmpWrapper(Function *SetjmpFunc) {
  populateWrapperCommon(
      /* Func             */ SetjmpFunc,
      /* FuncName         */ "setjmp",
      /* CallRewriter     */ &RewritePNaClLibraryCalls::rewriteSetjmpCall,
      /* CallCannotReturn */ false,
      /* ...              */ "env", NULL);
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

Function *RewritePNaClLibraryCalls::findSetjmpIntrinsic() {
  if (!SetjmpIntrinsic) {
    SetjmpIntrinsic = Intrinsic::getDeclaration(
        TheModule, Intrinsic::nacl_setjmp);
  }
  return SetjmpIntrinsic;
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
