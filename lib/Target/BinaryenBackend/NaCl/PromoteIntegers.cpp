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
// Legal sizes are currently 1, 8, and large power-of-two sizes. Operations on
// illegal integers are changed to operate on the next-higher legal size.
//
// It maintains no invariants about the upper bits (above the size of the
// original type); therefore before operations which can be affected by the
// value of these bits (e.g. cmp, select, lshr), the upper bits of the operands
// are cleared.
//
// Limitations:
// 1) It can't change function signatures or global variables
// 2) Doesn't handle arrays or structs with illegal types
// 3) Doesn't handle constant expressions (it also doesn't produce them, so it
//    can run after ExpandConstantExpr)
//
//===----------------------------------------------------------------------===//

#include "SimplifiedFuncTypeMap.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

static Type *getPromotedType(Type *Ty);

namespace {

class TypeMap : public SimplifiedFuncTypeMap {
protected:
  MappingResult getSimpleFuncType(LLVMContext &Ctx, StructMap &Tentatives,
                                  FunctionType *OldFnTy) override {
    ParamTypeVector NewArgTypes;

    auto Ret = getPromotedArgType(Ctx, OldFnTy->getReturnType(), Tentatives);
    bool Changed = Ret.isChanged();
    for (auto &ArgTy : OldFnTy->params()) {
      auto NewArgTy = getPromotedArgType(Ctx, ArgTy, Tentatives);
      NewArgTypes.push_back(NewArgTy);
      Changed |= NewArgTy.isChanged();
    }

    auto *NewFctType = FunctionType::get(Ret, NewArgTypes, OldFnTy->isVarArg());
    return {NewFctType, Changed};
  }

private:
  MappingResult getPromotedArgType(LLVMContext &Ctx, Type *Ty,
                                   StructMap &Tentatives) {
    if (Ty->isIntegerTy()) {
      auto *NTy = getPromotedType(Ty);
      return {NTy, NTy != Ty};
    }
    return getSimpleAggregateTypeInternal(Ctx, Ty, Tentatives);
  }
};

class PromoteIntegers : public ModulePass {
public:
  static char ID;

  PromoteIntegers() : ModulePass(ID) {
    initializePromoteIntegersPass(*PassRegistry::getPassRegistry());
  }

  bool runOnModule(Module &M) override;

private:
  typedef DenseMap<const llvm::Function *, DISubprogram *> DebugMap;
  TypeMap TypeMapper;

  bool ensureCompliantSignature(LLVMContext &Ctx, Function *OldFct, Module &M);
};
} // anonymous namespace

char PromoteIntegers::ID = 0;

INITIALIZE_PASS(PromoteIntegers, "nacl-promote-ints",
                "Promote integer types which are illegal in PNaCl", false,
                false)

static bool isLegalSize(unsigned Size) {
  return Size == 1 || (Size >= 8 && isPowerOf2_32(Size));
}

static Type *getPromotedIntType(IntegerType *Ty) {
  auto Width = Ty->getBitWidth();
  if (isLegalSize(Width))
    return Ty;
  assert(Width < (1ull << (sizeof(Width) * CHAR_BIT - 1)) &&
         "width can't be rounded to the next power of two");
  return IntegerType::get(Ty->getContext(),
                          Width < 8 ? 8 : NextPowerOf2(Width));
}

// Return a legal integer type, promoting to a larger size if necessary.
static Type *getPromotedType(Type *Ty) {
  assert(isa<IntegerType>(Ty) && "Trying to convert a non-integer type");
  return getPromotedIntType(cast<IntegerType>(Ty));
}

// Return true if Val is an int which should be converted.
static bool shouldConvert(Value *Val) {
  if (auto *ITy = dyn_cast<IntegerType>(Val->getType()))
    return !isLegalSize(ITy->getBitWidth());
  return false;
}

