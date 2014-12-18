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

#include "OptPasses.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/InstIterator.h"
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

    virtual bool runOnFunction(Function &F);
  };
}

char ExpandInsertExtractElement::ID = 0;
INITIALIZE_PASS(ExpandInsertExtractElement, "expand-insert-extract-elements",
                "Expand and lower insert and extract element operations",
                false, false)

// Utilities

static Instruction *CopyDebug(Instruction *NewInst, Instruction *Original) {
  NewInst->setDebugLoc(Original->getDebugLoc());
  return NewInst;
}

bool ExpandInsertExtractElement::runOnFunction(Function &F) {
  Changed = false;

  Instruction *Entry = F.getEntryBlock().begin();
  Type *Int32 = Type::getInt32Ty(F.getContext());
  Constant *Zero = ConstantInt::get(Int32, 0);
  for (inst_iterator I = inst_begin(&F), E = inst_end(&F); I != E; ) {
    Instruction *Inst = &*I++;

    if (InsertElementInst *III = dyn_cast<InsertElementInst>(Inst)) {
      if (isa<ConstantInt>(III->getOperand(2)))
          continue;

      Instruction *A = new AllocaInst(III->getType(), 0, "", Entry);
      CopyDebug(new StoreInst(III->getOperand(0), A, III), III);

      Value *Idxs[] = { Zero, III->getOperand(2) };
      Instruction *B = CopyDebug(GetElementPtrInst::Create(A, Idxs, "", III), III);
      CopyDebug(new StoreInst(III->getOperand(1), B, III), III);

      Instruction *L = CopyDebug(new LoadInst(A, "", III), III);
      III->replaceAllUsesWith(L);
      III->eraseFromParent();
    } else if (ExtractElementInst *EII = dyn_cast<ExtractElementInst>(Inst)) {
      if (isa<ConstantInt>(EII->getOperand(1)))
          continue;

      Instruction *A = new AllocaInst(EII->getOperand(0)->getType(), 0, "", Entry);
      CopyDebug(new StoreInst(EII->getOperand(0), A, EII), EII);

      Value *Idxs[] = { Zero, EII->getOperand(1) };
      Instruction *B = CopyDebug(GetElementPtrInst::Create(A, Idxs, "", EII), EII);
      Instruction *L = CopyDebug(new LoadInst(B, "", EII), EII);
      EII->replaceAllUsesWith(L);
      EII->eraseFromParent();
    }
  }

  return Changed;
}

Pass *llvm::createExpandInsertExtractElementPass() {
  return new ExpandInsertExtractElement();
}
