//===- ReplacePtrsWithInts.cpp - Convert pointer values to integer values--===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass strips out aggregate pointer types and replaces them with
// the integer type iPTR, which is i32 for PNaCl (though this pass
// will allow iPTR to be i64 if the DataLayout specifies 64-bit
// pointers).
//
// This pass relies on -simplify-allocas to transform allocas into arrays of
// bytes.
//
// The pass converts IR to the following normal form:
//
// All inttoptr and ptrtoint instructions use the same integer size
// (iPTR), so they do not implicitly truncate or zero-extend.
//
// Pointer types only appear in the following instructions:
//  * loads and stores:  the pointer operand is a NormalizedPtr.
//  * function calls:  the function operand is a NormalizedPtr.
//  * intrinsic calls:  any pointer arguments are NormalizedPtrs.
//  * alloca
//  * bitcast and inttoptr:  only used as part of a NormalizedPtr.
//  * ptrtoint:  the operand is an InherentPtr.
//
// Where an InherentPtr is defined as a pointer value that is:
//  * an alloca;
//  * a GlobalValue (a function or global variable); or
//  * an intrinsic call.
//
// And a NormalizedPtr is defined as a pointer value that is:
//  * an inttoptr instruction;
//  * an InherentPtr; or
//  * a bitcast of an InherentPtr.
//
// This pass currently strips out lifetime markers (that is, calls to
// the llvm.lifetime.start/end intrinsics) and invariant markers
// (calls to llvm.invariant.start/end).
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

namespace {
  // This is a ModulePass because the pass must recreate functions in
  // order to change their argument and return types.
  struct ReplacePtrsWithInts : public ModulePass {
    static char ID; // Pass identification, replacement for typeid
    ReplacePtrsWithInts() : ModulePass(ID) {
      initializeReplacePtrsWithIntsPass(*PassRegistry::getPassRegistry());
    }

    virtual bool runOnModule(Module &M);
  };

  // FunctionConverter stores the state for mapping old instructions
  // (of pointer type) to converted instructions (of integer type)
  // within a function, and provides methods for doing the conversion.
  class FunctionConverter {
    // Int type that pointer types are to be replaced with, typically i32.
    Type *IntPtrType;

    struct RewrittenVal {
      RewrittenVal(): Placeholder(NULL), NewIntVal(NULL) {}
      Value *Placeholder;
      Value *NewIntVal;
    };
    // Maps from old values (of pointer type) to converted values (of
    // IntPtrType type).
    DenseMap<Value *, RewrittenVal> RewriteMap;

  public:
    FunctionConverter(Type *IntPtrType) : IntPtrType(IntPtrType) {}

    // Returns the normalized version of the given type, converting
    // pointer types to IntPtrType.
    Type *convertType(Type *Ty);
    // Returns the normalized version of the given function type by
    // normalizing the function's argument types.
    FunctionType *convertFuncType(FunctionType *FTy);

    // Records that 'To' is the normalized version of 'From'.  If 'To'
    // is not of pointer type, no type conversion is required, so this
    // can take the short cut of replacing 'To' with 'From'.
    void recordConverted(Value *From, Value *To);
    void recordConvertedAndErase(Instruction *From, Value *To);

    // Returns Val with no-op casts (those that convert between
    // IntPtrType and pointer types) stripped off.
    Value *stripNoopCasts(Value *Val);

    // Returns the normalized version of the given value.
    //
    // If the conversion of Val has been deferred, this returns a
    // placeholder object, which will later be replaceAllUsesWith'd to
    // the final value.  Since replaceAllUsesWith does not work on
    // references by metadata nodes, this can be bypassed using
    // BypassPlaceholder to get the real converted value, assuming it
    // is available.
    Value *convert(Value *Val, bool BypassPlaceholder = false);
    // Returns the NormalizedPtr form of the given pointer value.
    // Inserts conversion instructions at InsertPt.
    Value *convertBackToPtr(Value *Val, Instruction *InsertPt);
    // Returns the NormalizedPtr form of the given function pointer.
    // Inserts conversion instructions at InsertPt.
    Value *convertFunctionPtr(Value *Callee, Instruction *InsertPt);
    // Converts an instruction without recreating it, by wrapping its
    // operands and result.
    void convertInPlace(Instruction *Inst);

