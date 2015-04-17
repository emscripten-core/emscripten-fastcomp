//===- ConstantInsertExtractElementIndex.cpp - Insert/Extract element -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Transform all InsertElement and ExtractElement with non-constant or
// out-of-bounds indices into either in-bounds constant accesses or
// stack accesses. This moves all undefined behavior to the stack,
// making InsertElement and ExtractElement well-defined.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/NaCl.h"

#include <algorithm>

using namespace llvm;

namespace {
class ConstantInsertExtractElementIndex : public BasicBlockPass {
public:
  static char ID; // Pass identification, replacement for typeid
  ConstantInsertExtractElementIndex() : BasicBlockPass(ID), M(0), DL(0) {
    initializeConstantInsertExtractElementIndexPass(
        *PassRegistry::getPassRegistry());
  }
  using BasicBlockPass::doInitialization;
  bool doInitialization(Module &Mod) override {
    M = &Mod;
    return false; // Unchanged.
  }
  bool runOnBasicBlock(BasicBlock &BB) override;

private:
  typedef SmallVector<Instruction *, 8> Instructions;
  const Module *M;
  const DataLayout *DL;

  void findNonConstantInsertExtractElements(
      const BasicBlock &BB, Instructions &OutOfRangeConstantIndices,
      Instructions &NonConstantVectorIndices) const;
  void fixOutOfRangeConstantIndices(BasicBlock &BB,
                                    const Instructions &Instrs) const;
  void fixNonConstantVectorIndices(BasicBlock &BB,
                                   const Instructions &Instrs) const;
};

/// Number of elements in a vector instruction.
unsigned vectorNumElements(const Instruction *I) {
  return cast<VectorType>(I->getOperand(0)->getType())->getNumElements();
}

/// Get the index of an InsertElement or ExtractElement instruction, or null.
Value *getInsertExtractElementIdx(const Instruction *I) {
  switch (I->getOpcode()) {
  default: return NULL;
  case Instruction::InsertElement: return I->getOperand(2);
  case Instruction::ExtractElement: return I->getOperand(1);
  }
}

/// Set the index of an InsertElement or ExtractElement instruction.
void setInsertExtractElementIdx(Instruction *I, Value *NewIdx) {
  switch (I->getOpcode()) {
  default:
    llvm_unreachable(
        "expected instruction to be InsertElement or ExtractElement");
  case Instruction::InsertElement: I->setOperand(2, NewIdx); break;
  case Instruction::ExtractElement: I->setOperand(1, NewIdx); break;
  }
}
} // anonymous namespace

char ConstantInsertExtractElementIndex::ID = 0;
INITIALIZE_PASS(
    ConstantInsertExtractElementIndex, "constant-insert-extract-element-index",
    "Force insert and extract vector element to always be in bounds", false,
    false)

void ConstantInsertExtractElementIndex::findNonConstantInsertExtractElements(
    const BasicBlock &BB, Instructions &OutOfRangeConstantIndices,
    Instructions &NonConstantVectorIndices) const {
  for (BasicBlock::const_iterator BBI = BB.begin(), BBE = BB.end(); BBI != BBE;
       ++BBI) {
    const Instruction *I = &*BBI;
    if (Value *Idx = getInsertExtractElementIdx(I)) {
      if (ConstantInt *CI = dyn_cast<ConstantInt>(Idx)) {
        if (!CI->getValue().ult(vectorNumElements(I)))
          OutOfRangeConstantIndices.push_back(const_cast<Instruction *>(I));
      } else
        NonConstantVectorIndices.push_back(const_cast<Instruction *>(I));
    }
  }
}

void ConstantInsertExtractElementIndex::fixOutOfRangeConstantIndices(
    BasicBlock &BB, const Instructions &Instrs) const {
  for (Instructions::const_iterator IB = Instrs.begin(), IE = Instrs.end();
       IB != IE; ++IB) {
    Instruction *I = *IB;
    const APInt &Idx =
        cast<ConstantInt>(getInsertExtractElementIdx(I))->getValue();
    APInt NumElements = APInt(Idx.getBitWidth(), vectorNumElements(I));
    APInt NewIdx = Idx.urem(NumElements);
    setInsertExtractElementIdx(I, ConstantInt::get(M->getContext(), NewIdx));
  }
}

void ConstantInsertExtractElementIndex::fixNonConstantVectorIndices(
    BasicBlock &BB, const Instructions &Instrs) const {
  for (Instructions::const_iterator IB = Instrs.begin(), IE = Instrs.end();
       IB != IE; ++IB) {
    Instruction *I = *IB;
    Value *Vec = I->getOperand(0);
    Value *Idx = getInsertExtractElementIdx(I);
    VectorType *VecTy = cast<VectorType>(Vec->getType());
    Type *ElemTy = VecTy->getElementType();
    unsigned ElemAlign = DL->getPrefTypeAlignment(ElemTy);
    unsigned VecAlign = std::max(ElemAlign, DL->getPrefTypeAlignment(VecTy));

    IRBuilder<> IRB(I);
    AllocaInst *Alloca = IRB.CreateAlloca(
        ElemTy, ConstantInt::get(Type::getInt32Ty(M->getContext()),
                                 vectorNumElements(I)));
    Alloca->setAlignment(VecAlign);
    Value *AllocaAsVec = IRB.CreateBitCast(Alloca, VecTy->getPointerTo());
    IRB.CreateAlignedStore(Vec, AllocaAsVec, Alloca->getAlignment());
    Value *GEP = IRB.CreateGEP(Alloca, Idx);

    Value *Res;
    switch (I->getOpcode()) {
    default:
      llvm_unreachable("expected InsertElement or ExtractElement");
    case Instruction::InsertElement:
      IRB.CreateAlignedStore(I->getOperand(1), GEP, ElemAlign);
      Res = IRB.CreateAlignedLoad(AllocaAsVec, Alloca->getAlignment());
      break;
    case Instruction::ExtractElement:
      Res = IRB.CreateAlignedLoad(GEP, ElemAlign);
      break;
    }

    I->replaceAllUsesWith(Res);
    I->eraseFromParent();
  }
}

bool ConstantInsertExtractElementIndex::runOnBasicBlock(BasicBlock &BB) {
  bool Changed = false;
  if (!DL)
    DL = &BB.getParent()->getParent()->getDataLayout();
  Instructions OutOfRangeConstantIndices;
  Instructions NonConstantVectorIndices;

  findNonConstantInsertExtractElements(BB, OutOfRangeConstantIndices,
                                       NonConstantVectorIndices);
  if (!OutOfRangeConstantIndices.empty()) {
    Changed = true;
    fixOutOfRangeConstantIndices(BB, OutOfRangeConstantIndices);
  }
  if (!NonConstantVectorIndices.empty()) {
    Changed = true;
    fixNonConstantVectorIndices(BB, NonConstantVectorIndices);
  }
  return Changed;
}

BasicBlockPass *llvm::createConstantInsertExtractElementIndexPass() {
  return new ConstantInsertExtractElementIndex();
}
