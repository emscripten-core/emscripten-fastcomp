//===- PromoteIntegers.cpp - Promote illegal integers for PNaCl ABI -------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
// A limited set of transformations to promote illegal-sized int types.
//
//===----------------------------------------------------------------------===//
//
// Legal sizes are currently 1, 8, 16, 32, 64 (and higher, see note below)
// Operations on illegal integers and int pointers are be changed to operate
// on the next-higher legal size.
// It maintains no invariants about the upper bits (above the size of the
// original type); therefore before operations which can be affected by the
// value of these bits (e.g. cmp, select, lshr), the upper bits of the operands
// are cleared.
//
// Limitations:
// 1) It can't change function signatures or global variables
// 2) It won't promote (and can't expand) types larger than i64
// 3) Doesn't support div operators
// 4) Doesn't handle arrays or structs (or GEPs) with illegal types
// 5) Doesn't handle constant expressions (it also doesn't produce them, so it
//    can run after ExpandConstantExpr)
//
//===----------------------------------------------------------------------===//


#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Pass.h"
#include "llvm/Support/IntegersSubset.h"
#include "llvm/Support/IntegersSubsetMapping.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

namespace {
class ConversionState;

class PromoteIntegers : public FunctionPass {
  DataLayout *DL;

  Value *splitLoad(LoadInst *Inst, ConversionState &State);
  Value *splitStore(StoreInst *Inst, ConversionState &State);
  void convertInstruction(Instruction *Inst, ConversionState &State);

 public:
  static char ID;
  PromoteIntegers() : FunctionPass(ID) {
    initializePromoteIntegersPass(*PassRegistry::getPassRegistry());
  }
  virtual bool runOnFunction(Function &F);
  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<DataLayout>();
    return FunctionPass::getAnalysisUsage(AU);
  }
};
}

char PromoteIntegers::ID = 0;
INITIALIZE_PASS(PromoteIntegers, "nacl-promote-ints",
                "Promote integer types which are illegal in PNaCl",
                false, false)

// Legal sizes are currently 1, 8, 16, 32, and 64.
// We can't yet expand types above 64 bit, so don't try to touch them for now.
// TODO(dschuff): expand >64bit types or disallow >64bit packed bitfields.
// There are currently none in our tests that use the ABI checker.
// See https://code.google.com/p/nativeclient/issues/detail?id=3360
static bool isLegalSize(unsigned Size) {
#if 0 // XXX EMSCRIPTEN: Generalize this code to work on any bit width.
  if (Size > 64) return true;
  return Size == 1 || Size == 8 || Size == 16 || Size == 32 || Size == 64;
#else
  return Size == 1 || (Size >= 8 && isPowerOf2_32(Size));
#endif
}

static Type *getPromotedIntType(IntegerType *Ty) {
  unsigned Width = Ty->getBitWidth();
#if 0 // XXX EMSCRIPTEN: We support promoting these types to power-of-2 sizes.
  assert(Width <= 64 && "Don't know how to legalize >64 bit types yet");
#endif
  if (isLegalSize(Width))
    return Ty;
  return IntegerType::get(Ty->getContext(),
                          Width < 8 ? 8 : NextPowerOf2(Width));
}

// Return a legal integer or pointer-to-integer type, promoting to a larger
// size if necessary.
static Type *getPromotedType(Type *Ty) {
  assert((isa<IntegerType>(Ty) ||
          (isa<PointerType>(Ty) && isa<IntegerType>(Ty->getContainedType(0))))
         && "Trying to convert a non-integer type");

  if (isa<PointerType>(Ty))
    return getPromotedIntType(
        cast<IntegerType>(Ty->getContainedType(0)))->getPointerTo();

  return getPromotedIntType(cast<IntegerType>(Ty));
}