    void eraseReplacedInstructions();

    // List of instructions whose deletion has been deferred.
    SmallVector<Instruction *, 20> ToErase;
  };
}

Type *FunctionConverter::convertType(Type *Ty) {
  if (Ty->isPointerTy())
    return IntPtrType;
  return Ty;
}

FunctionType *FunctionConverter::convertFuncType(FunctionType *FTy) {
  SmallVector<Type *, 8> ArgTypes;
  for (FunctionType::param_iterator ArgTy = FTy->param_begin(),
           E = FTy->param_end(); ArgTy != E; ++ArgTy) {
    ArgTypes.push_back(convertType(*ArgTy));
  }
  return FunctionType::get(convertType(FTy->getReturnType()), ArgTypes,
                           FTy->isVarArg());
}

void FunctionConverter::recordConverted(Value *From, Value *To) {
  if (!From->getType()->isPointerTy()) {
    From->replaceAllUsesWith(To);
    return;
  }
  RewrittenVal *RV = &RewriteMap[From];
  assert(!RV->NewIntVal);
  RV->NewIntVal = To;
}

void FunctionConverter::recordConvertedAndErase(Instruction *From, Value *To) {
  recordConverted(From, To);
  // There may still be references to this value, so defer deleting it.
  ToErase.push_back(From);
}

Value *FunctionConverter::stripNoopCasts(Value *Val) {
  SmallPtrSet<Value *, 4> Visited;
  for (;;) {
    if (!Visited.insert(Val).second) {
      // It is possible to get a circular reference in unreachable
      // basic blocks.  Handle this case for completeness.
      return UndefValue::get(Val->getType());
    }
    if (CastInst *Cast = dyn_cast<CastInst>(Val)) {
      Value *Src = Cast->getOperand(0);
      if ((isa<BitCastInst>(Cast) && Cast->getType()->isPointerTy()) ||
          (isa<PtrToIntInst>(Cast) && Cast->getType() == IntPtrType) ||
          (isa<IntToPtrInst>(Cast) && Src->getType() == IntPtrType)) {
        Val = Src;
        continue;
      }
    }
    return Val;
  }
}

Value *FunctionConverter::convert(Value *Val, bool BypassPlaceholder) {
  Val = stripNoopCasts(Val);
  if (!Val->getType()->isPointerTy())
    return Val;
  if (Constant *C = dyn_cast<Constant>(Val))
    return ConstantExpr::getPtrToInt(C, IntPtrType);
  RewrittenVal *RV = &RewriteMap[Val];
  if (BypassPlaceholder) {
    assert(RV->NewIntVal);
    return RV->NewIntVal;
  }
  if (!RV->Placeholder)
    RV->Placeholder = new Argument(convertType(Val->getType()));
  return RV->Placeholder;
}

Value *FunctionConverter::convertBackToPtr(Value *Val, Instruction *InsertPt) {
  Type *NewTy =
    convertType(Val->getType()->getPointerElementType())->getPointerTo();
  return new IntToPtrInst(convert(Val), NewTy, "", InsertPt);
}

Value *FunctionConverter::convertFunctionPtr(Value *Callee,
                                             Instruction *InsertPt) {
  FunctionType *FuncType = cast<FunctionType>(
      Callee->getType()->getPointerElementType());
  return new IntToPtrInst(convert(Callee),
                          convertFuncType(FuncType)->getPointerTo(),
                          "", InsertPt);
}

static bool ShouldLeaveAlone(Value *V) {
  if (Function *F = dyn_cast<Function>(V))
    return F->isIntrinsic();
  if (isa<InlineAsm>(V))
    return true;
  return false;
}

