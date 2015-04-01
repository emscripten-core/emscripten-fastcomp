//===- NormalizeAlignment.cpp - Normalize Alignment -----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Normalize the alignment of loads and stores to better fit the PNaCl ABI:
//
//  * On memcpy/memmove/memset intrinsic calls.
//  * On regular memory accesses.
//  * On atomic memory accesses.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

namespace {
class NormalizeAlignment : public FunctionPass {
public:
  static char ID;
  NormalizeAlignment() : FunctionPass(ID) {
    initializeNormalizeAlignmentPass(*PassRegistry::getPassRegistry());
  }
  bool runOnFunction(Function &F) override;
};
}

char NormalizeAlignment::ID = 0;
INITIALIZE_PASS(NormalizeAlignment, "normalize-alignment",
                "Normalize the alignment of loads and stores", false, false)

static unsigned normalizeAlignment(DataLayout *DL, unsigned Alignment, Type *Ty,
                                   bool IsAtomic) {
  unsigned MaxAllowed = 1;
  if (isa<VectorType>(Ty))
    // Already handled properly by FixVectorLoadStoreAlignment.
    return Alignment;
  if (Ty->isDoubleTy() || Ty->isFloatTy() || IsAtomic)
    MaxAllowed = DL->getTypeAllocSize(Ty);
  // If the alignment is set to 0, this means "use the default
  // alignment for the target", which we fill in explicitly.
  if (Alignment == 0 || Alignment >= MaxAllowed)
    return MaxAllowed;
  return 1;
}

bool NormalizeAlignment::runOnFunction(Function &F) {
  DataLayout DL(F.getParent());
  bool Modified = false;

  for (BasicBlock &BB : F)
    for (Instruction &I : BB)
      if (auto *MemOp = dyn_cast<MemIntrinsic>(&I)) {
        Modified = true;
        Type *AlignTy = MemOp->getAlignmentCst()->getType();
        MemOp->setAlignment(ConstantInt::get(AlignTy, 1));
      } else if (auto *Load = dyn_cast<LoadInst>(&I)) {
        Modified = true;
        Load->setAlignment(normalizeAlignment(
            &DL, Load->getAlignment(), Load->getType(), Load->isAtomic()));
      } else if (auto *Store = dyn_cast<StoreInst>(&I)) {
        Modified = true;
        Store->setAlignment(normalizeAlignment(
            &DL, Store->getAlignment(), Store->getValueOperand()->getType(),
            Store->isAtomic()));
      }

  return Modified;
}

FunctionPass *llvm::createNormalizeAlignmentPass() {
  return new NormalizeAlignment();
}