// Return true if Val is an int or pointer-to-int which should be converted.
static bool shouldConvert(Value *Val) {
  Type *Ty = Val->getType();
#if 0 // XXX EMSCRIPTEN: We don't need to convert pointers.
  if (PointerType *Pty = dyn_cast<PointerType>(Ty))
    Ty = Pty->getContainedType(0);
#endif
  if (IntegerType *ITy = dyn_cast<IntegerType>(Ty)) {
    if (!isLegalSize(ITy->getBitWidth())) {
      return true;
    }
  }
  return false;
}

// Return a constant which has been promoted to a legal size.
static Value *convertConstant(Constant *C, bool SignExt=false) {
  assert(shouldConvert(C));
  if (isa<UndefValue>(C)) {
    return UndefValue::get(getPromotedType(C->getType()));
  } else if (ConstantInt *CInt = dyn_cast<ConstantInt>(C)) {
#if 0 // XXX EMSCRIPTEN: Generalize this code to work on any bit width.
    return ConstantInt::get(
        getPromotedType(C->getType()),
        SignExt ? CInt->getSExtValue() : CInt->getZExtValue(),
        /*isSigned=*/SignExt);
#else
    unsigned BitWidth = getPromotedType(C->getType())->getIntegerBitWidth();
    const APInt &Value = CInt->getValue();
    return ConstantInt::get(C->getContext(),
                            SignExt ? Value.sext(BitWidth) : Value.zext(BitWidth));
#endif
  } else {
    errs() << "Value: " << *C << "\n";
    report_fatal_error("Unexpected constant value");
  }
}

namespace {

// Holds the state for converting/replacing values. Conversion is done in one
// pass, with each value requiring conversion possibly having two stages. When
// an instruction needs to be replaced (i.e. it has illegal operands or result)
// a new instruction is created, and the pass calls getConverted to get its
// operands. If the original operand has already been converted, the new value
// is returned. Otherwise, a placeholder is created and used in the new
// instruction. After a new instruction is created to replace an illegal one,
// recordConverted is called to register the replacement. All users are updated,
// and if there is a placeholder, its users are also updated.
// recordConverted also queues the old value for deletion.
// This strategy avoids the need for recursion or worklists for conversion.
class ConversionState {
 public:
  // Return the promoted value for Val. If Val has not yet been converted,
  // return a placeholder, which will be converted later.
  Value *getConverted(Value *Val) {
    if (!shouldConvert(Val))
        return Val;
    if (isa<GlobalVariable>(Val))
      report_fatal_error("Can't convert illegal GlobalVariables");
    if (RewrittenMap.count(Val))
      return RewrittenMap[Val];
    Value *P;
    // Directly convert constants.
    if (Constant *C = dyn_cast<Constant>(Val)) {
      return convertConstant(C, /*SignExt=*/false);
    } else {
      // No converted value available yet, so create a placeholder.
      P = new Argument(getPromotedType(Val->getType()));
    }
    RewrittenMap[Val] = P;
    Placeholders[Val] = P;
    return P;
  }

  // Replace the uses of From with To, replace the uses of any
  // placeholders for From, and optionally give From's name to To.
  // Also mark To for deletion.
  void recordConverted(Instruction *From, Value *To, bool TakeName=true) {
    ToErase.push_back(From);
    if (!shouldConvert(From)) {
      // From does not produce an illegal value, update its users in place.
      From->replaceAllUsesWith(To);
    } else {
      // From produces an illegal value, so its users will be replaced. When
      // replacements are created they will use values returned by getConverted.
      if (Placeholders.count(From)) {
        // Users of the placeholder can be updated in place.
        Placeholders[From]->replaceAllUsesWith(To);
        Placeholders.erase(From);
      }
      RewrittenMap[From] = To;
    }
    if (TakeName) {
      To->takeName(From);
    }
  }

  void eraseReplacedInstructions() {
    for (SmallVectorImpl<Instruction *>::iterator I = ToErase.begin(),
             E = ToErase.end(); I != E; ++I)
      (*I)->dropAllReferences();
    for (SmallVectorImpl<Instruction *>::iterator I = ToErase.begin(),
             E = ToErase.end(); I != E; ++I)
      (*I)->eraseFromParent();
  }