void FunctionConverter::convertInPlace(Instruction *Inst) {
  // Convert operands.
  for (unsigned I = 0; I < Inst->getNumOperands(); ++I) {
    Value *Arg = Inst->getOperand(I);
    if (Arg->getType()->isPointerTy() && !ShouldLeaveAlone(Arg)) {
      Value *Conv = convert(Arg);
      Inst->setOperand(I, new IntToPtrInst(Conv, Arg->getType(), "", Inst));
    }
  }
  // Convert result.
  if (Inst->getType()->isPointerTy()) {
    Instruction *Cast = new PtrToIntInst(
        Inst, convertType(Inst->getType()), Inst->getName() + ".asint");
    Cast->insertAfter(Inst);
    recordConverted(Inst, Cast);
  }
}

void FunctionConverter::eraseReplacedInstructions() {
  bool Error = false;
  for (DenseMap<Value *, RewrittenVal>::iterator I = RewriteMap.begin(),
           E = RewriteMap.end(); I != E; ++I) {
    if (I->second.Placeholder) {
      if (I->second.NewIntVal) {
        I->second.Placeholder->replaceAllUsesWith(I->second.NewIntVal);
      } else {
        errs() << "Not converted: " << *I->first << "\n";
        Error = true;
      }
    }
  }
  if (Error)
    report_fatal_error("Case not handled in ReplacePtrsWithInts");

  // Delete the placeholders in a separate pass.  This means that if
  // one placeholder is accidentally rewritten to another, we will get
  // a useful error message rather than accessing a dangling pointer.
  for (DenseMap<Value *, RewrittenVal>::iterator I = RewriteMap.begin(),
           E = RewriteMap.end(); I != E; ++I) {
    delete I->second.Placeholder;
  }

  // We must do dropAllReferences() before doing eraseFromParent(),
  // otherwise we will try to erase instructions that are still
  // referenced.
  for (SmallVectorImpl<Instruction *>::iterator I = ToErase.begin(),
           E = ToErase.end();
       I != E; ++I) {
    (*I)->dropAllReferences();
  }
  for (SmallVectorImpl<Instruction *>::iterator I = ToErase.begin(),
           E = ToErase.end();
       I != E; ++I) {
    (*I)->eraseFromParent();
  }
}

// Remove attributes that only apply to pointer arguments.  Returns
// the updated AttributeSet.
static AttributeSet RemovePointerAttrs(LLVMContext &Context,
                                       AttributeSet Attrs) {
  SmallVector<AttributeSet, 8> AttrList;
  for (unsigned Slot = 0; Slot < Attrs.getNumSlots(); ++Slot) {
    unsigned Index = Attrs.getSlotIndex(Slot);
    AttrBuilder AB;
    for (AttributeSet::iterator Attr = Attrs.begin(Slot), E = Attrs.end(Slot);
         Attr != E; ++Attr) {
      if (!Attr->isEnumAttribute()) {
        continue;
      }
      switch (Attr->getKindAsEnum()) {
        // ByVal and StructRet should already have been removed by the
        // ExpandByVal pass.
        case Attribute::ByVal:
        case Attribute::StructRet:
        case Attribute::Nest:
          Attrs.dump();
          report_fatal_error("ReplacePtrsWithInts cannot handle "
                             "byval, sret or nest attrs");
          break;
        // Strip these attributes because they apply only to pointers. This pass
        // rewrites pointer arguments, thus these parameter attributes are
        // meaningless. Also, they are rejected by the PNaCl module verifier.
        case Attribute::NoCapture:
        case Attribute::NoAlias:
        case Attribute::ReadNone:
        case Attribute::ReadOnly:
        case Attribute::NonNull:
        case Attribute::Dereferenceable:
        case Attribute::DereferenceableOrNull:
          break;
        default:
          AB.addAttribute(*Attr);
      }
    }
    AttrList.push_back(AttributeSet::get(Context, Index, AB));
  }
  return AttributeSet::get(Context, AttrList);
}

