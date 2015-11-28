//==- ExpandInsertExtractElement.cpp - Expand vector insert and extract -=//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===------------------------------------------------------------------===//
//
// This pass expands insertelement and extractelement instructions with
// variable indices, which SIMD.js doesn't natively support yet.
//
//===------------------------------------------------------------------===//

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/NaCl.h"
#include "llvm/Transforms/Utils/Local.h"
#include <map>
#include <vector>

#include "llvm/Support/raw_ostream.h"

#ifdef NDEBUG
#undef assert
#define assert(x) { if (!(x)) report_fatal_error(#x); }
#endif

using namespace llvm;

namespace {

  class ExpandInsertExtractElement : public FunctionPass {
    bool Changed;

  public:
    static char ID;
    ExpandInsertExtractElement() : FunctionPass(ID) {
      initializeExpandInsertExtractElementPass(*PassRegistry::getPassRegistry());
    }

    bool runOnFunction(Function &F) override;
  };
}

char ExpandInsertExtractElement::ID = 0;
INITIALIZE_PASS(ExpandInsertExtractElement, "expand-insert-extract-elements",
                "Expand and lower insert and extract element operations",
                false, false)

// Utilities

bool ExpandInsertExtractElement::runOnFunction(Function &F) {
  Changed = false;

  Instruction *Entry = &*F.getEntryBlock().begin();
  Type *Int32 = Type::getInt32Ty(F.getContext());
  Constant *Zero = ConstantInt::get(Int32, 0);
  for (inst_iterator I = inst_begin(&F), E = inst_end(&F); I != E; ) {
    Instruction *Inst = &*I++;

    if (InsertElementInst *III = dyn_cast<InsertElementInst>(Inst)) {
      if (isa<ConstantInt>(III->getOperand(2)))
          continue;

      Type *AllocaTy = III->getType();
      Instruction *A = new AllocaInst(AllocaTy, 0, "", Entry);
      CopyDebug(new StoreInst(III->getOperand(0), A, III), III);

      Value *Idxs[] = { Zero, III->getOperand(2) };
      Instruction *B = CopyDebug(
          GetElementPtrInst::Create(AllocaTy, A, Idxs, "", III), III);
      CopyDebug(new StoreInst(III->getOperand(1), B, III), III);

      Instruction *L = CopyDebug(new LoadInst(A, "", III), III);
      III->replaceAllUsesWith(L);
      III->eraseFromParent();
    } else if (ExtractElementInst *EII = dyn_cast<ExtractElementInst>(Inst)) {
      if (isa<ConstantInt>(EII->getOperand(1)))
          continue;

      Type *AllocaTy = EII->getOperand(0)->getType();
      Instruction *A = new AllocaInst(AllocaTy, 0, "", Entry);
      CopyDebug(new StoreInst(EII->getOperand(0), A, EII), EII);

      Value *Idxs[] = { Zero, EII->getOperand(1) };
      Instruction *B = CopyDebug(
          GetElementPtrInst::Create(AllocaTy, A, Idxs, "", EII), EII);
      Instruction *L = CopyDebug(new LoadInst(B, "", EII), EII);
      EII->replaceAllUsesWith(L);
      EII->eraseFromParent();
    }
  }

  return Changed;
}

FunctionPass *llvm::createExpandInsertExtractElementPass() {
  return new ExpandInsertExtractElement();
}
