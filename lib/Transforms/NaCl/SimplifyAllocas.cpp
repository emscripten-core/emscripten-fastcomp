//===- SimplifyAllocas.cpp - Simplify allocas to arrays of bytes         --===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Simplify all allocas into allocas of byte arrays.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;
namespace {
class SimplifyAllocas : public BasicBlockPass {
public:
  static char ID; // Pass identification, replacement for typeid
  SimplifyAllocas()
      : BasicBlockPass(ID), Initialized(false), M(nullptr), IntPtrType(nullptr),
        Int8Type(nullptr), DL(nullptr) {
    initializeSimplifyAllocasPass(*PassRegistry::getPassRegistry());
  }

private:
  bool Initialized;
  const Module *M;
  Type *IntPtrType;
  Type *Int8Type;
  const DataLayout *DL;

  using llvm::Pass::doInitialization;
  bool doInitialization(Function &F) override {
    if (!Initialized) {
      M = F.getParent();
      DL = M->getDataLayout();
      IntPtrType = DL->getIntPtrType(M->getContext());
      Int8Type = Type::getInt8Ty(M->getContext());
      Initialized = true;
      return true;
    }
    return false;
  }

  bool runOnBasicBlock(BasicBlock &BB) override {
    bool Changed = false;
    for (BasicBlock::iterator I = BB.getFirstInsertionPt(), E = BB.end();
         I != E;) {
      if (AllocaInst *Alloca = dyn_cast<AllocaInst>(I++)) {
        Changed = true;
        Type *ElementTy = Alloca->getType()->getPointerElementType();
        Constant *ElementSize =
            ConstantInt::get(IntPtrType, DL->getTypeAllocSize(ElementTy));
        // Expand out alloca's built-in multiplication.
        Value *MulSize;
        if (ConstantInt *C = dyn_cast<ConstantInt>(Alloca->getArraySize())) {
          const APInt Value =
              C->getValue().zextOrTrunc(IntPtrType->getScalarSizeInBits());
          MulSize = ConstantExpr::getMul(ElementSize,
                                         ConstantInt::get(IntPtrType, Value));
        } else {
          Value *ArraySize = Alloca->getArraySize();
          if (ArraySize->getType() != IntPtrType) {
            // We assume ArraySize is always positive, and thus is unsigned.
            assert(!isa<ConstantInt>(ArraySize) ||
                   !cast<ConstantInt>(ArraySize)->isNegative());
            ArraySize =
                CastInst::CreateIntegerCast(ArraySize, IntPtrType,
                                            /* isSigned = */ false, "", Alloca);
          }
          MulSize = CopyDebug(
              BinaryOperator::Create(Instruction::Mul, ElementSize, ArraySize,
                                     Alloca->getName() + ".alloca_mul", Alloca),
              Alloca);
        }
        unsigned Alignment = Alloca->getAlignment();
        if (Alignment == 0)
          Alignment = DL->getPrefTypeAlignment(ElementTy);
        AllocaInst *Tmp =
            new AllocaInst(Int8Type, MulSize, Alignment, "", Alloca);
        CopyDebug(Tmp, Alloca);
        Tmp->takeName(Alloca);
        BitCastInst *BC = new BitCastInst(Tmp, Alloca->getType(),
                                          Tmp->getName() + ".bc", Alloca);
        CopyDebug(BC, Alloca);
        Alloca->replaceAllUsesWith(BC);
        Alloca->eraseFromParent();
      }
    }
    return Changed;
  }
};
}
char SimplifyAllocas::ID = 0;

INITIALIZE_PASS(SimplifyAllocas, "simplify-allocas",
                "Simplify allocas to arrays of bytes", false, false)

BasicBlockPass *llvm::createSimplifyAllocasPass() {
  return new SimplifyAllocas();
}