static void ConvertInstruction(DataLayout *DL, Type *IntPtrType,
                               FunctionConverter *FC, Instruction *Inst) {
  if (ReturnInst *Ret = dyn_cast<ReturnInst>(Inst)) {
    Value *Result = Ret->getReturnValue();
    if (Result)
      Result = FC->convert(Result);
    CopyDebug(ReturnInst::Create(Ret->getContext(), Result, Ret), Inst);
    Ret->eraseFromParent();
  } else if (PHINode *Phi = dyn_cast<PHINode>(Inst)) {
    PHINode *Phi2 = PHINode::Create(FC->convertType(Phi->getType()),
                                    Phi->getNumIncomingValues(),
                                    "", Phi);
    CopyDebug(Phi2, Phi);
    for (unsigned I = 0; I < Phi->getNumIncomingValues(); ++I) {
      Phi2->addIncoming(FC->convert(Phi->getIncomingValue(I)),
                        Phi->getIncomingBlock(I));
    }
    Phi2->takeName(Phi);
    FC->recordConvertedAndErase(Phi, Phi2);
  } else if (SelectInst *Op = dyn_cast<SelectInst>(Inst)) {
    Instruction *Op2 = SelectInst::Create(Op->getCondition(),
                                          FC->convert(Op->getTrueValue()),
                                          FC->convert(Op->getFalseValue()),
                                          "", Op);
    CopyDebug(Op2, Op);
    Op2->takeName(Op);
    FC->recordConvertedAndErase(Op, Op2);
  } else if (isa<PtrToIntInst>(Inst) || isa<IntToPtrInst>(Inst)) {
    Value *Arg = FC->convert(Inst->getOperand(0));
    Type *ResultTy = FC->convertType(Inst->getType());
    unsigned ArgSize = Arg->getType()->getIntegerBitWidth();
    unsigned ResultSize = ResultTy->getIntegerBitWidth();
    Value *Result;
    // We avoid using IRBuilder's CreateZExtOrTrunc() here because it
    // constant-folds ptrtoint ConstantExprs.  This leads to creating
    // ptrtoints of non-IntPtrType type, which is not what we want,
    // because we want truncation/extension to be done explicitly by
    // separate instructions.
    if (ArgSize == ResultSize) {
      Result = Arg;
    } else {
      Instruction::CastOps CastType =
          ArgSize > ResultSize ? Instruction::Trunc : Instruction::ZExt;
      Result = CopyDebug(CastInst::Create(CastType, Arg, ResultTy, "", Inst),
                         Inst);
    }
    if (Result != Arg)
      Result->takeName(Inst);
    FC->recordConvertedAndErase(Inst, Result);
  } else if (isa<BitCastInst>(Inst)) {
    if (Inst->getType()->isPointerTy()) {
      FC->ToErase.push_back(Inst);
    }
  } else if (ICmpInst *Cmp = dyn_cast<ICmpInst>(Inst)) {
    Value *Cmp2 = CopyDebug(new ICmpInst(Inst, Cmp->getPredicate(),
                                         FC->convert(Cmp->getOperand(0)),
                                         FC->convert(Cmp->getOperand(1)), ""),
                            Inst);
    Cmp2->takeName(Cmp);
    Cmp->replaceAllUsesWith(Cmp2);
    Cmp->eraseFromParent();
  } else if (LoadInst *Load = dyn_cast<LoadInst>(Inst)) {
    Value *Ptr = FC->convertBackToPtr(Load->getPointerOperand(), Inst);
    LoadInst *Result = new LoadInst(Ptr, "", Inst);
    Result->takeName(Inst);
    CopyDebug(Result, Inst);
    CopyLoadOrStoreAttrs(Result, Load);
    FC->recordConvertedAndErase(Inst, Result);
  } else if (StoreInst *Store = dyn_cast<StoreInst>(Inst)) {
    Value *Ptr = FC->convertBackToPtr(Store->getPointerOperand(), Inst);
    StoreInst *Result = new StoreInst(FC->convert(Store->getValueOperand()),
                                      Ptr, Inst);
    CopyDebug(Result, Inst);
    CopyLoadOrStoreAttrs(Result, Store);
    Inst->eraseFromParent();
  } else if (CallInst *Call = dyn_cast<CallInst>(Inst)) {
    if (IntrinsicInst *ICall = dyn_cast<IntrinsicInst>(Inst)) {
      if (ICall->getIntrinsicID() == Intrinsic::lifetime_start ||
          ICall->getIntrinsicID() == Intrinsic::lifetime_end ||
          ICall->getIntrinsicID() == Intrinsic::invariant_start) {
        // Remove alloca lifetime markers for now.  This is because
        // the GVN pass can introduce lifetime markers taking PHI
        // nodes as arguments.  If ReplacePtrsWithInts converts the
        // PHI node to int type, we will render those lifetime markers
        // ineffective.  But dropping a subset of lifetime markers is
        // not safe in general.  So, until LLVM better defines the
        // semantics of lifetime markers, we drop them all.  See:
        // https://code.google.com/p/nativeclient/issues/detail?id=3443
        // We do the same for invariant.start/end because they work in
        // a similar way.
        Inst->eraseFromParent();
      } else {
        FC->convertInPlace(Inst);
      }
    } else if (isa<InlineAsm>(Call->getCalledValue())) {
      FC->convertInPlace(Inst);
    } else {
      SmallVector<Value *, 10> Args;
      for (unsigned I = 0; I < Call->getNumArgOperands(); ++I)
        Args.push_back(FC->convert(Call->getArgOperand(I)));
      CallInst *NewCall = CallInst::Create(
          FC->convertFunctionPtr(Call->getCalledValue(), Call),
          Args, "", Inst);
      CopyDebug(NewCall, Call);
      NewCall->setAttributes(RemovePointerAttrs(Call->getContext(),
                                                Call->getAttributes()));
      NewCall->setCallingConv(Call->getCallingConv());
      NewCall->setTailCall(Call->isTailCall());
      NewCall->takeName(Call);
      FC->recordConvertedAndErase(Call, NewCall);
    }
  } else if (InvokeInst *Call = dyn_cast<InvokeInst>(Inst)) {
    SmallVector<Value *, 10> Args;
    for (unsigned I = 0; I < Call->getNumArgOperands(); ++I)
      Args.push_back(FC->convert(Call->getArgOperand(I)));
    InvokeInst *NewCall = InvokeInst::Create(
        FC->convertFunctionPtr(Call->getCalledValue(), Call),
        Call->getNormalDest(),
        Call->getUnwindDest(),
        Args, "", Inst);
    CopyDebug(NewCall, Call);
    NewCall->setAttributes(RemovePointerAttrs(Call->getContext(),
                                              Call->getAttributes()));
    NewCall->setCallingConv(Call->getCallingConv());
    NewCall->takeName(Call);
    FC->recordConvertedAndErase(Call, NewCall);
  } else if (// Handle these instructions as a convenience to allow
             // the pass to be used in more situations, even though we
             // don't expect them in PNaCl's stable ABI.
             isa<AllocaInst>(Inst) ||
             isa<GetElementPtrInst>(Inst) ||
             isa<VAArgInst>(Inst) ||
             isa<IndirectBrInst>(Inst) ||
             isa<ExtractValueInst>(Inst) ||
             isa<InsertValueInst>(Inst) ||
             // These atomics only operate on integer pointers, not
             // other pointers, so we don't need to recreate the
             // instruction.
             isa<AtomicCmpXchgInst>(Inst) ||
             isa<AtomicRMWInst>(Inst)) {
    FC->convertInPlace(Inst);
  }
}

