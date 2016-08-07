//===- SimplifyStructRegSignatures.cpp - struct regs to struct pointers----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass replaces function signatures exposing struct registers
// to byval pointer-based signatures.
//
// There are 2 types of signatures that are thus changed:
//
// @foo(%some_struct %val) -> @foo(%some_struct* byval %val)
//      and
// %someStruct @bar(<other_args>) -> void @bar(%someStruct* sret, <other_args>)
//
// Such function types may appear in other type declarations, for example:
//
// %a_struct = type { void (%some_struct)*, i32 }
//
// We map such types to corresponding types, mapping the function types
// appropriately:
//
// %a_struct.0 = type { void (%some_struct*)*, i32 }
//===----------------------------------------------------------------------===//
#include "SimplifiedFuncTypeMap.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/ilist.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Twine.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Use.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/PassInfo.h"
#include "llvm/PassRegistry.h"
#include "llvm/PassSupport.h"
#include "llvm/Transforms/NaCl.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <cassert>
#include <cstddef>
using namespace llvm;
namespace {
static const unsigned int TypicalFuncArity = 8;
static bool shouldPromote(const Type *Ty) {
  return Ty->isAggregateType();
}
// Utility class. For any given type, get the associated type that is free of
// struct register arguments.
class TypeMapper : public SimplifiedFuncTypeMap {
protected:
  MappingResult getSimpleFuncType(LLVMContext &Ctx, StructMap &Tentatives,
                                  FunctionType *OldFnTy) override {
    Type *OldRetType = OldFnTy->getReturnType();
    Type *NewRetType = OldRetType;
    Type *Void = Type::getVoidTy(Ctx);
    ParamTypeVector NewArgs;
    bool Changed = false;
    // Struct register returns become the first parameter of the new FT.
    // The new FT has void for the return type
    if (shouldPromote(OldRetType)) {
      NewRetType = Void;
      Changed = true;
      NewArgs.push_back(getSimpleArgumentType(Ctx, OldRetType, Tentatives));
    }
    for (auto OldParam : OldFnTy->params()) {
      auto NewType = getSimpleArgumentType(Ctx, OldParam, Tentatives);
      Changed |= NewType.isChanged();
      NewArgs.push_back(NewType);
    }
    Type *NewFuncType =
        FunctionType::get(NewRetType, NewArgs, OldFnTy->isVarArg());
    return {NewFuncType, Changed};
  }
private:
  // Get the simplified type of a function argument.
  MappingResult getSimpleArgumentType(LLVMContext &Ctx, Type *Ty,
                                      StructMap &Tentatives) {
    // struct registers become pointers to simple structs
    if (shouldPromote(Ty)) {
      return {PointerType::get(
                  getSimpleAggregateTypeInternal(Ctx, Ty, Tentatives), 0),
              true};
    }
    return getSimpleAggregateTypeInternal(Ctx, Ty, Tentatives);
  }
};
// This is a ModulePass because the pass recreates functions in
// order to change their signatures.
class SimplifyStructRegSignatures : public ModulePass {
public:
  static char ID;
  SimplifyStructRegSignatures() : ModulePass(ID) {
    initializeSimplifyStructRegSignaturesPass(*PassRegistry::getPassRegistry());
  }
  virtual bool runOnModule(Module &M);
private:
  TypeMapper Mapper;
  DenseSet<Function *> FunctionsToDelete;
  SetVector<CallInst *> CallsToPatch;
  SetVector<InvokeInst *> InvokesToPatch;
  DenseMap<Function *, Function *> FunctionMap;

  struct FunctionAddressing {
    Value *Temp;
    Function *Old;
    FunctionAddressing(Value *Temp, Function *Old) : Temp(Temp), Old(Old) {}
  };
  std::vector<FunctionAddressing> FunctionAddressings;

