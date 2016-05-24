//===- ExpandArithWithOverflow.cpp - Expand out uses of *.with.overflow----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// The llvm.*.with.overflow.*() intrinsics are awkward for PNaCl support because
// they return structs, and we want to omit struct types from IR in PNaCl's
// stable ABI.
//
// However, llvm.{umul,uadd}.with.overflow.*() are used by Clang to implement an
// overflow check for C++'s new[] operator, and {sadd,ssub} are used by
// ubsan. This pass expands out these uses so that PNaCl does not have to
// support *.with.overflow as part of PNaCl's stable ABI.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/APInt.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/NaCl.h"

#define DEBUG_TYPE "expand-arith-with-overflow"

using namespace llvm;

namespace {
class ExpandArithWithOverflow : public ModulePass {
public:
  static char ID;
  ExpandArithWithOverflow() : ModulePass(ID) {
    initializeExpandArithWithOverflowPass(*PassRegistry::getPassRegistry());
  }
  virtual bool runOnModule(Module &M);
};
}

char ExpandArithWithOverflow::ID = 0;
INITIALIZE_PASS(ExpandArithWithOverflow, "expand-arith-with-overflow",
                "Expand out some uses of *.with.overflow intrinsics", false,
                false)

enum class ExpandArith { Add, Sub, Mul };
static const ExpandArith ExpandArithOps[] = {ExpandArith::Add, ExpandArith::Sub,
                                             ExpandArith::Mul};

static Intrinsic::ID getID(ExpandArith Op, bool Signed) {
  static const Intrinsic::ID IDs[][2] = {
      //         Unsigned                       Signed
      /* Add */ {Intrinsic::uadd_with_overflow, Intrinsic::sadd_with_overflow},
      /* Sub */ {Intrinsic::usub_with_overflow, Intrinsic::ssub_with_overflow},
      /* Mul */ {Intrinsic::umul_with_overflow, Intrinsic::smul_with_overflow},
  };
  return IDs[(size_t)Op][Signed];
}

static Instruction::BinaryOps getOpcode(ExpandArith Op) {
  static const Instruction::BinaryOps Opcodes[] = {
      Instruction::Add, Instruction::Sub, Instruction::Mul,
  };
  return Opcodes[(size_t)Op];
}

static Value *CreateInsertValue(IRBuilder<> *IRB, Value *StructVal,
                                unsigned Index, Value *Field,
                                Instruction *BasedOn) {
  SmallVector<unsigned, 1> EVIndexes(1, Index);
  return IRB->CreateInsertValue(StructVal, Field, EVIndexes,
                                BasedOn->getName() + ".insert");
}