// Convert ptrtoint+inttoptr to a bitcast because it's shorter and
// because some intrinsics work on bitcasts but not on
// ptrtoint+inttoptr, in particular:
//  * llvm.lifetime.start/end (although we strip these out)
//  * llvm.eh.typeid.for
static void SimplifyCasts(Instruction *Inst, Type *IntPtrType) {
  if (IntToPtrInst *Cast1 = dyn_cast<IntToPtrInst>(Inst)) {
    if (PtrToIntInst *Cast2 = dyn_cast<PtrToIntInst>(Cast1->getOperand(0))) {
      assert(Cast2->getType() == IntPtrType);
      Value *V = Cast2->getPointerOperand();
      if (V->getType() != Cast1->getType())
        V = new BitCastInst(V, Cast1->getType(), V->getName() + ".bc", Cast1);
      Cast1->replaceAllUsesWith(V);
      if (Cast1->use_empty())
        Cast1->eraseFromParent();
      if (Cast2->use_empty())
        Cast2->eraseFromParent();
    }
  }
}

static void CleanUpFunction(Function *Func, Type *IntPtrType) {
  // Remove the ptrtoint/bitcast ConstantExprs we introduced for
  // referencing globals.
  FunctionPass *Pass = createExpandConstantExprPass();
  Pass->runOnFunction(*Func);
  delete Pass;

  for (Function::iterator BB = Func->begin(), E = Func->end();
       BB != E; ++BB) {
    for (BasicBlock::iterator Iter = BB->begin(), E = BB->end();
         Iter != E; ) {
      SimplifyCasts(&*Iter++, IntPtrType);
    }
  }
  // Cleanup pass.
  for (Function::iterator BB = Func->begin(), E = Func->end();
       BB != E; ++BB) {
    for (BasicBlock::iterator Iter = BB->begin(), E = BB->end();
         Iter != E; ) {
      Instruction *Inst = &*Iter++;
      // Add names to inttoptrs to make the output more readable.  The
      // placeholder values get in the way of doing this earlier when
      // the inttoptrs are created.
      if (isa<IntToPtrInst>(Inst))
        Inst->setName(Inst->getOperand(0)->getName() + ".asptr");
      // Remove ptrtoints that were introduced for allocas but not used.
      if (isa<PtrToIntInst>(Inst) && Inst->use_empty())
        Inst->eraseFromParent();
    }
  }
}