  bool
  simplifyFunction(LLVMContext &Ctx, Function *OldFunc);
  void scheduleInstructionsForCleanup(Function *NewFunc);
  template <class TCall>
  void fixCallSite(LLVMContext &Ctx, TCall *Call, unsigned PreferredAlignment);
  void fixFunctionBody(LLVMContext &Ctx, Function *OldFunc, Function *NewFunc);
  template <class TCall>
  TCall *fixCallTargetAndArguments(LLVMContext &Ctx, IRBuilder<> &Builder,
                                   TCall *OldCall, Value *NewTarget,
                                   FunctionType *NewType,
                                   BasicBlock::iterator AllocaInsPoint,
                                   Value *ExtraArg = nullptr);
  void checkNoUnsupportedInstructions(LLVMContext &Ctx, Function *Fct);
};
}
char SimplifyStructRegSignatures::ID = 0;
INITIALIZE_PASS(
    SimplifyStructRegSignatures, "simplify-struct-reg-signatures",
    "Simplify function signatures by removing struct register parameters",
    false, false)
// Update the arg names for a newly created function.
static void UpdateArgNames(Function *OldFunc, Function *NewFunc) {
  auto NewArgIter = NewFunc->arg_begin();
  if (shouldPromote(OldFunc->getReturnType())) {
    NewArgIter->setName("retVal");
    NewArgIter++;
  }
  for (const Argument &OldArg : OldFunc->args()) {
    Argument *NewArg = &*NewArgIter++;
    NewArg->setName(OldArg.getName() +
                    (shouldPromote(OldArg.getType()) ? ".ptr" : ""));
  }
}
// Replace all uses of an old value with a new one, disregarding the type. We
// correct the types after we wire the new parameters in, in fixFunctionBody.
static void BlindReplace(Value *Old, Value *New) {
  for (auto UseIter = Old->use_begin(), E = Old->use_end(); E != UseIter;) {
    Use &AUse = *(UseIter++);
    AUse.set(New);
  }
}
// Adapt the body of a function for the new arguments.
static void ConvertArgumentValue(Value *Old, Value *New, Instruction *InsPoint,
                                 const bool IsAggregateToPtr) {
  if (Old == New)
    return;
  if (Old->getType() == New->getType()) {
    Old->replaceAllUsesWith(New);
    New->takeName(Old);
    return;
  }
  BlindReplace(Old, (IsAggregateToPtr
                         ? new LoadInst(New, Old->getName() + ".sreg", InsPoint)
                         : New));
}
// Fix returns. Return true if fixes were needed.
static void FixReturn(Function *OldFunc, Function *NewFunc) {
  Argument *FirstNewArg = &*NewFunc->getArgumentList().begin();
  for (auto BIter = NewFunc->begin(), LastBlock = NewFunc->end();
       LastBlock != BIter;) {
    BasicBlock *BB = &*BIter++;
    for (auto IIter = BB->begin(), LastI = BB->end(); LastI != IIter;) {
      Instruction *Instr = &*IIter++;
      if (ReturnInst *Ret = dyn_cast<ReturnInst>(Instr)) {
        auto RetVal = Ret->getReturnValue();
        IRBuilder<> Builder(Ret);
        StoreInst *Store = Builder.CreateStore(RetVal, FirstNewArg);
        Store->setAlignment(FirstNewArg->getParamAlignment());
        Builder.CreateRetVoid();
        Ret->eraseFromParent();
      }
    }
  }
}
/// In the next two functions, `RetIndex` is the index of the possibly promoted
/// return.
/// Ie if the return is promoted, `RetIndex` should be `1`, else `0`.
static AttributeSet CopyRetAttributes(LLVMContext &C, const DataLayout &DL,
                                      const AttributeSet From, Type *RetTy,
                                      const unsigned RetIndex) {
  AttributeSet NewAttrs;
  if (RetIndex != 0) {
    NewAttrs = NewAttrs.addAttribute(C, RetIndex, Attribute::StructRet);
    NewAttrs = NewAttrs.addAttribute(C, RetIndex, Attribute::NonNull);
    NewAttrs = NewAttrs.addAttribute(C, RetIndex, Attribute::NoCapture);
    if (RetTy->isSized()) {
      NewAttrs = NewAttrs.addDereferenceableAttr(C, RetIndex,
                                                 DL.getTypeAllocSize(RetTy));
    }
  } else {
    NewAttrs = NewAttrs.addAttributes(C, RetIndex, From.getRetAttributes());
  }
  auto FnAttrs = From.getFnAttributes();
  if (RetIndex != 0) {
    FnAttrs = FnAttrs.removeAttribute(C, AttributeSet::FunctionIndex,
                                      Attribute::ReadOnly);
    FnAttrs = FnAttrs.removeAttribute(C, AttributeSet::FunctionIndex,
                                      Attribute::ReadNone);
  }
  NewAttrs = NewAttrs.addAttributes(C, AttributeSet::FunctionIndex, FnAttrs);
  return NewAttrs;
}
/// Iff the argument in question was promoted, `NewArgTy` should be non-null.
static AttributeSet CopyArgAttributes(AttributeSet NewAttrs, LLVMContext &C,
                                      const DataLayout &DL,
                                      const AttributeSet From,
                                      const unsigned OldArg, Type *NewArgTy,
                                      const unsigned RetIndex) {
  const unsigned NewIndex = RetIndex + OldArg + 1;
  if (!NewArgTy) {
    const unsigned OldIndex = OldArg + 1;
    auto OldAttrs = From.getParamAttributes(OldIndex);
    if (OldAttrs.getNumSlots() == 0) {
      return NewAttrs;
    }
    // move the params to the new index position:
    unsigned OldSlot = 0;
    for (; OldSlot < OldAttrs.getNumSlots(); ++OldSlot) {
      if (OldAttrs.getSlotIndex(OldSlot) == OldIndex) {
        break;
      }
    }
    assert(OldSlot != OldAttrs.getNumSlots());
    AttrBuilder B(AttributeSet(), NewIndex);
    for (auto II = OldAttrs.begin(OldSlot), IE = OldAttrs.end(OldSlot);
         II != IE; ++II) {
      B.addAttribute(*II);
    }
    auto Attrs = AttributeSet::get(C, NewIndex, B);
    NewAttrs = NewAttrs.addAttributes(C, NewIndex, Attrs);
    return NewAttrs;
  } else {
    NewAttrs = NewAttrs.addAttribute(C, NewIndex, Attribute::NonNull);
    NewAttrs = NewAttrs.addAttribute(C, NewIndex, Attribute::NoCapture);
    NewAttrs = NewAttrs.addAttribute(C, NewIndex, Attribute::ReadOnly);
    if (NewArgTy->isSized()) {
      NewAttrs = NewAttrs.addDereferenceableAttr(C, NewIndex,
                                                 DL.getTypeAllocSize(NewArgTy));
    }
    return NewAttrs;
  }
}
// TODO (mtrofin): is this comprehensive?
template <class TCall>
void CopyCallAttributesAndMetadata(TCall *Orig, TCall *NewCall) {
  NewCall->setCallingConv(Orig->getCallingConv());
  NewCall->setAttributes(NewCall->getAttributes().addAttributes(
      Orig->getContext(), AttributeSet::FunctionIndex,
      Orig->getAttributes().getFnAttributes()));
  NewCall->takeName(Orig);
}
static InvokeInst *CreateCallFrom(InvokeInst *Orig, Value *Target,
                                  ArrayRef<Value *> &Args,
                                  IRBuilder<> &Builder) {
  auto Ret = Builder.CreateInvoke(Target, Orig->getNormalDest(),
                                  Orig->getUnwindDest(), Args);
  CopyCallAttributesAndMetadata(Orig, Ret);
  return Ret;
}
static CallInst *CreateCallFrom(CallInst *Orig, Value *Target,
                                ArrayRef<Value *> &Args, IRBuilder<> &Builder) {
  CallInst *Ret = Builder.CreateCall(Target, Args);
  Ret->setTailCallKind(Orig->getTailCallKind());
  CopyCallAttributesAndMetadata(Orig, Ret);
  return Ret;
}
// Insert Alloca at a specified location (normally, beginning of function)
// to avoid memory leaks if reason for inserting the Alloca
// (typically a call/invoke) is in a loop.
static AllocaInst *InsertAllocaAtLocation(IRBuilder<> &Builder,
                                          BasicBlock::iterator &AllocaInsPoint,
                                          Type *ValType) {
  auto SavedInsPoint = Builder.GetInsertPoint();
  Builder.SetInsertPoint(&*AllocaInsPoint);
  auto *Alloca = Builder.CreateAlloca(ValType);
  AllocaInsPoint = Builder.GetInsertPoint();
  Builder.SetInsertPoint(&*SavedInsPoint);
  return Alloca;
}
// Fix a call site by handing return type changes and/or parameter type and
// attribute changes.
template <class TCall>
void SimplifyStructRegSignatures::fixCallSite(LLVMContext &Ctx, TCall *OldCall,
                                              unsigned PreferredAlignment) {
  Value *NewTarget = OldCall->getCalledValue();
  bool IsTargetFunction = false;
  if (Function *CalledFunc = dyn_cast<Function>(NewTarget)) {
    NewTarget = this->FunctionMap[CalledFunc];
    IsTargetFunction = true;
  }
  assert(NewTarget);
  auto *NewType = cast<FunctionType>(
      Mapper.getSimpleType(Ctx, NewTarget->getType())->getPointerElementType());
  IRBuilder<> Builder(OldCall);
  if (!IsTargetFunction) {
    NewTarget = Builder.CreateBitCast(NewTarget, NewType->getPointerTo());
  }
  auto *OldRetType = OldCall->getType();
  const bool IsSRet =
      !OldCall->getType()->isVoidTy() && NewType->getReturnType()->isVoidTy();
  auto AllocaInsPoint =
      OldCall->getParent()->getParent()->getEntryBlock().getFirstInsertionPt();
  if (IsSRet) {
    auto *Alloca = InsertAllocaAtLocation(Builder, AllocaInsPoint, OldRetType);
    Alloca->takeName(OldCall);
    Alloca->setAlignment(PreferredAlignment);
    auto *NewCall = fixCallTargetAndArguments(Ctx, Builder, OldCall, NewTarget,
                                              NewType, AllocaInsPoint, Alloca);
    assert(NewCall);
    if (auto *Invoke = dyn_cast<InvokeInst>(OldCall))
      Builder.SetInsertPoint(&*Invoke->getNormalDest()->getFirstInsertionPt());
    auto *Load = Builder.CreateLoad(Alloca, Alloca->getName() + ".sreg");
    Load->setAlignment(Alloca->getAlignment());
    OldCall->replaceAllUsesWith(Load);
  } else {
    auto *NewCall = fixCallTargetAndArguments(Ctx, Builder, OldCall, NewTarget,
                                              NewType, AllocaInsPoint);
    OldCall->replaceAllUsesWith(NewCall);
  }
  OldCall->eraseFromParent();
}
template <class TCall>
TCall *SimplifyStructRegSignatures::fixCallTargetAndArguments(
    LLVMContext &Ctx, IRBuilder<> &Builder, TCall *OldCall, Value *NewTarget,
    FunctionType *NewType, BasicBlock::iterator AllocaInsPoint,
    Value *ExtraArg) {
  SmallVector<Value *, TypicalFuncArity> NewArgs;
  const DataLayout &DL = OldCall->getParent() // BB
                             ->getParent()    // F
                             ->getParent()    // M
                             ->getDataLayout();
  const AttributeSet OldSet = OldCall->getAttributes();
  unsigned argOffset = ExtraArg ? 1 : 0;
  const unsigned RetSlot = AttributeSet::ReturnIndex + argOffset;
  if (ExtraArg)
    NewArgs.push_back(ExtraArg);
  AttributeSet NewSet =
      CopyRetAttributes(Ctx, DL, OldSet, OldCall->getType(), RetSlot);
  // Go over the argument list used in the call/invoke, in order to
  // correctly deal with varargs scenarios.
  unsigned NumActualParams = OldCall->getNumArgOperands();
  unsigned VarargMark = NewType->getNumParams();
  for (unsigned ArgPos = 0; ArgPos < NumActualParams; ArgPos++) {
    Use &OldArgUse = OldCall->getOperandUse(ArgPos);
    Value *OldArg = OldArgUse;
    Type *OldArgType = OldArg->getType();
    unsigned NewArgPos = OldArgUse.getOperandNo() + argOffset;
    Type *NewArgType = NewArgPos < VarargMark ? NewType->getFunctionParamType(NewArgPos) : nullptr;
    Type *InnerNewArgType = nullptr;
    if (OldArgType != NewArgType && shouldPromote(OldArgType)) {
      if (NewArgPos >= VarargMark) {
        errs() << *OldCall << '\n';
        report_fatal_error("Aggregate register vararg is not supported");
      }
      auto *Alloca =
          InsertAllocaAtLocation(Builder, AllocaInsPoint, OldArgType);
      Alloca->setName(OldArg->getName() + ".ptr");
      Builder.CreateStore(OldArg, Alloca);
      NewArgs.push_back(Alloca);
      InnerNewArgType = NewArgType->getPointerElementType();
    } else if (NewArgType && OldArgType != NewArgType && isa<Function>(OldArg)) {
      // If a function pointer has a changed type due to struct reg changes, it will still have
      // the wrong type here, since we may have not changed that method yet. We'll fix it up
      // later, and meanwhile place an undef of the right type in that slot.
      Value *Temp = UndefValue::get(NewArgType);
      FunctionAddressings.emplace_back(Temp, cast<Function>(OldArg));
      NewArgs.push_back(Temp);
    } else if (NewArgType && OldArgType != NewArgType && OldArgType->isPointerTy()) {
      // This would be a function ptr or would have a function type nested in
      // it.
      NewArgs.push_back(Builder.CreatePointerCast(OldArg, NewArgType));
    } else {
      NewArgs.push_back(OldArg);
    }
    NewSet = CopyArgAttributes(NewSet, Ctx, DL, OldSet, ArgPos, InnerNewArgType,
                               RetSlot);
  }

  if (isa<Instruction>(NewTarget)) {
    Type* NewPointerType = PointerType::get(NewType, 0);
    if (NewPointerType != OldCall->getType()) {
      // This is a function pointer, and it has the wrong type after our
      // changes. Bitcast it.
      NewTarget = Builder.CreateBitCast(NewTarget, NewPointerType, ".casttarget");
    }
  }

  ArrayRef<Value *> ArrRef = NewArgs;
  TCall *NewCall = CreateCallFrom(OldCall, NewTarget, ArrRef, Builder);
  NewCall->setAttributes(NewSet);
  return NewCall;
}
void
SimplifyStructRegSignatures::scheduleInstructionsForCleanup(Function *NewFunc) {
  for (auto &BBIter : NewFunc->getBasicBlockList()) {
    for (auto &IIter : BBIter.getInstList()) {
      if (CallInst *Call = dyn_cast<CallInst>(&IIter)) {
        if (Function *F = dyn_cast<Function>(Call->getCalledValue())) {
          if (F->isIntrinsic()) {
            continue;
          }
        }
        CallsToPatch.insert(Call);
      } else if (InvokeInst *Invoke = dyn_cast<InvokeInst>(&IIter)) {
        InvokesToPatch.insert(Invoke);
      }
    }
  }
}
// Change function body in the light of type changes.
void SimplifyStructRegSignatures::fixFunctionBody(LLVMContext &Ctx,
                                                  Function *OldFunc,
                                                  Function *NewFunc) {
  const DataLayout &DL = OldFunc->getParent()->getDataLayout();
  bool returnWasFixed = shouldPromote(OldFunc->getReturnType());
  const AttributeSet OldSet = OldFunc->getAttributes();
  const unsigned RetSlot = AttributeSet::ReturnIndex + (returnWasFixed ? 1 : 0);
  AttributeSet NewSet =
      CopyRetAttributes(Ctx, DL, OldSet, OldFunc->getReturnType(), RetSlot);
  Instruction *InsPoint = &*NewFunc->begin()->begin();
  auto NewArgIter = NewFunc->arg_begin();
  // Advance one more if we used to return a struct register.
  if (returnWasFixed)
    NewArgIter++;
  // Wire new parameters in.
  unsigned ArgIndex = 0;
  for (auto ArgIter = OldFunc->arg_begin(), E = OldFunc->arg_end();
       E != ArgIter; ArgIndex++) {
    Argument *OldArg = &*ArgIter++;
    Argument *NewArg = &*NewArgIter++;
    const bool IsAggregateToPtr =
        shouldPromote(OldArg->getType()) && NewArg->getType()->isPointerTy();
    if (!NewFunc->empty()) {
      ConvertArgumentValue(OldArg, NewArg, InsPoint, IsAggregateToPtr);
    }
    Type *Inner = nullptr;
    if (IsAggregateToPtr) {
      Inner = NewArg->getType()->getPointerElementType();
    }
    NewSet =
        CopyArgAttributes(NewSet, Ctx, DL, OldSet, ArgIndex, Inner, RetSlot);
  }
  NewFunc->setAttributes(NewSet);
  // Now fix instruction types. We know that each value could only possibly be
  // of a simplified type. At the end of this, call sites will be invalid, but
  // we handle that afterwards, to make sure we have all the functions changed
  // first (so that calls have valid targets)
  for (auto BBIter = NewFunc->begin(), LBlock = NewFunc->end();
       LBlock != BBIter;) {
    auto Block = BBIter++;
    for (auto IIter = Block->begin(), LIns = Block->end(); LIns != IIter;) {
      auto Instr = IIter++;
      auto *NewTy = Mapper.getSimpleType(Ctx, Instr->getType());
      Instr->mutateType(NewTy);
      if (isa<CallInst>(Instr) ||
          isa<InvokeInst>(Instr)) {
        continue;
      }
      for (unsigned OpI = 0; OpI < Instr->getNumOperands(); OpI++) {
        if(Constant *C = dyn_cast<Constant>(Instr->getOperand(OpI))) {
          auto *NewTy = Mapper.getSimpleType(Ctx, C->getType());
          if (NewTy == C->getType()) { continue; }
          const auto CastOp = CastInst::getCastOpcode(C, false, NewTy, false);
          auto *NewOp = ConstantExpr::getCast(CastOp, C, NewTy);
          Instr->setOperand(OpI, NewOp);
        }
      }
    }
  }
  if (returnWasFixed)
    FixReturn(OldFunc, NewFunc);
}
// Ensure function is simplified, returning true if the function
// had to be changed.
bool SimplifyStructRegSignatures::simplifyFunction(
    LLVMContext &Ctx, Function *OldFunc) {
  auto *OldFT = OldFunc->getFunctionType();
  auto *NewFT = cast<FunctionType>(Mapper.getSimpleType(Ctx, OldFT));
  Function *&AssociatedFctLoc = FunctionMap[OldFunc];
  if (NewFT != OldFT) {
    auto *NewFunc = Function::Create(NewFT, OldFunc->getLinkage());
    AssociatedFctLoc = NewFunc;
    OldFunc->getParent()->getFunctionList().insert(OldFunc->getIterator(), NewFunc);
    NewFunc->takeName(OldFunc);
    UpdateArgNames(OldFunc, NewFunc);
    NewFunc->getBasicBlockList().splice(NewFunc->begin(),
                                        OldFunc->getBasicBlockList());
    fixFunctionBody(Ctx, OldFunc, NewFunc);
    Constant *Cast = ConstantExpr::getPointerCast(NewFunc, OldFunc->getType());
    OldFunc->replaceAllUsesWith(Cast);
    FunctionsToDelete.insert(OldFunc);
  } else {
    AssociatedFctLoc = OldFunc;
  }
  scheduleInstructionsForCleanup(AssociatedFctLoc);
  return NewFT != OldFT;
}
bool SimplifyStructRegSignatures::runOnModule(Module &M) {
  bool Changed = false;
  unsigned PreferredAlignment = 0;
  PreferredAlignment = M.getDataLayout().getStackAlignment();
  LLVMContext &Ctx = M.getContext();
  // Change function signatures and fix a changed function body by
  // wiring the new arguments. Call sites are unchanged at this point.
  for (Module::iterator Iter = M.begin(), E = M.end(); Iter != E;) {
    Function *Func = &*Iter++;
    if (Func->isIntrinsic()) {
      // Can't rewrite intrinsics.
      continue;
    }
    checkNoUnsupportedInstructions(Ctx, Func);
    Changed |= simplifyFunction(Ctx, Func);
  }
  // Fix call sites.
  for (auto &CallToFix : CallsToPatch) {
    fixCallSite(Ctx, CallToFix, PreferredAlignment);
  }
  for (auto &InvokeToFix : InvokesToPatch) {
    fixCallSite(Ctx, InvokeToFix, PreferredAlignment);
  }

  // Update taking of a function's address from a parameter
  for (auto &Addressing : FunctionAddressings) {
    Value *Temp = Addressing.Temp;
    Function *Old = Addressing.Old;
    Function *New = FunctionMap[Old];
    assert(New);
    Temp->replaceAllUsesWith(New);
  }

  // Remaining uses of functions we modified (like in a global vtable)
  // can be handled via a constantexpr bitcast
  for (auto &Old : FunctionsToDelete) {
    Function *New = FunctionMap[Old];
    assert(New);
    Old->replaceAllUsesWith(ConstantExpr::getBitCast(New, Old->getType()));
  }

  // Delete leftover functions - the ones with old signatures.
  for (auto &ToDelete : FunctionsToDelete) {
    ToDelete->eraseFromParent();
  }
  return Changed;
}
void
SimplifyStructRegSignatures::checkNoUnsupportedInstructions(LLVMContext &Ctx,
                                                            Function *Fct) {
  for (auto &BB : Fct->getBasicBlockList()) {
    for (auto &Inst : BB.getInstList()) {
      if (auto *Landing = dyn_cast<LandingPadInst>(&Inst)) {
        auto *LType = Fct->getPersonalityFn()->getType();
        if (LType != Mapper.getSimpleType(Ctx, LType)) {
          errs() << *Landing << '\n';
          report_fatal_error("Landing pads with aggregate register "
                             "signatures are not supported.");
        }
      } else if (auto *Resume = dyn_cast<ResumeInst>(&Inst)) {
        auto *RType = Resume->getValue()->getType();
        if (RType != Mapper.getSimpleType(Ctx, RType)) {
          errs() << *Resume << '\n';
          report_fatal_error(
              "Resumes with aggregate register signatures are not supported.");
        }
      }
    }
  }
}
ModulePass *llvm::createSimplifyStructRegSignaturesPass() {
  return new SimplifyStructRegSignatures();
}