static bool Expand(Module *M, unsigned Bits, ExpandArith Op, bool Signed) {
  IntegerType *IntTy = IntegerType::get(M->getContext(), Bits);
  SmallVector<Type *, 1> Types(1, IntTy);
  Function *Intrinsic =
      M->getFunction(Intrinsic::getName(getID(Op, Signed), Types));
  if (!Intrinsic)
    return false;

  SmallVector<CallInst *, 64> Calls;
  for (User *U : Intrinsic->users())
    if (CallInst *Call = dyn_cast<CallInst>(U)) {
      Calls.push_back(Call);
    } else {
      errs() << "User: " << *U << "\n";
      report_fatal_error("ExpandArithWithOverflow: Taking the address of a "
                         "*.with.overflow intrinsic is not allowed");
    }

  for (CallInst *Call : Calls) {
    DEBUG(dbgs() << "Expanding " << *Call << "\n");

    StringRef Name = Call->getName();
    Value *LHS;
    Value *RHS;
    Value *NonConstOperand;
    ConstantInt *ConstOperand;
    bool hasConstOperand;

    if (ConstantInt *C = dyn_cast<ConstantInt>(Call->getArgOperand(0))) {
      LHS = ConstOperand = C;
      RHS = NonConstOperand = Call->getArgOperand(1);
      hasConstOperand = true;
    } else if (ConstantInt *C = dyn_cast<ConstantInt>(Call->getArgOperand(1))) {
      LHS = NonConstOperand = Call->getArgOperand(0);
      RHS = ConstOperand = C;
      hasConstOperand = true;
    } else {
      LHS = Call->getArgOperand(0);
      RHS = Call->getArgOperand(1);
      hasConstOperand = false;
    }

    IRBuilder<> IRB(Call);
    Value *ArithResult =
        IRB.CreateBinOp(getOpcode(Op), LHS, RHS, Name + ".arith");
    Value *OverflowResult;

    if (ExpandArith::Mul == Op && hasConstOperand &&
        ConstOperand->getValue() == 0) {
      // Mul by zero never overflows but can divide by zero.
      OverflowResult = ConstantInt::getFalse(M->getContext());
    } else if (hasConstOperand && !Signed && ExpandArith::Sub != Op) {
      // Unsigned add & mul with a constant operand can be optimized.
      uint64_t ArgMax =
          (ExpandArith::Mul == Op
               ? APInt::getMaxValue(Bits).udiv(ConstOperand->getValue())
               : APInt::getMaxValue(Bits) - ConstOperand->getValue())
              .getLimitedValue();
      OverflowResult =
          IRB.CreateICmp(CmpInst::ICMP_UGT, NonConstOperand,
                         ConstantInt::get(IntTy, ArgMax), Name + ".overflow");
    } else if (ExpandArith::Mul == Op) {
      // Dividing the result by one of the operands should yield the other
      // operand if there was no overflow. Note that this division can't
      // overflow (signed division of INT_MIN / -1 overflows but can't occur
      // here), but it could divide by 0 in which case we instead divide by 1
      // (this case didn't overflow).
      //
      // FIXME: This approach isn't optimal because it's better to perform a
      // wider multiplication and mask off the result, or perform arithmetic on
      // the component pieces.
      auto DivOp = Signed ? Instruction::SDiv : Instruction::UDiv;
      auto DenomIsZero =
          IRB.CreateICmp(CmpInst::ICMP_EQ, RHS,
                         ConstantInt::get(RHS->getType(), 0), Name + ".iszero");
      auto Denom =
          IRB.CreateSelect(DenomIsZero, ConstantInt::get(RHS->getType(), 1),
                           RHS, Name + ".denom");
      auto Div = IRB.CreateBinOp(DivOp, ArithResult, Denom, Name + ".div");
      OverflowResult = IRB.CreateSelect(
          DenomIsZero, ConstantInt::getFalse(M->getContext()),
          IRB.CreateICmp(CmpInst::ICMP_NE, Div, LHS, Name + ".same"),
          Name + ".overflow");
    } else {
      if (!Signed) {
        switch (Op) {
        case ExpandArith::Add:
          // Overflow occurs if unsigned x+y < x (or y). We only need to compare
          // with one of them because this is unsigned arithmetic: on overflow
          // the result is smaller than both inputs, and when there's no
          // overflow the result is greater than both inputs.
          OverflowResult = IRB.CreateICmp(CmpInst::ICMP_ULT, ArithResult, LHS,
                                          Name + ".overflow");
          break;
        case ExpandArith::Sub:
          // Overflow occurs if x < y.
          OverflowResult =
              IRB.CreateICmp(CmpInst::ICMP_ULT, LHS, RHS, Name + ".overflow");
          break;
        case ExpandArith::Mul: // This is handled above.
          llvm_unreachable("Unsigned variable saturating multiplication");
        }
      } else {
        // In the signed case, we care if the sum is >127 or <-128. When looked
        // at as an unsigned number, that is precisely when the sum is >= 128.
        Value *PositiveTemp = IRB.CreateBinOp(
            Instruction::Add, LHS,
            ConstantInt::get(IntTy, APInt::getSignedMinValue(Bits) +
                                        (ExpandArith::Sub == Op ? 1 : 0)),
            Name + ".postemp");
        Value *NegativeTemp = IRB.CreateBinOp(
            Instruction::Add, LHS,
            ConstantInt::get(IntTy, APInt::getSignedMaxValue(Bits) +
                                        (ExpandArith::Sub == Op ? 1 : 0)),
            Name + ".negtemp");
        Value *PositiveCheck = IRB.CreateICmp(CmpInst::ICMP_SLT, ArithResult,
                                              PositiveTemp, Name + ".poscheck");
        Value *NegativeCheck = IRB.CreateICmp(CmpInst::ICMP_SGT, ArithResult,
                                              NegativeTemp, Name + ".negcheck");
        Value *IsPositive =
            IRB.CreateICmp(CmpInst::ICMP_SGE, LHS, ConstantInt::get(IntTy, 0),
                           Name + ".ispos");
        OverflowResult = IRB.CreateSelect(IsPositive, PositiveCheck,
                                          NegativeCheck, Name + ".select");
      }
    }

    // Construct the struct result.
    Value *NewStruct = UndefValue::get(Call->getType());
    NewStruct = CreateInsertValue(&IRB, NewStruct, 0, ArithResult, Call);
    NewStruct = CreateInsertValue(&IRB, NewStruct, 1, OverflowResult, Call);
    Call->replaceAllUsesWith(NewStruct);
    Call->eraseFromParent();
  }

  Intrinsic->eraseFromParent();
  return true;
}

static const unsigned MaxBits = 64;

bool ExpandArithWithOverflow::runOnModule(Module &M) {
  bool Modified = false;
  for (ExpandArith Op : ExpandArithOps)
    for (int Signed = false; Signed <= true; ++Signed)
      for (unsigned Bits = 8; Bits <= MaxBits; Bits <<= 1)
        Modified |= Expand(&M, Bits, Op, Signed);
  return Modified;
}

ModulePass *llvm::createExpandArithWithOverflowPass() {
  return new ExpandArithWithOverflow();
}
