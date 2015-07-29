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
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/NaCl.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

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
      DL = &M->getDataLayout();
      IntPtrType = DL->getIntPtrType(M->getContext());
      Int8Type = Type::getInt8Ty(M->getContext());
      Initialized = true;
      return true;
    }
    return false;
  }

  AllocaInst *findAllocaFromCast(CastInst *CInst) {
    Value *Op0 = CInst->getOperand(0);
    while (!llvm::isa<AllocaInst>(Op0)) {
      auto *NextCast = llvm::dyn_cast<CastInst>(Op0);
      if (NextCast && NextCast->isNoopCast(IntPtrType)) {
        Op0 = NextCast->getOperand(0);
      } else {
        return nullptr;
      }
    }
    return llvm::cast<AllocaInst>(Op0);
  }

  bool runOnBasicBlock(BasicBlock &BB) override {
    bool Changed = false;
    for (BasicBlock::iterator I = BB.getFirstInsertionPt(), E = BB.end();
         I != E;) {
      Instruction *Inst = &*I++;
      if (AllocaInst *Alloca = dyn_cast<AllocaInst>(Inst)) {
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
      else if (auto *Call = dyn_cast<IntrinsicInst>(Inst)) {
        if (Call->getIntrinsicID() == Intrinsic::dbg_declare) {
          // dbg.declare's first argument is a special metadata that wraps a
          // value, and RAUW works on those. It is supposed to refer to the
          // alloca that represents the variable's storage, but the alloca
          // simplification may have RAUWed it to use the bitcast.
          // Fix it up here by recreating the metadata to use the new alloca.
          auto *MV = cast<MetadataAsValue>(Call->getArgOperand(0));
          // Sometimes dbg.declare points to an argument instead of an alloca.
          if (auto *VM = dyn_cast<ValueAsMetadata>(MV->getMetadata())) {
            if (auto *CInst = dyn_cast<CastInst>(VM->getValue())) {
              if (AllocaInst *Alloca = findAllocaFromCast(CInst)) {
                Call->setArgOperand(
                    0,
                    MetadataAsValue::get(Inst->getContext(),
                                         ValueAsMetadata::get(Alloca)));
                Changed = true;
              }
            }
          }
        }
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