 private:
  // Maps illegal values to their new converted values (or placeholders
  // if no new value is available yet)
  DenseMap<Value *, Value *> RewrittenMap;
  // Maps illegal values with no conversion available yet to their placeholders
  DenseMap<Value *, Value *> Placeholders;
  // Illegal values which have already been converted, will be erased.
  SmallVector<Instruction *, 8> ToErase;
};

} // anonymous namespace

// Split an illegal load into multiple legal loads and return the resulting
// promoted value. The size of the load is assumed to be a multiple of 8.
Value *PromoteIntegers::splitLoad(LoadInst *Inst, ConversionState &State) {
  if (Inst->isVolatile() || Inst->isAtomic())
    report_fatal_error("Can't split volatile/atomic loads");
  if (DL->getTypeSizeInBits(Inst->getType()) % 8 != 0)
    report_fatal_error("Loads must be a multiple of 8 bits");

  Value *OrigPtr = State.getConverted(Inst->getPointerOperand());
  // OrigPtr is a placeholder in recursive calls, and so has no name
  if (OrigPtr->getName().empty())
    OrigPtr->setName(Inst->getPointerOperand()->getName());
  unsigned Width = DL->getTypeSizeInBits(Inst->getType());
  Type *NewType = getPromotedType(Inst->getType());
  unsigned LoWidth = Width;

  while (!isLegalSize(LoWidth)) LoWidth -= 8;
  IntegerType *LoType = IntegerType::get(Inst->getContext(), LoWidth);
  IntegerType *HiType = IntegerType::get(Inst->getContext(), Width - LoWidth);
  IRBuilder<> IRB(Inst);

  Value *BCLo = IRB.CreateBitCast(
      OrigPtr,
      LoType->getPointerTo(),
      OrigPtr->getName() + ".loty");
  Value *LoadLo = IRB.CreateAlignedLoad(
      BCLo, Inst->getAlignment(), Inst->getName() + ".lo");
  Value *LoExt = IRB.CreateZExt(LoadLo, NewType, LoadLo->getName() + ".ext");
  Value *GEPHi = IRB.CreateConstGEP1_32(BCLo, 1, OrigPtr->getName() + ".hi");
  Value *BCHi = IRB.CreateBitCast(
        GEPHi,
        HiType->getPointerTo(),
        OrigPtr->getName() + ".hity");

#if 0 // XXX EMSCRIPTEN: We want the full-strength alignment.
  Value *LoadHi = IRB.CreateAlignedLoad(BCHi, 1, Inst->getName() + ".hi");
#else
  unsigned HiAlign = MinAlign(Inst->getAlignment() == 0 ?
                                  DL->getABITypeAlignment(Inst->getType()) :
                                  Inst->getAlignment(),
                              LoWidth / 8);
  Value *LoadHi = IRB.CreateAlignedLoad(BCHi, HiAlign, Inst->getName() + ".hi");
#endif
  if (!isLegalSize(Width - LoWidth)) {
    LoadHi = splitLoad(cast<LoadInst>(LoadHi), State);
#if 0 /// XXX EMSCRIPTEN: We don't need to convert pointers.
    // BCHi was still illegal, and has been replaced with a placeholder in the
    // recursive call. Since it is redundant with BCLo in the recursive call,
    // just splice it out entirely.
    State.recordConverted(cast<Instruction>(BCHi), GEPHi, /*TakeName=*/false);
#endif
  }

  Value *HiExt = IRB.CreateZExt(LoadHi, NewType, LoadHi->getName() + ".ext");
  Value *HiShift = IRB.CreateShl(HiExt, LoWidth, HiExt->getName() + ".sh");
  Value *Result = IRB.CreateOr(LoExt, HiShift);

  State.recordConverted(Inst, Result);

  return Result;
}

