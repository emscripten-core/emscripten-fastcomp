//===- ExpandVarArgs.cpp - Expand out variable argument function calls-----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass expands out all use of variable argument functions.
//
// This pass replaces a varargs function call with a function call in
// which a pointer to the variable arguments is passed explicitly.
// The callee explicitly allocates space for the variable arguments on
// the stack using "alloca".
//
// Alignment:
//
// This pass does not add any alignment padding between the arguments
// that are copied onto the stack.  We assume that the only argument
// types that need to be handled are 32-bit and 64-bit -- i32, i64,
// pointers and double:
//
//  * We won't see i1, i8, i16 and float as varargs arguments because
//    the C standard requires the compiler to promote these to the
//    types "int" and "double".
//
//  * We won't see va_arg instructions of struct type because Clang
//    does not yet support them in PNaCl mode.  See
//    https://code.google.com/p/nativeclient/issues/detail?id=2381
//
// If such arguments do appear in the input, this pass will generate
// correct, working code, but this code might be inefficient due to
// using unaligned memory accesses.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

namespace {
  // This is a ModulePass because the pass recreates functions in
  // order to change their argument lists.
  class ExpandVarArgs : public ModulePass {
  public:
    static char ID; // Pass identification, replacement for typeid
    ExpandVarArgs() : ModulePass(ID) {
      initializeExpandVarArgsPass(*PassRegistry::getPassRegistry());
    }

    virtual bool runOnModule(Module &M);
  };
}

char ExpandVarArgs::ID = 0;
INITIALIZE_PASS(ExpandVarArgs, "expand-varargs",
                "Expand out variable argument function definitions and calls",
                false, false)

static bool isEmscriptenJSArgsFunc(StringRef Name) {
  return Name.equals("emscripten_asm_const_int") ||
         Name.equals("emscripten_asm_const_double") ||
         Name.equals("emscripten_landingpad") ||
         Name.equals("emscripten_resume");
}

static void ExpandVarArgFunc(Function *Func) {
  Type *PtrType = Type::getInt8PtrTy(Func->getContext());

  FunctionType *FTy = Func->getFunctionType();
  SmallVector<Type *, 8> Params(FTy->param_begin(), FTy->param_end());
  Params.push_back(PtrType);
  FunctionType *NFTy = FunctionType::get(FTy->getReturnType(), Params, false);
  Function *NewFunc = RecreateFunction(Func, NFTy);

  // Declare the new argument as "noalias".
  NewFunc->setAttributes(
      Func->getAttributes().addAttribute(
          Func->getContext(), FTy->getNumParams() + 1, Attribute::NoAlias));

  // Move the arguments across to the new function.
  for (Function::arg_iterator Arg = Func->arg_begin(), E = Func->arg_end(),
         NewArg = NewFunc->arg_begin();
       Arg != E; ++Arg, ++NewArg) {
    Arg->replaceAllUsesWith(NewArg);
    NewArg->takeName(Arg);
  }

  Func->eraseFromParent();

  Value *VarArgsArg = --NewFunc->arg_end();
  VarArgsArg->setName("varargs");

  // Expand out uses of llvm.va_start in this function.
  for (Function::iterator BB = NewFunc->begin(), E = NewFunc->end();
       BB != E;
       ++BB) {
    for (BasicBlock::iterator Iter = BB->begin(), E = BB->end();
         Iter != E; ) {
      Instruction *Inst = Iter++;
      if (VAStartInst *VAS = dyn_cast<VAStartInst>(Inst)) {
        Value *Cast = CopyDebug(new BitCastInst(VAS->getArgList(),
                                                PtrType->getPointerTo(),
                                                "arglist", VAS), VAS);
        CopyDebug(new StoreInst(VarArgsArg, Cast, VAS), VAS);
        VAS->eraseFromParent();
      }
    }
  }
}

static void ExpandVAArgInst(VAArgInst *Inst) {
  // Read the argument.  We assume that no realignment of the pointer
  // is required.
  Value *ArgList = CopyDebug(new BitCastInst(
      Inst->getPointerOperand(),
      Inst->getType()->getPointerTo()->getPointerTo(), "arglist", Inst), Inst);
  Value *CurrentPtr = CopyDebug(new LoadInst(ArgList, "arglist_current", Inst),
                                Inst);
  Value *Result = CopyDebug(new LoadInst(CurrentPtr, "va_arg", Inst), Inst);
  Result->takeName(Inst);

  // Update the va_list to point to the next argument.
  SmallVector<Value *, 1> Indexes;
  Indexes.push_back(ConstantInt::get(Inst->getContext(), APInt(32, 1)));
  Value *Next = CopyDebug(GetElementPtrInst::Create(
                              CurrentPtr, Indexes, "arglist_next", Inst), Inst);
  CopyDebug(new StoreInst(Next, ArgList, Inst), Inst);

  Inst->replaceAllUsesWith(Result);
  Inst->eraseFromParent();
}

