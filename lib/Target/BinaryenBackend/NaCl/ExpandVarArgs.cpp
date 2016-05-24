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
//===----------------------------------------------------------------------===//

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Triple.h"
#include "llvm/IR/Constants.h"
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
class ExpandVarArgs : public ModulePass {
public:
  static char ID;
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

static bool isEmscriptenJSArgsFunc(Module *M, StringRef Name) {
  // TODO(jfb) Make these intrinsics in clang and remove the assert: these
  //           intrinsics should only exist for Emscripten.
  bool isEmscriptenSpecial = Name.equals("emscripten_asm_const_int") ||
                             Name.equals("emscripten_asm_const_double") ||
                             Name.equals("emscripten_landingpad") ||
                             Name.equals("emscripten_resume");
  assert(isEmscriptenSpecial ? Triple(M->getTargetTriple()).isOSEmscripten()
                             : true);
  return isEmscriptenSpecial;
}

static bool ExpandVarArgFunc(Module *M, Function *Func) {
  if (Func->isDeclaration() && Func->use_empty())
    return false; // No point in doing any work.

  if (isEmscriptenJSArgsFunc(M, Func->getName()))
    return false;

  Type *PtrType = Type::getInt8PtrTy(Func->getContext());

  FunctionType *FTy = Func->getFunctionType();
  SmallVector<Type *, 8> Params(FTy->param_begin(), FTy->param_end());
  Params.push_back(PtrType);
  FunctionType *NFTy =
      FunctionType::get(FTy->getReturnType(), Params, /*isVarArg=*/false);
  Function *NewFunc = RecreateFunction(Func, NFTy);

  // Declare the new argument as "noalias".
  NewFunc->setAttributes(Func->getAttributes().addAttribute(
      Func->getContext(), FTy->getNumParams() + 1, Attribute::NoAlias));

  // Move the arguments across to the new function.
  auto NewArg = NewFunc->arg_begin();
  for (Argument &Arg : Func->args()) {
    Arg.replaceAllUsesWith(&*NewArg);
    NewArg->takeName(&Arg);
    ++NewArg;
  }
  // The last argument is the new `i8 * noalias %varargs`.
  NewArg->setName("varargs");

  Func->eraseFromParent();

  // Expand out uses of llvm.va_start in this function.
  for (BasicBlock &BB : *NewFunc) {
    for (auto BI = BB.begin(), BE = BB.end(); BI != BE;) {
      Instruction *I = &*BI++;
      if (auto *VAS = dyn_cast<VAStartInst>(I)) {
        IRBuilder<> IRB(VAS);
        Value *Cast = IRB.CreateBitCast(VAS->getArgList(),
                                        PtrType->getPointerTo(), "arglist");
        IRB.CreateStore(&*NewArg, Cast);
        VAS->eraseFromParent();
      }
    }
  }

  return true;
}

static void ExpandVAArgInst(VAArgInst *Inst, DataLayout *DL) {
  Type *IntPtrTy = DL->getIntPtrType(Inst->getContext());
  auto *One = ConstantInt::get(IntPtrTy, 1);
  IRBuilder<> IRB(Inst);
  auto *ArgList = IRB.CreateBitCast(
      Inst->getPointerOperand(),
      Inst->getType()->getPointerTo()->getPointerTo(), "arglist");

  // The caller spilled all of the va_args onto the stack in an unpacked
  // struct. Each va_arg load from that struct needs to realign the element to
  // its target-appropriate alignment in the struct in order to jump over
  // padding that may have been in-between arguments. Do this with ConstantExpr
  // to ensure good code gets generated, following the same approach as
  // Support/MathExtras.h:alignAddr:
  //   ((uintptr_t)Addr + Alignment - 1) & ~(uintptr_t)(Alignment - 1)
  // This assumes the alignment of the type is a power of 2 (or 1, in which case
  // no realignment occurs).
  auto *Ptr = IRB.CreateLoad(ArgList, "arglist_current");
  auto *AlignOf = ConstantExpr::getIntegerCast(
      ConstantExpr::getAlignOf(Inst->getType()), IntPtrTy, /*isSigned=*/false);
  auto *AlignMinus1 = ConstantExpr::getNUWSub(AlignOf, One);
  auto *NotAlignMinus1 = IRB.CreateNot(AlignMinus1);
  auto *CurrentPtr = IRB.CreateIntToPtr(
      IRB.CreateAnd(
          IRB.CreateNUWAdd(IRB.CreatePtrToInt(Ptr, IntPtrTy), AlignMinus1),
          NotAlignMinus1),
      Ptr->getType());

  auto *Result = IRB.CreateLoad(CurrentPtr, "va_arg");
  Result->takeName(Inst);

  // Update the va_list to point to the next argument.
  Value *Indexes[] = {One};
  auto *Next = IRB.CreateInBoundsGEP(CurrentPtr, Indexes, "arglist_next");
  IRB.CreateStore(Next, ArgList);

  Inst->replaceAllUsesWith(Result);
  Inst->eraseFromParent();
}

static void ExpandVAEnd(VAEndInst *VAE) {
  // va_end() is a no-op in this implementation.
  VAE->eraseFromParent();
}

static void ExpandVACopyInst(VACopyInst *Inst) {
  // va_list may have more space reserved, but we only need to
  // copy a single pointer.
  Type *PtrTy = Type::getInt8PtrTy(Inst->getContext())->getPointerTo();
  IRBuilder<> IRB(Inst);
  auto *Src = IRB.CreateBitCast(Inst->getSrc(), PtrTy, "vacopy_src");
  auto *Dest = IRB.CreateBitCast(Inst->getDest(), PtrTy, "vacopy_dest");
  auto *CurrentPtr = IRB.CreateLoad(Src, "vacopy_currentptr");
  IRB.CreateStore(CurrentPtr, Dest);
  Inst->eraseFromParent();
}

// ExpandVarArgCall() converts a CallInst or InvokeInst to expand out
// of varargs.  It returns whether the module was modified.
template <class InstType>
static bool ExpandVarArgCall(Module *M, InstType *Call, DataLayout *DL) {
  FunctionType *FuncType = cast<FunctionType>(
      Call->getCalledValue()->getType()->getPointerElementType());
  if (!FuncType->isFunctionVarArg())
    return false;
  if (auto *F = dyn_cast<Function>(Call->getCalledValue()))
    if (isEmscriptenJSArgsFunc(M, F->getName()))
      return false;

  Function *F = Call->getParent()->getParent();
  LLVMContext &Ctx = M->getContext();

  SmallVector<AttributeSet, 8> Attrs;
  Attrs.push_back(Call->getAttributes().getFnAttributes());
  Attrs.push_back(Call->getAttributes().getRetAttributes());

  // Split argument list into fixed and variable arguments.
  SmallVector<Value *, 8> FixedArgs;
  SmallVector<Value *, 8> VarArgs;
  SmallVector<Type *, 8> VarArgsTypes;
  for (unsigned I = 0, E = FuncType->getNumParams(); I < E; ++I) {
    FixedArgs.push_back(Call->getArgOperand(I));
    // AttributeSets use 1-based indexing.
    Attrs.push_back(Call->getAttributes().getParamAttributes(I + 1));
  }
  for (unsigned I = FuncType->getNumParams(), E = Call->getNumArgOperands();
       I < E; ++I) {
    Value *ArgVal = Call->getArgOperand(I);
    VarArgs.push_back(ArgVal);
    bool isByVal = Call->getAttributes().hasAttribute(I + 1, Attribute::ByVal);
    // For "byval" arguments we must dereference the pointer.
    VarArgsTypes.push_back(isByVal ? ArgVal->getType()->getPointerElementType()
                                   : ArgVal->getType());
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
    VarArgsTypes.push_back(Type::getInt32Ty(Ctx));
  }

  // Create struct type for packing variable arguments into.
  StructType *VarArgsTy = StructType::get(Ctx, VarArgsTypes);

  // Allocate space for the variable argument buffer.  Do this at the
  // start of the function so that we don't leak space if the function
  // is called in a loop.
  IRBuilder<> IRB(&*F->getEntryBlock().getFirstInsertionPt());
  auto *Buf = IRB.CreateAlloca(VarArgsTy, nullptr, "vararg_buffer");

  // Call llvm.lifetime.start/end intrinsics to indicate that Buf is
  // only used for the duration of the function call, so that the
  // stack space can be reused elsewhere.
  auto LifetimeStart = Intrinsic::getDeclaration(M, Intrinsic::lifetime_start);
  auto LifetimeEnd = Intrinsic::getDeclaration(M, Intrinsic::lifetime_end);
  auto *I8Ptr = Type::getInt8Ty(Ctx)->getPointerTo();
  auto *BufPtr = IRB.CreateBitCast(Buf, I8Ptr, "vararg_lifetime_bitcast");
  auto *BufSize =
      ConstantInt::get(Ctx, APInt(64, DL->getTypeAllocSize(VarArgsTy)));
  IRB.CreateCall(LifetimeStart, {BufSize, BufPtr});

  // Copy variable arguments into buffer.
  int Index = 0;
  IRB.SetInsertPoint(Call);
  for (Value *Arg : VarArgs) {
    Value *Indexes[] = {ConstantInt::get(Ctx, APInt(32, 0)),
                        ConstantInt::get(Ctx, APInt(32, Index))};
    Value *Ptr = IRB.CreateInBoundsGEP(Buf, Indexes, "vararg_ptr");
    bool isByVal = Call->getAttributes().hasAttribute(
        FuncType->getNumParams() + Index + 1, Attribute::ByVal);
    if (isByVal)
      IRB.CreateMemCpy(Ptr, Arg, DL->getTypeAllocSize(
                                     Arg->getType()->getPointerElementType()),
                       /*Align=*/1);
    else
      IRB.CreateStore(Arg, Ptr);
    ++Index;
  }

  // Cast function to new type to add our extra pointer argument.
  SmallVector<Type *, 8> ArgTypes(FuncType->param_begin(),
                                  FuncType->param_end());
  ArgTypes.push_back(VarArgsTy->getPointerTo());
  FunctionType *NFTy = FunctionType::get(FuncType->getReturnType(), ArgTypes,
                                         /*isVarArg=*/false);
  Value *CastFunc = IRB.CreateBitCast(Call->getCalledValue(),
                                      NFTy->getPointerTo(), "vararg_func");

  // Create the converted function call.
  FixedArgs.push_back(Buf);
  Instruction *NewCall;
  if (auto *C = dyn_cast<CallInst>(Call)) {
    auto *N = IRB.CreateCall(CastFunc, FixedArgs);
    N->setAttributes(AttributeSet::get(Ctx, Attrs));
    NewCall = N;
    IRB.CreateCall(LifetimeEnd, {BufSize, BufPtr});
  } else if (auto *C = dyn_cast<InvokeInst>(Call)) {
    auto *N = IRB.CreateInvoke(CastFunc, C->getNormalDest(), C->getUnwindDest(),
                               FixedArgs, C->getName());
    N->setAttributes(AttributeSet::get(Ctx, Attrs));
    (IRBuilder<>(&*C->getNormalDest()->getFirstInsertionPt()))
        .CreateCall(LifetimeEnd, {BufSize, BufPtr});
    (IRBuilder<>(&*C->getUnwindDest()->getFirstInsertionPt()))
        .CreateCall(LifetimeEnd, {BufSize, BufPtr});
    NewCall = N;
  } else {
    llvm_unreachable("not a call/invoke");
  }

  NewCall->takeName(Call);
  Call->replaceAllUsesWith(NewCall);
  Call->eraseFromParent();

  return true;
}

bool ExpandVarArgs::runOnModule(Module &M) {
  bool Changed = false;
  DataLayout DL(&M);

  for (auto MI = M.begin(), ME = M.end(); MI != ME;) {
    Function *F = &*MI++;
    for (BasicBlock &BB : *F) {
      for (auto BI = BB.begin(), BE = BB.end(); BI != BE;) {
        Instruction *I = &*BI++;
        if (auto *VI = dyn_cast<VAArgInst>(I)) {
          Changed = true;
          ExpandVAArgInst(VI, &DL);
        } else if (auto *VAE = dyn_cast<VAEndInst>(I)) {
          Changed = true;
          ExpandVAEnd(VAE);
        } else if (auto *VAC = dyn_cast<VACopyInst>(I)) {
          Changed = true;
          ExpandVACopyInst(VAC);
        } else if (auto *Call = dyn_cast<CallInst>(I)) {
          Changed |= ExpandVarArgCall(&M, Call, &DL);
        } else if (auto *Call = dyn_cast<InvokeInst>(I)) {
          Changed |= ExpandVarArgCall(&M, Call, &DL);
        }
      }
    }

    if (F->isVarArg())
      Changed |= ExpandVarArgFunc(&M, F);
  }

  return Changed;
}

ModulePass *llvm::createExpandVarArgsPass() { return new ExpandVarArgs(); }