Value *PromoteIntegers::splitStore(StoreInst *Inst, ConversionState &State) {
  if (Inst->isVolatile() || Inst->isAtomic())
    report_fatal_error("Can't split volatile/atomic stores");
  if (DL->getTypeSizeInBits(Inst->getValueOperand()->getType()) % 8 != 0)
    report_fatal_error("Stores must be a multiple of 8 bits");

  Value *OrigPtr = State.getConverted(Inst->getPointerOperand());
  // OrigPtr is now a placeholder in recursive calls, and so has no name.
  if (OrigPtr->getName().empty())
    OrigPtr->setName(Inst->getPointerOperand()->getName());
  Value *OrigVal = State.getConverted(Inst->getValueOperand());
  unsigned Width = DL->getTypeSizeInBits(Inst->getValueOperand()->getType());
  unsigned LoWidth = Width;

  while (!isLegalSize(LoWidth)) LoWidth -= 8;
  IntegerType *LoType = IntegerType::get(Inst->getContext(), LoWidth);
  IntegerType *HiType = IntegerType::get(Inst->getContext(), Width - LoWidth);
  IRBuilder<> IRB(Inst);

  Value *BCLo = IRB.CreateBitCast(
      OrigPtr,
      LoType->getPointerTo(),
      OrigPtr->getName() + ".loty");
  Value *LoTrunc = IRB.CreateTrunc(
      OrigVal, LoType, OrigVal->getName() + ".lo");
  IRB.CreateAlignedStore(LoTrunc, BCLo, Inst->getAlignment());

  Value *HiLShr = IRB.CreateLShr(
      OrigVal, LoWidth, OrigVal->getName() + ".hi.sh");
  Value *GEPHi = IRB.CreateConstGEP1_32(BCLo, 1, OrigPtr->getName() + ".hi");
  Value *HiTrunc = IRB.CreateTrunc(
      HiLShr, HiType, OrigVal->getName() + ".hi");
  Value *BCHi = IRB.CreateBitCast(
        GEPHi,
        HiType->getPointerTo(),
        OrigPtr->getName() + ".hity");

#if 0 // XXX EMSCRIPTEN: We want the full-strength alignment.
  Value *StoreHi = IRB.CreateAlignedStore(HiTrunc, BCHi, 1);
#else
  unsigned HiAlign = MinAlign(Inst->getAlignment() == 0 ?
                                  DL->getABITypeAlignment(Inst->getValueOperand()->getType()) :
                                  Inst->getAlignment(),
                              LoWidth / 8);
  Value *StoreHi = IRB.CreateAlignedStore(HiTrunc, BCHi, HiAlign);
#endif

  if (!isLegalSize(Width - LoWidth)) {
    // HiTrunc is still illegal, and is redundant with the truncate in the
    // recursive call, so just get rid of it.
#if 0 /// XXX EMSCRIPTEN: Allow these to be ConstantExprs
    State.recordConverted(cast<Instruction>(HiTrunc), HiLShr,
                          /*TakeName=*/false);
#else
    if (Instruction *HiTruncInst = dyn_cast<Instruction>(HiTrunc)) {
      State.recordConverted(HiTruncInst, HiLShr,
                            /*TakeName=*/false);
    }
#endif
    StoreHi = splitStore(cast<StoreInst>(StoreHi), State);
    // BCHi was still illegal, and has been replaced with a placeholder in the
    // recursive call. Since it is redundant with BCLo in the recursive call,
    // just splice it out entirely.
#if 0 /// XXX EMSCRIPTEN: We don't need to convert pointers.
    State.recordConverted(cast<Instruction>(BCHi), GEPHi, /*TakeName=*/false);
#endif
  }
  State.recordConverted(Inst, StoreHi, /*TakeName=*/false);
  return StoreHi;
}