static void ExpandVACopyInst(VACopyInst *Inst) {
  // va_list may have more space reserved, but we only need to
  // copy a single pointer.
  Type *PtrTy = Type::getInt8PtrTy(Inst->getContext())->getPointerTo();
  Value *Src = CopyDebug(new BitCastInst(Inst->getSrc(), PtrTy, "vacopy_src",
                                         Inst), Inst);
  Value *Dest = CopyDebug(new BitCastInst(Inst->getDest(), PtrTy, "vacopy_dest",
                                          Inst), Inst);
  Value *CurrentPtr = CopyDebug(new LoadInst(Src, "vacopy_currentptr", Inst),
                                Inst);
  CopyDebug(new StoreInst(CurrentPtr, Dest, Inst), Inst);
  Inst->eraseFromParent();
}

static void LifetimeDecl(Intrinsic::ID id, Value *Ptr, Value *Size,
                         Instruction *InsertPt) {
  Module *M = InsertPt->getParent()->getParent()->getParent();
  Value *Func = Intrinsic::getDeclaration(M, id);
  SmallVector<Value *, 2> Args;
  Args.push_back(Size);
  Args.push_back(Ptr);
  CallInst::Create(Func, Args, "", InsertPt);
}

// CopyCall() uses argument overloading so that it can be used by the
// template ExpandVarArgCall().
static CallInst *CopyCall(CallInst *Original, Value *Callee,
                          ArrayRef<Value*> Args) {
  return CallInst::Create(Callee, Args, "", Original);
}

static InvokeInst *CopyCall(InvokeInst *Original, Value *Callee,
                            ArrayRef<Value*> Args) {
  return InvokeInst::Create(Callee, Original->getNormalDest(),
                            Original->getUnwindDest(), Args, "", Original);
}

