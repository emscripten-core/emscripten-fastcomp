//===- ExpandShuffleVector.cpp - shufflevector to {insert/extract}element -===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Replace all shufflevector instructions by insertelement / extractelement.
// BackendCanonicalize is able to reconstruct the shufflevector.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

namespace {
class ExpandShuffleVector : public BasicBlockPass {
public:
  static char ID; // Pass identification, replacement for typeid
  ExpandShuffleVector() : BasicBlockPass(ID), M(0) {
    initializeExpandShuffleVectorPass(*PassRegistry::getPassRegistry());
  }
  using BasicBlockPass::doInitialization;
  bool doInitialization(Module &Mod) override {
    M = &Mod;
    return false; // Unchanged.
  }
  bool runOnBasicBlock(BasicBlock &BB) override;

private:
  const Module *M;
  void Expand(ShuffleVectorInst *Shuf, Type *Int32);
};
}

char ExpandShuffleVector::ID = 0;
INITIALIZE_PASS(
    ExpandShuffleVector, "expand-shufflevector",
    "Expand shufflevector instructions into insertelement and extractelement",
    false, false)

void ExpandShuffleVector::Expand(ShuffleVectorInst *Shuf, Type *Int32) {
  Value *L = Shuf->getOperand(0);
  Value *R = Shuf->getOperand(1);
  assert(L->getType() == R->getType());
  VectorType *SrcVecTy = cast<VectorType>(L->getType());
  VectorType *DstVecTy = Shuf->getType();
  Type *ElemTy = DstVecTy->getElementType();
  SmallVector<int, 16> Mask = Shuf->getShuffleMask();
  unsigned NumSrcElems = SrcVecTy->getNumElements();
  unsigned NumDstElems = Mask.size();

  // Start with an undefined vector, extract each element from either L
  // or R according to the Mask, and insert it into contiguous element
  // locations in the result vector.
  //
  // The sources for shufflevector must have the same type but the
  // destination could be a narrower or wider vector with the same
  // element type.
  Instruction *ExtractLoc = Shuf;
  Value *Res = UndefValue::get(DstVecTy);
  for (unsigned Elem = 0; Elem != NumDstElems; ++Elem) {
    bool IsUndef =
        0 > Mask[Elem] || static_cast<unsigned>(Mask[Elem]) >= NumSrcElems * 2;
    bool IsL = static_cast<unsigned>(Mask[Elem]) < NumSrcElems;
    Value *From = IsL ? L : R;
    int Adjustment = IsL ? 0 : NumSrcElems;
    Constant *ExtractIdx = ConstantInt::get(Int32, Mask[Elem] - Adjustment);
    Constant *InsertIdx = ConstantInt::get(Int32, Elem);
    Value *ElemToInsert = IsUndef ? UndefValue::get(ElemTy)
                                  : (Value *)ExtractElementInst::Create(
                                        From, ExtractIdx, "", ExtractLoc);
    Res = InsertElementInst::Create(Res, ElemToInsert, InsertIdx, "", Shuf);
    if (ExtractLoc == Shuf)
      // All the extracts should be added just before the first insert we added.
      ExtractLoc = cast<Instruction>(Res);
  }

  Shuf->replaceAllUsesWith(Res);
  Shuf->eraseFromParent();
}

bool ExpandShuffleVector::runOnBasicBlock(BasicBlock &BB) {
  Type *Int32 = Type::getInt32Ty(M->getContext());
  typedef SmallVector<ShuffleVectorInst *, 8> Instructions;
  Instructions Shufs;

  for (BasicBlock::iterator BBI = BB.begin(); BBI != BB.end(); ++BBI)
    if (ShuffleVectorInst *S = dyn_cast<ShuffleVectorInst>(&*BBI))
      Shufs.push_back(S);

  for (Instructions::iterator S = Shufs.begin(), E = Shufs.end(); S != E; ++S)
    Expand(*S, Int32);

  return !Shufs.empty();
}

BasicBlockPass *llvm::createExpandShuffleVectorPass() {
  return new ExpandShuffleVector();
}