// Return a converted value with the bits of the operand above the size of the
// original type cleared.
static Value *getClearConverted(Value *Operand, Instruction *InsertPt,
                                ConversionState &State) {
  Type *OrigType = Operand->getType();
  Instruction *OrigInst = dyn_cast<Instruction>(Operand);
  Operand = State.getConverted(Operand);
  // If the operand is a constant, it will have been created by
  // ConversionState.getConverted, which zero-extends by default.
  if (isa<Constant>(Operand))
    return Operand;
  Instruction *NewInst = BinaryOperator::Create(
      Instruction::And,
      Operand,
      ConstantInt::get(
          getPromotedType(OrigType),
          APInt::getLowBitsSet(getPromotedType(OrigType)->getIntegerBitWidth(),
                               OrigType->getIntegerBitWidth())),
      Operand->getName() + ".clear",
      InsertPt);
  if (OrigInst)
    CopyDebug(NewInst, OrigInst);
  return NewInst;
}

// Return a value with the bits of the operand above the size of the original
// type equal to the sign bit of the original operand. The new operand is
// assumed to have been legalized already.
// This is done by shifting the sign bit of the smaller value up to the MSB
// position in the larger size, and then arithmetic-shifting it back down.
static Value *getSignExtend(Value *Operand, Value *OrigOperand,
                            Instruction *InsertPt) {
  // If OrigOperand was a constant, NewOperand will have been created by
  // ConversionState.getConverted, which zero-extends by default. But that is
  // wrong here, so replace it with a sign-extended constant.
  if (Constant *C = dyn_cast<Constant>(OrigOperand))
    return convertConstant(C, /*SignExt=*/true);
  Type *OrigType = OrigOperand->getType();
  ConstantInt *ShiftAmt = ConstantInt::getSigned(
      cast<IntegerType>(getPromotedType(OrigType)),
      getPromotedType(OrigType)->getIntegerBitWidth() -
        OrigType->getIntegerBitWidth());
  BinaryOperator *Shl = BinaryOperator::Create(
      Instruction::Shl,
      Operand,
      ShiftAmt,
      Operand->getName() + ".getsign",
      InsertPt);
  if (Instruction *Inst = dyn_cast<Instruction>(OrigOperand))
    CopyDebug(Shl, Inst);
  return CopyDebug(BinaryOperator::Create(
      Instruction::AShr,
      Shl,
      ShiftAmt,
      Operand->getName() + ".signed",
      InsertPt), Shl);
}