char ReplacePtrsWithInts::ID = 0;
INITIALIZE_PASS(ReplacePtrsWithInts, "replace-ptrs-with-ints",
                "Convert pointer values to integer values",
                false, false)

bool ReplacePtrsWithInts::runOnModule(Module &M) {
  DataLayout DL(&M);
  Type *IntPtrType = DL.getIntPtrType(M.getContext());

  for (Module::iterator Iter = M.begin(), E = M.end(); Iter != E; ) {
    Function *OldFunc = &*Iter++;
    // Intrinsics' types must be left alone.
    if (OldFunc->isIntrinsic())
      continue;

    FunctionConverter FC(IntPtrType);
    FunctionType *NFTy = FC.convertFuncType(OldFunc->getFunctionType());
    OldFunc->setAttributes(RemovePointerAttrs(M.getContext(),
                                              OldFunc->getAttributes()));
    Function *NewFunc = RecreateFunction(OldFunc, NFTy);

    // Move the arguments across to the new function.
    for (Function::arg_iterator Arg = OldFunc->arg_begin(),
             E = OldFunc->arg_end(), NewArg = NewFunc->arg_begin();
         Arg != E; ++Arg, ++NewArg) {
      FC.recordConverted(&*Arg, &*NewArg);
      NewArg->takeName(&*Arg);
    }

    // invariant.end calls refer to invariant.start calls, so we must
    // remove the former first.
    for (Function::iterator BB = NewFunc->begin(), E = NewFunc->end();
         BB != E; ++BB) {
      for (BasicBlock::iterator Iter = BB->begin(), E = BB->end();
           Iter != E; ) {
        if (IntrinsicInst *ICall = dyn_cast<IntrinsicInst>(Iter++)) {
          if (ICall->getIntrinsicID() == Intrinsic::invariant_end)
            ICall->eraseFromParent();
        }
      }
    }

    // Convert the function body.
    for (Function::iterator BB = NewFunc->begin(), E = NewFunc->end();
         BB != E; ++BB) {
      for (BasicBlock::iterator Iter = BB->begin(), E = BB->end();
           Iter != E; ) {
        ConvertInstruction(&DL, IntPtrType, &FC, &*Iter++);
      }
    }
    FC.eraseReplacedInstructions();

    OldFunc->eraseFromParent();
  }
  // Now that all functions have their normalized types, we can remove
  // various casts.
  for (Module::iterator Func = M.begin(), E = M.end(); Func != E; ++Func) {
    CleanUpFunction(&*Func, IntPtrType);
    // Delete the now-unused bitcast ConstantExprs that we created so
    // that they don't interfere with StripDeadPrototypes.
    Func->removeDeadConstantUsers();
  }
  return true;
}

ModulePass *llvm::createReplacePtrsWithIntsPass() {
  return new ReplacePtrsWithInts();
}