// ExpandVarArgCall() converts a CallInst or InvokeInst to expand out
// of varargs.  It returns whether the module was modified.
template <class InstType>
static bool ExpandVarArgCall(InstType *Call, DataLayout *DL) {
  FunctionType *FuncType = cast<FunctionType>(
      Call->getCalledValue()->getType()->getPointerElementType());
  if (!FuncType->isFunctionVarArg())
    return false;


  // EMSCRIPTEN: use js varargs for special instrinsics
  const Value *CV = Call->getCalledValue();
  if (isa<Function>(CV) && isEmscriptenJSArgsFunc(CV->getName())) {
    return false;
  }

  LLVMContext *Context = &Call->getContext();

  SmallVector<AttributeSet, 8> Attrs;
  Attrs.push_back(Call->getAttributes().getFnAttributes());
  Attrs.push_back(Call->getAttributes().getRetAttributes());

  // Split argument list into fixed and variable arguments.
  SmallVector<Value *, 8> FixedArgs;
  SmallVector<Value *, 8> VarArgs;
  SmallVector<Type *, 8> VarArgsTypes;
  for (unsigned I = 0; I < FuncType->getNumParams(); ++I) {
    FixedArgs.push_back(Call->getArgOperand(I));
    // AttributeSets use 1-based indexing.
    Attrs.push_back(Call->getAttributes().getParamAttributes(I + 1));
  }
  for (unsigned I = FuncType->getNumParams();
       I < Call->getNumArgOperands(); ++I) {
    Value *ArgVal = Call->getArgOperand(I);
    VarArgs.push_back(ArgVal);
    if (Call->getAttributes().hasAttribute(I + 1, Attribute::ByVal)) {
      // For "byval" arguments we must dereference the pointer.
      VarArgsTypes.push_back(ArgVal->getType()->getPointerElementType());
    } else {
      VarArgsTypes.push_back(ArgVal->getType());
    }
  }
  if (VarArgsTypes.size() == 0) {
    // Some buggy code (e.g. 176.gcc in Spec2k) uses va_arg on an
    // empty argument list, which gives undefined behaviour in C.  To
    // work around such programs, we create a dummy varargs buffer on
    // the stack even though there are no arguments to put in it.
    // This allows va_arg to read an undefined value from the stack
    // rather than crashing by reading from an uninitialized pointer.
    // An alternative would be to pass a null pointer to catch the
    // invalid use of va_arg.
    VarArgsTypes.push_back(Type::getInt32Ty(*Context));
  }

  // Create struct type for packing variable arguments into.  We
  // create this as packed for now and assume that no alignment
  // padding is desired.
  StructType *VarArgsTy = StructType::get(*Context, VarArgsTypes, true);

  // Allocate space for the variable argument buffer.  Do this at the
  // start of the function so that we don't leak space if the function
  // is called in a loop.
  Function *Func = Call->getParent()->getParent();
  Instruction *Buf = new AllocaInst(VarArgsTy, "vararg_buffer");
  Func->getEntryBlock().getInstList().push_front(Buf);

  // Call llvm.lifetime.start/end intrinsics to indicate that Buf is
  // only used for the duration of the function call, so that the
  // stack space can be reused elsewhere.
  Type *I8Ptr = Type::getInt8Ty(*Context)->getPointerTo();
  Instruction *BufPtr = new BitCastInst(Buf, I8Ptr, "vararg_lifetime_bitcast");
  BufPtr->insertAfter(Buf);
  Value *BufSize = ConstantInt::get(*Context,
                                    APInt(64, DL->getTypeAllocSize(VarArgsTy)));
  LifetimeDecl(Intrinsic::lifetime_start, BufPtr, BufSize, Call);

  // Copy variable arguments into buffer.
  int Index = 0;
  for (SmallVector<Value *, 8>::iterator Iter = VarArgs.begin();
       Iter != VarArgs.end();
       ++Iter, ++Index) {
    SmallVector<Value *, 2> Indexes;
    Indexes.push_back(ConstantInt::get(*Context, APInt(32, 0)));
    Indexes.push_back(ConstantInt::get(*Context, APInt(32, Index)));
    Value *Ptr = CopyDebug(GetElementPtrInst::Create(
                               Buf, Indexes, "vararg_ptr", Call), Call);
    if (Call->getAttributes().hasAttribute(
            FuncType->getNumParams() + Index + 1, Attribute::ByVal)) {
      IRBuilder<> Builder(Call);
      Builder.CreateMemCpy(
          Ptr, *Iter,
          DL->getTypeAllocSize((*Iter)->getType()->getPointerElementType()),
          /* Align= */ 1);
    } else {
      StoreInst *S = new StoreInst(*Iter, Ptr, Call);
      CopyDebug(S, Call);
      S->setAlignment(4); // EMSCRIPTEN: pnacl stack is only 4-byte aligned
    }
  }

  // Cast function to new type to add our extra pointer argument.
  SmallVector<Type *, 8> ArgTypes(FuncType->param_begin(),
                                  FuncType->param_end());
  ArgTypes.push_back(VarArgsTy->getPointerTo());
  FunctionType *NFTy = FunctionType::get(FuncType->getReturnType(),
                                         ArgTypes, false);
  /// XXX EMSCRIPTEN: Handle Constants as well as Instructions, since we
  /// don't run the ConstantExpr lowering pass.
  Value *CastFunc;
  if (Constant *C = dyn_cast<Constant>(Call->getCalledValue()))
    CastFunc = ConstantExpr::getBitCast(C, NFTy->getPointerTo());
  else
    CastFunc = CopyDebug(new BitCastInst(Call->getCalledValue(), NFTy->getPointerTo(),
                                         "vararg_func", Call), Call);

  // Create the converted function call.
  FixedArgs.push_back(Buf);
  InstType *NewCall = CopyCall(Call, CastFunc, FixedArgs);
  CopyDebug(NewCall, Call);
  NewCall->setAttributes(AttributeSet::get(Call->getContext(), Attrs));
  NewCall->takeName(Call);

  if (isa<CallInst>(Call)) {
    LifetimeDecl(Intrinsic::lifetime_end, BufPtr, BufSize, Call);
  } else if (InvokeInst *Invoke = dyn_cast<InvokeInst>(Call)) {
    LifetimeDecl(Intrinsic::lifetime_end, BufPtr, BufSize,
                 Invoke->getNormalDest()->getFirstInsertionPt());
    LifetimeDecl(Intrinsic::lifetime_end, BufPtr, BufSize,
                 Invoke->getUnwindDest()->getFirstInsertionPt());
  }

  Call->replaceAllUsesWith(NewCall);
  Call->eraseFromParent();

  return true;
}

bool ExpandVarArgs::runOnModule(Module &M) {
  bool Changed = false;
  DataLayout DL(&M);

  for (Module::iterator Iter = M.begin(), E = M.end(); Iter != E; ) {
    Function *Func = Iter++;

    for (Function::iterator BB = Func->begin(), E = Func->end();
         BB != E;
         ++BB) {
      for (BasicBlock::iterator Iter = BB->begin(), E = BB->end();
           Iter != E; ) {
        Instruction *Inst = Iter++;
        if (VAArgInst *VI = dyn_cast<VAArgInst>(Inst)) {
          Changed = true;
          ExpandVAArgInst(VI);
        } else if (isa<VAEndInst>(Inst)) {
          // va_end() is a no-op in this implementation.
          Changed = true;
          Inst->eraseFromParent();
        } else if (VACopyInst *VAC = dyn_cast<VACopyInst>(Inst)) {
          Changed = true;
          ExpandVACopyInst(VAC);
        } else if (CallInst *Call = dyn_cast<CallInst>(Inst)) {
          Changed |= ExpandVarArgCall(Call, &DL);
        } else if (InvokeInst *Call = dyn_cast<InvokeInst>(Inst)) {
          Changed |= ExpandVarArgCall(Call, &DL);
        }
      }
    }

    // EMSCRIPTEN: use js varargs for special instrinsics
    if (Func->isVarArg() && !isEmscriptenJSArgsFunc(Func->getName())) {
      Changed = true;
      ExpandVarArgFunc(Func);
    }
  }

  return Changed;
}

ModulePass *llvm::createExpandVarArgsPass() {
  return new ExpandVarArgs();
}