void PromoteIntegers::convertInstruction(Instruction *Inst, ConversionState &State) {
  if (SExtInst *Sext = dyn_cast<SExtInst>(Inst)) {
    Value *Op = Sext->getOperand(0);
    Value *NewInst = NULL;
    // If the operand to be extended is illegal, we first need to fill its
    // upper bits with its sign bit.
    if (shouldConvert(Op)) {
      NewInst = getSignExtend(State.getConverted(Op), Op, Sext);
    }
    // If the converted type of the operand is the same as the converted
    // type of the result, we won't actually be changing the type of the
    // variable, just its value.
    if (getPromotedType(Op->getType()) !=
        getPromotedType(Sext->getType())) {
      NewInst = CopyDebug(new SExtInst(
          NewInst ? NewInst : State.getConverted(Op),
          getPromotedType(cast<IntegerType>(Sext->getType())),
          Sext->getName() + ".sext", Sext), Sext);
    }
    assert(NewInst && "Failed to convert sign extension");
    State.recordConverted(Sext, NewInst);
  } else if (ZExtInst *Zext = dyn_cast<ZExtInst>(Inst)) {
    Value *Op = Zext->getOperand(0);
    Value *NewInst = NULL;
    if (shouldConvert(Op)) {
      NewInst = getClearConverted(Op, Zext, State);
    }
    // If the converted type of the operand is the same as the converted
    // type of the result, we won't actually be changing the type of the
    // variable, just its value.
    if (getPromotedType(Op->getType()) !=
        getPromotedType(Zext->getType())) {
      NewInst = CopyDebug(CastInst::CreateZExtOrBitCast(
          NewInst ? NewInst : State.getConverted(Op),
          getPromotedType(cast<IntegerType>(Zext->getType())),
          "", Zext), Zext);
    }
    assert(NewInst);
    State.recordConverted(Zext, NewInst);
  } else if (TruncInst *Trunc = dyn_cast<TruncInst>(Inst)) {
    Value *Op = Trunc->getOperand(0);
    Value *NewInst;
    // If the converted type of the operand is the same as the converted
    // type of the result, we don't actually need to change the type of the
    // variable, just its value. However, because we don't care about the values
    // of the upper bits until they are consumed, truncation can be a no-op.
    if (getPromotedType(Op->getType()) !=
        getPromotedType(Trunc->getType())) {
      NewInst = CopyDebug(new TruncInst(
          State.getConverted(Op),
          getPromotedType(cast<IntegerType>(Trunc->getType())),
          State.getConverted(Op)->getName() + ".trunc",
          Trunc), Trunc);
    } else {
      NewInst = State.getConverted(Op);
    }
    State.recordConverted(Trunc, NewInst);
  } else if (AllocaInst *Alloc = dyn_cast<AllocaInst>(Inst)) {
    // Don't handle arrays of illegal types, but we could handle an array
    // with size specified as an illegal type, as unlikely as that seems.
    if (shouldConvert(Alloc) && Alloc->isArrayAllocation())
      report_fatal_error("Can't convert arrays of illegal type");
    AllocaInst *NewInst = new AllocaInst(
        getPromotedType(Alloc->getAllocatedType()),
        State.getConverted(Alloc->getArraySize()),
        "", Alloc);
    CopyDebug(NewInst, Alloc);
    NewInst->setAlignment(Alloc->getAlignment());
    State.recordConverted(Alloc, NewInst);
  } else if (BitCastInst *BCInst = dyn_cast<BitCastInst>(Inst)) {
    // Only handle pointers. Ints can't be casted to/from other ints
    Type *DestType = shouldConvert(BCInst) ?
        getPromotedType(BCInst->getDestTy()) : BCInst->getDestTy();
    Instruction *NewInst = CopyDebug(new BitCastInst(
        State.getConverted(BCInst->getOperand(0)),
        DestType,
        "", BCInst), BCInst);
    State.recordConverted(BCInst, NewInst);
  } else if (LoadInst *Load = dyn_cast<LoadInst>(Inst)) {
    if (shouldConvert(Load)) {
      splitLoad(Load, State);
    }
  } else if (StoreInst *Store = dyn_cast<StoreInst>(Inst)) {
    if (shouldConvert(Store->getValueOperand())) {
      splitStore(Store, State);
    }
  } else if (isa<CallInst>(Inst)) {
    report_fatal_error("can't convert calls with illegal types");
  } else if (BinaryOperator *Binop = dyn_cast<BinaryOperator>(Inst)) {
    Value *NewInst = NULL;
    switch (Binop->getOpcode()) {
      case Instruction::AShr: {
        // The AShr operand needs to be sign-extended to the promoted size
        // before shifting. Because the sign-extension is implemented with
        // with AShr, it can be combined with the original operation.
        Value *Op = Binop->getOperand(0);
        Value *ShiftAmount = NULL;
        APInt SignShiftAmt = APInt(
            getPromotedType(Op->getType())->getIntegerBitWidth(),
            getPromotedType(Op->getType())->getIntegerBitWidth() -
            Op->getType()->getIntegerBitWidth());
        NewInst = CopyDebug(BinaryOperator::Create(
            Instruction::Shl,
            State.getConverted(Op),
            ConstantInt::get(getPromotedType(Op->getType()), SignShiftAmt),
            State.getConverted(Op)->getName() + ".getsign",
            Binop), Binop);
        if (ConstantInt *C = dyn_cast<ConstantInt>(
                State.getConverted(Binop->getOperand(1)))) {
          ShiftAmount = ConstantInt::get(getPromotedType(Op->getType()),
                                         SignShiftAmt + C->getValue());
        } else {
          // Clear the upper bits of the original shift amount, and add back the
          // amount we shifted to get the sign bit.
          ShiftAmount = getClearConverted(Binop->getOperand(1), Binop, State);
          ShiftAmount = CopyDebug(BinaryOperator::Create(
              Instruction::Add,
              ShiftAmount,
              ConstantInt::get(
                  getPromotedType(Binop->getOperand(1)->getType()),
                  SignShiftAmt),
              State.getConverted(Op)->getName() + ".shamt", Binop), Binop);
        }
        NewInst = CopyDebug(BinaryOperator::Create(
            Instruction::AShr,
            NewInst,
            ShiftAmount,
            Binop->getName() + ".result", Binop), Binop);
        break;
      }

      case Instruction::LShr:
      case Instruction::Shl: {
        // For LShr, clear the upper bits of the operand before shifting them
        // down into the valid part of the value.
        Value *Op = Binop->getOpcode() == Instruction::LShr
                        ? getClearConverted(Binop->getOperand(0), Binop, State)
                        : State.getConverted(Binop->getOperand(0));
        NewInst = BinaryOperator::Create(
            Binop->getOpcode(), Op,
            // Clear the upper bits of the shift amount.
            getClearConverted(Binop->getOperand(1), Binop, State),
            Binop->getName() + ".result", Binop);
        break;
      }
      case Instruction::Add:
      case Instruction::Sub:
      case Instruction::Mul:
      case Instruction::And:
      case Instruction::Or:
      case Instruction::Xor:
        // These operations don't care about the state of the upper bits.
        NewInst = CopyDebug(BinaryOperator::Create(
            Binop->getOpcode(),
            State.getConverted(Binop->getOperand(0)),
            State.getConverted(Binop->getOperand(1)),
            Binop->getName() + ".result", Binop), Binop);
        break;
      case Instruction::FAdd:
      case Instruction::FSub:
      case Instruction::FMul:
      case Instruction::UDiv:
      case Instruction::SDiv:
      case Instruction::FDiv:
      case Instruction::URem:
      case Instruction::SRem:
      case Instruction::FRem:
      case Instruction::BinaryOpsEnd:
        // We should not see FP operators here.
        // We don't handle div.
        errs() << *Inst << "\n";
        llvm_unreachable("Cannot handle binary operator");
        break;
    }

    if (isa<OverflowingBinaryOperator>(NewInst)) {
      cast<BinaryOperator>(NewInst)->setHasNoUnsignedWrap(
          Binop->hasNoUnsignedWrap());
      cast<BinaryOperator>(NewInst)->setHasNoSignedWrap(
          Binop->hasNoSignedWrap());
    }
    State.recordConverted(Binop, NewInst);
  } else if (ICmpInst *Cmp = dyn_cast<ICmpInst>(Inst)) {
    Value *Op0, *Op1;
    // For signed compares, operands are sign-extended to their
    // promoted type. For unsigned or equality compares, the upper bits are
    // cleared.
    if (Cmp->isSigned()) {
      Op0 = getSignExtend(State.getConverted(Cmp->getOperand(0)),
                          Cmp->getOperand(0),
                          Cmp);
      Op1 = getSignExtend(State.getConverted(Cmp->getOperand(1)),
                          Cmp->getOperand(1),
                          Cmp);
    } else {
      Op0 = getClearConverted(Cmp->getOperand(0), Cmp, State);
      Op1 = getClearConverted(Cmp->getOperand(1), Cmp, State);
    }
    Instruction *NewInst = CopyDebug(new ICmpInst(
        Cmp, Cmp->getPredicate(), Op0, Op1, ""), Cmp);
    State.recordConverted(Cmp, NewInst);
  } else if (SelectInst *Select = dyn_cast<SelectInst>(Inst)) {
    Instruction *NewInst = CopyDebug(SelectInst::Create(
        Select->getCondition(),
        State.getConverted(Select->getTrueValue()),
        State.getConverted(Select->getFalseValue()),
        "", Select), Select);
    State.recordConverted(Select, NewInst);
  } else if (PHINode *Phi = dyn_cast<PHINode>(Inst)) {
    PHINode *NewPhi = PHINode::Create(
        getPromotedType(Phi->getType()),
        Phi->getNumIncomingValues(),
        "", Phi);
    CopyDebug(NewPhi, Phi);
    for (unsigned I = 0, E = Phi->getNumIncomingValues(); I < E; ++I) {
      NewPhi->addIncoming(State.getConverted(Phi->getIncomingValue(I)),
                          Phi->getIncomingBlock(I));
    }
    State.recordConverted(Phi, NewPhi);
  } else if (SwitchInst *Switch = dyn_cast<SwitchInst>(Inst)) {
    Value *Condition = getClearConverted(Switch->getCondition(), Switch, State);
    SwitchInst *NewInst = SwitchInst::Create(
        Condition,
        Switch->getDefaultDest(),
        Switch->getNumCases(),
        Switch);
    CopyDebug(NewInst, Switch);
    for (SwitchInst::CaseIt I = Switch->case_begin(),
             E = Switch->case_end();
         I != E; ++I) {
      // Build a new case from the ranges that map to the successor BB. Each
      // range consists of a high and low value which are typed, so the ranges
      // must be rebuilt and a new case constructed from them.
      IntegersSubset CaseRanges = I.getCaseValueEx();
      IntegersSubsetToBB CaseBuilder;
      for (unsigned RI = 0, RE = CaseRanges.getNumItems(); RI < RE; ++RI) {
        CaseBuilder.add(
            IntItem::fromConstantInt(cast<ConstantInt>(convertConstant(
                CaseRanges.getItem(RI).getLow().toConstantInt()))),
            IntItem::fromConstantInt(cast<ConstantInt>(convertConstant(
                CaseRanges.getItem(RI).getHigh().toConstantInt()))));
      }
      IntegersSubset Case = CaseBuilder.getCase();
      NewInst->addCase(Case, I.getCaseSuccessor());
    }
    Switch->eraseFromParent();
  } else {
    errs() << *Inst<<"\n";
    llvm_unreachable("unhandled instruction");
  }
}