// Return a constant which has been promoted to a legal size.
static Value *convertConstant(Constant *C, bool SignExt) {
  assert(shouldConvert(C));
  Type *ProTy = getPromotedType(C->getType());
  // ConstantExpr of a Constant yields a Constant, not a ConstantExpr.
  return SignExt ? ConstantExpr::getSExt(C, ProTy)
                 : ConstantExpr::getZExt(C, ProTy);
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
//
// recordConverted also queues the old value for deletion.
//
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

    // Directly convert constants.
    if (auto *C = dyn_cast<Constant>(Val))
      return convertConstant(C, /*SignExt=*/false);

    // No converted value available yet, so create a placeholder.
    auto *P = new Argument(getPromotedType(Val->getType()));

    RewrittenMap[Val] = P;
    Placeholders[Val] = P;
    return P;
  }

  // Replace the uses of From with To, replace the uses of any
  // placeholders for From, and optionally give From's name to To.
  // Also mark To for deletion.
  void recordConverted(Instruction *From, Value *To, bool TakeName = true) {
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
    for (Instruction *E : ToErase)
      E->dropAllReferences();
    for (Instruction *E : ToErase)
      E->eraseFromParent();
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

// Create a BitCast instruction from the original Value being cast. These
// instructions aren't replaced by convertInstruction because they are pointer
// types (which are always valid), but their uses eventually lead to an invalid
// type.
static Value *CreateBitCast(IRBuilder<> *IRB, Value *From, Type *ToTy,
                            const Twine &Name) {
  if (auto *BC = dyn_cast<BitCastInst>(From))
    return CreateBitCast(IRB, BC->getOperand(0), ToTy, Name);
  return IRB->CreateBitCast(From, ToTy, Name);
}

// Split an illegal load into multiple legal loads and return the resulting
// promoted value. The size of the load is assumed to be a multiple of 8.
//
// \param BaseAlign Alignment of the base load.
// \param Offset    Offset from the base load.
static Value *splitLoad(DataLayout *DL, LoadInst *Inst, ConversionState &State,
                        unsigned BaseAlign, unsigned Offset) {
  if (Inst->isVolatile() || Inst->isAtomic())
    report_fatal_error("Can't split volatile/atomic loads");
  if (DL->getTypeSizeInBits(Inst->getType()) % 8 != 0)
    report_fatal_error("Loads must be a multiple of 8 bits");

  auto *OrigPtr = State.getConverted(Inst->getPointerOperand());
  // OrigPtr is a placeholder in recursive calls, and so has no name.
  if (OrigPtr->getName().empty())
    OrigPtr->setName(Inst->getPointerOperand()->getName());
  unsigned Width = DL->getTypeSizeInBits(Inst->getType());
  auto *NewType = getPromotedType(Inst->getType());
  unsigned LoWidth = PowerOf2Floor(Width);
  assert(isLegalSize(LoWidth));

  auto *LoType = IntegerType::get(Inst->getContext(), LoWidth);
  auto *HiType = IntegerType::get(Inst->getContext(), Width - LoWidth);
  IRBuilder<> IRB(Inst);

  auto *BCLo = CreateBitCast(&IRB, OrigPtr, LoType->getPointerTo(),
                             OrigPtr->getName() + ".loty");
  auto *LoadLo = IRB.CreateAlignedLoad(BCLo, MinAlign(BaseAlign, Offset),
                                       Inst->getName() + ".lo");
  auto *LoExt = IRB.CreateZExt(LoadLo, NewType, LoadLo->getName() + ".ext");
  auto *GEPHi = IRB.CreateConstGEP1_32(BCLo, 1, OrigPtr->getName() + ".hi");
  auto *BCHi = CreateBitCast(&IRB, GEPHi, HiType->getPointerTo(),
                             OrigPtr->getName() + ".hity");

  auto HiOffset = (Offset + LoWidth) / CHAR_BIT;
  auto *LoadHi = IRB.CreateAlignedLoad(BCHi, MinAlign(BaseAlign, HiOffset),
                                       Inst->getName() + ".hi");
  auto *Hi = !isLegalSize(Width - LoWidth)
                 ? splitLoad(DL, LoadHi, State, BaseAlign, HiOffset)
                 : LoadHi;

  auto *HiExt = IRB.CreateZExt(Hi, NewType, Hi->getName() + ".ext");
  auto *HiShift = IRB.CreateShl(HiExt, LoWidth, HiExt->getName() + ".sh");
  auto *Result = IRB.CreateOr(LoExt, HiShift);

  State.recordConverted(Inst, Result);

  return Result;
}

static Value *splitStore(DataLayout *DL, StoreInst *Inst,
                         ConversionState &State, unsigned BaseAlign,
                         unsigned Offset) {
  if (Inst->isVolatile() || Inst->isAtomic())
    report_fatal_error("Can't split volatile/atomic stores");
  if (DL->getTypeSizeInBits(Inst->getValueOperand()->getType()) % 8 != 0)
    report_fatal_error("Stores must be a multiple of 8 bits");

  auto *OrigPtr = State.getConverted(Inst->getPointerOperand());
  // OrigPtr is now a placeholder in recursive calls, and so has no name.
  if (OrigPtr->getName().empty())
    OrigPtr->setName(Inst->getPointerOperand()->getName());
  auto *OrigVal = State.getConverted(Inst->getValueOperand());
  unsigned Width = DL->getTypeSizeInBits(Inst->getValueOperand()->getType());
  unsigned LoWidth = PowerOf2Floor(Width);
  assert(isLegalSize(LoWidth));

  auto *LoType = IntegerType::get(Inst->getContext(), LoWidth);
  auto *HiType = IntegerType::get(Inst->getContext(), Width - LoWidth);
  IRBuilder<> IRB(Inst);

  auto *BCLo = CreateBitCast(&IRB, OrigPtr, LoType->getPointerTo(),
                             OrigPtr->getName() + ".loty");
  auto *LoTrunc = IRB.CreateTrunc(OrigVal, LoType, OrigVal->getName() + ".lo");
  IRB.CreateAlignedStore(LoTrunc, BCLo, MinAlign(BaseAlign, Offset));

  auto HiOffset = (Offset + LoWidth) / CHAR_BIT;
  auto *HiLShr =
      IRB.CreateLShr(OrigVal, LoWidth, OrigVal->getName() + ".hi.sh");
  auto *GEPHi = IRB.CreateConstGEP1_32(BCLo, 1, OrigPtr->getName() + ".hi");
  auto *HiTrunc = IRB.CreateTrunc(HiLShr, HiType, OrigVal->getName() + ".hi");
  auto *BCHi = CreateBitCast(&IRB, GEPHi, HiType->getPointerTo(),
                             OrigPtr->getName() + ".hity");

  auto *StoreHi =
      IRB.CreateAlignedStore(HiTrunc, BCHi, MinAlign(BaseAlign, HiOffset));
  Value *Hi = StoreHi;

  if (!isLegalSize(Width - LoWidth)) {
    // HiTrunc is still illegal, and is redundant with the truncate in the
    // recursive call, so just get rid of it. If HiTrunc is a constant then the
    // IRB will have just returned a shifted, truncated constant, which is
    // already uniqued (and does not need to be RAUWed), and recordConverted
    // expects constants.
    if (!isa<Constant>(HiTrunc))
      State.recordConverted(cast<Instruction>(HiTrunc), HiLShr,
                            /*TakeName=*/false);
    Hi = splitStore(DL, StoreHi, State, BaseAlign, HiOffset);
  }
  State.recordConverted(Inst, Hi, /*TakeName=*/false);
  return Hi;
}

// Return a converted value with the bits of the operand above the size of the
// original type cleared.
static Value *getClearConverted(Value *Operand, Instruction *InsertPt,
                                ConversionState &State) {
  auto *OrigType = Operand->getType();
  auto *OrigInst = dyn_cast<Instruction>(Operand);
  Operand = State.getConverted(Operand);
  // If the operand is a constant, it will have been created by
  // ConversionState.getConverted, which zero-extends by default.
  if (isa<Constant>(Operand))
    return Operand;
  Instruction *NewInst = BinaryOperator::Create(
      Instruction::And, Operand,
      ConstantInt::get(
          getPromotedType(OrigType),
          APInt::getLowBitsSet(getPromotedType(OrigType)->getIntegerBitWidth(),
                               OrigType->getIntegerBitWidth())),
      Operand->getName() + ".clear", InsertPt);
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
  ConstantInt *ShiftAmt =
      ConstantInt::getSigned(cast<IntegerType>(getPromotedType(OrigType)),
                             getPromotedType(OrigType)->getIntegerBitWidth() -
                                 OrigType->getIntegerBitWidth());
  BinaryOperator *Shl =
      BinaryOperator::Create(Instruction::Shl, Operand, ShiftAmt,
                             Operand->getName() + ".getsign", InsertPt);
  if (Instruction *Inst = dyn_cast<Instruction>(OrigOperand))
    CopyDebug(Shl, Inst);
  return CopyDebug(BinaryOperator::Create(Instruction::AShr, Shl, ShiftAmt,
                                          Operand->getName() + ".signed",
                                          InsertPt),
                   Shl);
}

static void convertInstruction(DataLayout *DL, Instruction *Inst,
                               ConversionState &State) {
  if (SExtInst *Sext = dyn_cast<SExtInst>(Inst)) {
    Value *Op = Sext->getOperand(0);
    Value *NewInst = nullptr;
    // If the operand to be extended is illegal, we first need to fill its
    // upper bits with its sign bit.
    if (shouldConvert(Op)) {
      NewInst = getSignExtend(State.getConverted(Op), Op, Sext);
    }
    // If the converted type of the operand is the same as the converted
    // type of the result, we won't actually be changing the type of the
    // variable, just its value.
    if (getPromotedType(Op->getType()) != getPromotedType(Sext->getType())) {
      NewInst = CopyDebug(
          new SExtInst(NewInst ? NewInst : State.getConverted(Op),
                       getPromotedType(cast<IntegerType>(Sext->getType())),
                       Sext->getName() + ".sext", Sext),
          Sext);
    }
    assert(NewInst && "Failed to convert sign extension");
    State.recordConverted(Sext, NewInst);
  } else if (ZExtInst *Zext = dyn_cast<ZExtInst>(Inst)) {
    Value *Op = Zext->getOperand(0);
    Value *NewInst = nullptr;
    if (shouldConvert(Op)) {
      NewInst = getClearConverted(Op, Zext, State);
    }
    // If the converted type of the operand is the same as the converted
    // type of the result, we won't actually be changing the type of the
    // variable, just its value.
    if (getPromotedType(Op->getType()) != getPromotedType(Zext->getType())) {
      NewInst = CopyDebug(
          CastInst::CreateZExtOrBitCast(
              NewInst ? NewInst : State.getConverted(Op),
              getPromotedType(cast<IntegerType>(Zext->getType())), "", Zext),
          Zext);
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
    if (getPromotedType(Op->getType()) != getPromotedType(Trunc->getType())) {
      NewInst = CopyDebug(
          new TruncInst(State.getConverted(Op),
                        getPromotedType(cast<IntegerType>(Trunc->getType())),
                        State.getConverted(Op)->getName() + ".trunc", Trunc),
          Trunc);
    } else {
      NewInst = State.getConverted(Op);
    }
    State.recordConverted(Trunc, NewInst);
  } else if (LoadInst *Load = dyn_cast<LoadInst>(Inst)) {
    if (shouldConvert(Load)) {
      unsigned BaseAlign = Load->getAlignment() == 0
                               ? DL->getABITypeAlignment(Load->getType())
                               : Load->getAlignment();
      splitLoad(DL, Load, State, BaseAlign, /*Offset=*/0);
    }
  } else if (StoreInst *Store = dyn_cast<StoreInst>(Inst)) {
    if (shouldConvert(Store->getValueOperand())) {
      unsigned BaseAlign =
          Store->getAlignment() == 0
              ? DL->getABITypeAlignment(Store->getValueOperand()->getType())
              : Store->getAlignment();
      splitStore(DL, Store, State, BaseAlign, /*Offset=*/0);
    }
  } else if (isa<InvokeInst>(Inst) || isa<CallInst>(Inst) ||
             isa<LandingPadInst>(Inst)) {
    for (unsigned I = 0; I < Inst->getNumOperands(); I++) {
      auto *Arg = Inst->getOperand(I);
      if (shouldConvert(Arg))
        Inst->setOperand(I, State.getConverted(Arg));
    }
    if (shouldConvert(Inst)) {
      Inst->mutateType(getPromotedType(Inst->getType()));
    }
  } else if (auto *Ret = dyn_cast<ReturnInst>(Inst)) {
    auto *NewRet = ReturnInst::Create(
        Ret->getContext(), State.getConverted(Ret->getReturnValue()), Inst);
    State.recordConverted(Ret, NewRet);
  } else if (auto *Resume = dyn_cast<ResumeInst>(Inst)) {
    auto *NewRes =
        ResumeInst::Create(State.getConverted(Resume->getValue()), Inst);
    State.recordConverted(Ret, NewRes);
  } else if (BinaryOperator *Binop = dyn_cast<BinaryOperator>(Inst)) {
    Value *NewInst = nullptr;
    switch (Binop->getOpcode()) {
    case Instruction::AShr: {
      // The AShr operand needs to be sign-extended to the promoted size
      // before shifting. Because the sign-extension is implemented with
      // with AShr, it can be combined with the original operation.
      Value *Op = Binop->getOperand(0);
      Value *ShiftAmount = nullptr;
      APInt SignShiftAmt =
          APInt(getPromotedType(Op->getType())->getIntegerBitWidth(),
                getPromotedType(Op->getType())->getIntegerBitWidth() -
                    Op->getType()->getIntegerBitWidth());
      NewInst = CopyDebug(
          BinaryOperator::Create(
              Instruction::Shl, State.getConverted(Op),
              ConstantInt::get(getPromotedType(Op->getType()), SignShiftAmt),
              State.getConverted(Op)->getName() + ".getsign", Binop),
          Binop);
      if (ConstantInt *C =
              dyn_cast<ConstantInt>(State.getConverted(Binop->getOperand(1)))) {
        ShiftAmount = ConstantInt::get(getPromotedType(Op->getType()),
                                       SignShiftAmt + C->getValue());
      } else {
        // Clear the upper bits of the original shift amount, and add back the
        // amount we shifted to get the sign bit.
        ShiftAmount = getClearConverted(Binop->getOperand(1), Binop, State);
        ShiftAmount =
            CopyDebug(BinaryOperator::Create(
                          Instruction::Add, ShiftAmount,
                          ConstantInt::get(
                              getPromotedType(Binop->getOperand(1)->getType()),
                              SignShiftAmt),
                          State.getConverted(Op)->getName() + ".shamt", Binop),
                      Binop);
      }
      NewInst = CopyDebug(
          BinaryOperator::Create(Instruction::AShr, NewInst, ShiftAmount,
                                 Binop->getName() + ".result", Binop),
          Binop);
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
      NewInst = CopyDebug(
          BinaryOperator::Create(Binop->getOpcode(),
                                 State.getConverted(Binop->getOperand(0)),
                                 State.getConverted(Binop->getOperand(1)),
                                 Binop->getName() + ".result", Binop),
          Binop);
      break;
    case Instruction::UDiv:
    case Instruction::URem:
      NewInst =
          CopyDebug(BinaryOperator::Create(
                        Binop->getOpcode(),
                        getClearConverted(Binop->getOperand(0), Binop, State),
                        getClearConverted(Binop->getOperand(1), Binop, State),
                        Binop->getName() + ".result", Binop),
                    Binop);
      break;
    case Instruction::SDiv:
    case Instruction::SRem:
      NewInst =
          CopyDebug(BinaryOperator::Create(
                        Binop->getOpcode(),
                        getSignExtend(State.getConverted(Binop->getOperand(0)),
                                      Binop->getOperand(0), Binop),
                        getSignExtend(State.getConverted(Binop->getOperand(1)),
                                      Binop->getOperand(0), Binop),
                        Binop->getName() + ".result", Binop),
                    Binop);
      break;
    case Instruction::FAdd:
    case Instruction::FSub:
    case Instruction::FMul:
    case Instruction::FDiv:
    case Instruction::FRem:
    case Instruction::BinaryOpsEnd:
      // We should not see FP operators here.
      errs() << *Inst << "\n";
      llvm_unreachable("Cannot handle binary operator");
      break;
    }
    if (isa<OverflowingBinaryOperator>(NewInst)) {
      cast<BinaryOperator>(NewInst)
          ->setHasNoUnsignedWrap(Binop->hasNoUnsignedWrap());
      cast<BinaryOperator>(NewInst)
          ->setHasNoSignedWrap(Binop->hasNoSignedWrap());
    }
    State.recordConverted(Binop, NewInst);
  } else if (ICmpInst *Cmp = dyn_cast<ICmpInst>(Inst)) {
    Value *Op0, *Op1;
    // For signed compares, operands are sign-extended to their
    // promoted type. For unsigned or equality compares, the upper bits are
    // cleared.
    if (Cmp->isSigned()) {
      Op0 = getSignExtend(State.getConverted(Cmp->getOperand(0)),
                          Cmp->getOperand(0), Cmp);
      Op1 = getSignExtend(State.getConverted(Cmp->getOperand(1)),
                          Cmp->getOperand(1), Cmp);
    } else {
      Op0 = getClearConverted(Cmp->getOperand(0), Cmp, State);
      Op1 = getClearConverted(Cmp->getOperand(1), Cmp, State);
    }
    Instruction *NewInst =
        CopyDebug(new ICmpInst(Cmp, Cmp->getPredicate(), Op0, Op1, ""), Cmp);
    State.recordConverted(Cmp, NewInst);
  } else if (SelectInst *Select = dyn_cast<SelectInst>(Inst)) {
    Instruction *NewInst = CopyDebug(
        SelectInst::Create(
            Select->getCondition(), State.getConverted(Select->getTrueValue()),
            State.getConverted(Select->getFalseValue()), "", Select),
        Select);
    State.recordConverted(Select, NewInst);
  } else if (PHINode *Phi = dyn_cast<PHINode>(Inst)) {
    PHINode *NewPhi = PHINode::Create(getPromotedType(Phi->getType()),
                                      Phi->getNumIncomingValues(), "", Phi);
    CopyDebug(NewPhi, Phi);
    for (unsigned I = 0, E = Phi->getNumIncomingValues(); I < E; ++I) {
      NewPhi->addIncoming(State.getConverted(Phi->getIncomingValue(I)),
                          Phi->getIncomingBlock(I));
    }
    State.recordConverted(Phi, NewPhi);
  } else if (SwitchInst *Switch = dyn_cast<SwitchInst>(Inst)) {
    Value *Condition = getClearConverted(Switch->getCondition(), Switch, State);
    SwitchInst *NewInst = SwitchInst::Create(
        Condition, Switch->getDefaultDest(), Switch->getNumCases(), Switch);
    CopyDebug(NewInst, Switch);
    for (SwitchInst::CaseIt I = Switch->case_begin(), E = Switch->case_end();
         I != E; ++I) {
      NewInst->addCase(cast<ConstantInt>(convertConstant(I.getCaseValue(),
                                                         /*SignExt=*/false)),
                       I.getCaseSuccessor());
    }
    Switch->eraseFromParent();
  } else {
    errs() << *Inst << "\n";
    llvm_unreachable("unhandled instruction");
  }
}

static bool processFunction(Function &F, DataLayout &DL) {
  ConversionState State;
  bool Modified = false; // XXX Emscripten: Fixed use of an uninitialized variable.
  for (auto FI = F.begin(), FE = F.end(); FI != FE; ++FI) {
    for (auto BBI = FI->begin(), BBE = FI->end(); BBI != BBE;) {
      Instruction *Inst = &*BBI++;
      // Only attempt to convert an instruction if its result or any of its
      // operands are illegal.
      bool ShouldConvert = shouldConvert(Inst);
      for (auto OI = Inst->op_begin(), OE = Inst->op_end(); OI != OE; ++OI)
        ShouldConvert |= shouldConvert(cast<Value>(OI));

      if (ShouldConvert) {
        convertInstruction(&DL, Inst, State);
        Modified = true;
      }
    }
  }
  State.eraseReplacedInstructions();

  if (Modified)
    // Clean up bitcasts that were create with constexprs in them.
    std::unique_ptr<FunctionPass>(createExpandConstantExprPass())
        ->runOnFunction(F);
  return Modified;
}

bool PromoteIntegers::ensureCompliantSignature(
    LLVMContext &Ctx, Function *OldFct, Module &M) {

  auto *NewFctType = cast<FunctionType>(
      TypeMapper.getSimpleType(Ctx, OldFct->getFunctionType()));
  if (NewFctType == OldFct->getFunctionType())
    return false;

  auto *NewFct = Function::Create(NewFctType, OldFct->getLinkage(), "", &M);

  NewFct->takeName(OldFct);
  NewFct->copyAttributesFrom(OldFct);
  for (auto UseIter = OldFct->use_begin(), E = OldFct->use_end();
       E != UseIter;) {
    Use &FctUse = *(UseIter++);
    // Types are not going to match after this.
    FctUse.set(NewFct);
  }

  if (OldFct->empty())
    return true;

  NewFct->getBasicBlockList().splice(NewFct->begin(),
                                     OldFct->getBasicBlockList());
  IRBuilder<> Builder(&*NewFct->getEntryBlock().getFirstInsertionPt());

  auto OldArgIter = OldFct->getArgumentList().begin();
  for (auto &NewArg : NewFct->getArgumentList()) {
    Argument *OldArg = &*OldArgIter++;

    if (OldArg->getType() != NewArg.getType()) {
      if (NewArg.getType()->isIntegerTy()) {
        auto *Replacement = Builder.CreateTrunc(&NewArg, OldArg->getType());
        Replacement->takeName(OldArg);
        NewArg.setName(Replacement->getName() + ".exp");
        OldArg->replaceAllUsesWith(Replacement);
      } else {
        // Blindly replace the type of the uses, this is some composite
        // like a function type.
        NewArg.takeName(OldArg);
        for (auto UseIter = OldArg->use_begin(), E = OldArg->use_end();
             E != UseIter;) {
          Use &AUse = *(UseIter++);
          AUse.set(&NewArg);
        }
      }
    } else {
      NewArg.takeName(OldArg);
      OldArg->replaceAllUsesWith(&NewArg);
    }
  }

  return true;
}

bool PromoteIntegers::runOnModule(Module &M) {
  DataLayout DL(&M);
  LLVMContext &Ctx = M.getContext();
  bool Modified = false;

  // Change function signatures first.
  for (auto I = M.begin(), E = M.end(); I != E;) {
    Function *F = &*I++;
    bool Changed = ensureCompliantSignature(Ctx, F, M);
    if (Changed)
      F->eraseFromParent();
    Modified |= Changed;
  }

  for (auto &F : M.getFunctionList())
    Modified |= processFunction(F, DL);

  return Modified;
}

ModulePass *llvm::createPromoteIntegersPass() { return new PromoteIntegers(); }