bool PromoteIntegers::runOnFunction(Function &F) {
  DL = &getAnalysis<DataLayout>();
  // Don't support changing the function arguments. This should not be
  // generated by clang.
  for (Function::arg_iterator I = F.arg_begin(), E = F.arg_end(); I != E; ++I) {
    Value *Arg = I;
    if (shouldConvert(Arg)) {
      errs() << "Function " << F.getName() << ": " << *Arg << "\n";
      llvm_unreachable("Function has illegal integer/pointer argument");
    }
  }

  ConversionState State;
  bool Modified = false;
  for (Function::iterator FI = F.begin(), FE = F.end(); FI != FE; ++FI) {
    for (BasicBlock::iterator BBI = FI->begin(), BBE = FI->end(); BBI != BBE;) {
      Instruction *Inst = BBI++;
      // Only attempt to convert an instruction if its result or any of its
      // operands are illegal.
      bool ShouldConvert = shouldConvert(Inst);
      for (User::op_iterator OI = Inst->op_begin(), OE = Inst->op_end();
           OI != OE; ++OI)
        ShouldConvert |= shouldConvert(cast<Value>(OI));

      if (ShouldConvert) {
        convertInstruction(Inst, State);
        Modified = true;
      }
    }
  }
  State.eraseReplacedInstructions();
  return Modified;
}

FunctionPass *llvm::createPromoteIntegersPass() {
  return new PromoteIntegers();
}
