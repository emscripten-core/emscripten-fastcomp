//===- BackendCanonicalize.cpp --------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Clean up some toolchain-side PNaCl ABI simplification passes. These passes
// allow PNaCl to have a simple and stable ABI, but they sometimes lead to
// harder-to-optimize code. This is desirable because LLVM's definition of
// "canonical" evolves over time, meaning that PNaCl's simple ABI can stay
// simple yet still take full advantage of LLVM's backend by having this pass
// massage the code into something that the backend prefers handling.
//
// It currently:
// - Re-generates shufflevector (not part of the PNaCl ABI) from insertelement /
//   extractelement combinations. This is done by duplicating some of
//   instcombine's implementation, and ignoring optimizations that should
//   already have taken place.
// - Re-materializes constant loads, especially of vectors. This requires doing
//   constant folding through bitcasts.
//
// The pass also performs limited DCE on instructions it knows to be dead,
// instead of performing a full global DCE.
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/ConstantFolding.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Operator.h"
#include "llvm/InstVisitor.h"
#include "llvm/Pass.h"
#include "llvm/Target/TargetLibraryInfo.h"
#include "llvm/Transforms/NaCl.h"
#include "llvm/Transforms/Utils/Local.h"

using namespace llvm;

namespace {
// TODO(jfb) This function is as-is from instcombine. Make it reusable instead.
//
/// CollectSingleShuffleElements - If V is a shuffle of values that ONLY returns
/// elements from either LHS or RHS, return the shuffle mask and true.
/// Otherwise, return false.
bool CollectSingleShuffleElements(Value *V, Value *LHS, Value *RHS,
                                         SmallVectorImpl<Constant*> &Mask) {
  assert(V->getType() == LHS->getType() && V->getType() == RHS->getType() &&
         "Invalid CollectSingleShuffleElements");
  unsigned NumElts = V->getType()->getVectorNumElements();

  if (isa<UndefValue>(V)) {
    Mask.assign(NumElts, UndefValue::get(Type::getInt32Ty(V->getContext())));
    return true;
  }

  if (V == LHS) {
    for (unsigned i = 0; i != NumElts; ++i)
      Mask.push_back(ConstantInt::get(Type::getInt32Ty(V->getContext()), i));
    return true;
  }

  if (V == RHS) {
    for (unsigned i = 0; i != NumElts; ++i)
      Mask.push_back(ConstantInt::get(Type::getInt32Ty(V->getContext()),
                                      i+NumElts));
    return true;
  }

  if (InsertElementInst *IEI = dyn_cast<InsertElementInst>(V)) {
    // If this is an insert of an extract from some other vector, include it.
    Value *VecOp    = IEI->getOperand(0);
    Value *ScalarOp = IEI->getOperand(1);
    Value *IdxOp    = IEI->getOperand(2);

    if (!isa<ConstantInt>(IdxOp))
      return false;
    unsigned InsertedIdx = cast<ConstantInt>(IdxOp)->getZExtValue();

    if (isa<UndefValue>(ScalarOp)) {  // inserting undef into vector.
      // Okay, we can handle this if the vector we are insertinting into is
      // transitively ok.
      if (CollectSingleShuffleElements(VecOp, LHS, RHS, Mask)) {
        // If so, update the mask to reflect the inserted undef.
        Mask[InsertedIdx] = UndefValue::get(Type::getInt32Ty(V->getContext()));
        return true;
      }
    } else if (ExtractElementInst *EI = dyn_cast<ExtractElementInst>(ScalarOp)){
      if (isa<ConstantInt>(EI->getOperand(1)) &&
          EI->getOperand(0)->getType() == V->getType()) {
        unsigned ExtractedIdx =
        cast<ConstantInt>(EI->getOperand(1))->getZExtValue();

        // This must be extracting from either LHS or RHS.
        if (EI->getOperand(0) == LHS || EI->getOperand(0) == RHS) {
          // Okay, we can handle this if the vector we are insertinting into is
          // transitively ok.
          if (CollectSingleShuffleElements(VecOp, LHS, RHS, Mask)) {
            // If so, update the mask to reflect the inserted value.
            if (EI->getOperand(0) == LHS) {
              Mask[InsertedIdx % NumElts] =
              ConstantInt::get(Type::getInt32Ty(V->getContext()),
                               ExtractedIdx);
            } else {
              assert(EI->getOperand(0) == RHS);
              Mask[InsertedIdx % NumElts] =
              ConstantInt::get(Type::getInt32Ty(V->getContext()),
                               ExtractedIdx+NumElts);
            }
            return true;
          }
        }
      }
    }
  }
  // TODO: Handle shufflevector here!

  return false;
}

// TODO(jfb) This function is as-is from instcombine. Make it reusable instead.
//
/// CollectShuffleElements - We are building a shuffle of V, using RHS as the
/// RHS of the shuffle instruction, if it is not null.  Return a shuffle mask
/// that computes V and the LHS value of the shuffle.
Value *CollectShuffleElements(Value *V, SmallVectorImpl<Constant*> &Mask,
                                     Value *&RHS) {
  assert(V->getType()->isVectorTy() &&
         (RHS == 0 || V->getType() == RHS->getType()) &&
         "Invalid shuffle!");
  unsigned NumElts = cast<VectorType>(V->getType())->getNumElements();

  if (isa<UndefValue>(V)) {
    Mask.assign(NumElts, UndefValue::get(Type::getInt32Ty(V->getContext())));
    return V;
  }

  if (isa<ConstantAggregateZero>(V)) {
    Mask.assign(NumElts, ConstantInt::get(Type::getInt32Ty(V->getContext()),0));
    return V;
  }

  if (InsertElementInst *IEI = dyn_cast<InsertElementInst>(V)) {
    // If this is an insert of an extract from some other vector, include it.
    Value *VecOp    = IEI->getOperand(0);
    Value *ScalarOp = IEI->getOperand(1);
    Value *IdxOp    = IEI->getOperand(2);

    if (ExtractElementInst *EI = dyn_cast<ExtractElementInst>(ScalarOp)) {
      if (isa<ConstantInt>(EI->getOperand(1)) && isa<ConstantInt>(IdxOp) &&
          EI->getOperand(0)->getType() == V->getType()) {
        unsigned ExtractedIdx =
          cast<ConstantInt>(EI->getOperand(1))->getZExtValue();
        unsigned InsertedIdx = cast<ConstantInt>(IdxOp)->getZExtValue();

        // Either the extracted from or inserted into vector must be RHSVec,
        // otherwise we'd end up with a shuffle of three inputs.
        if (EI->getOperand(0) == RHS || RHS == 0) {
          RHS = EI->getOperand(0);
          Value *V = CollectShuffleElements(VecOp, Mask, RHS);
          Mask[InsertedIdx % NumElts] =
            ConstantInt::get(Type::getInt32Ty(V->getContext()),
                             NumElts+ExtractedIdx);
          return V;
        }

        if (VecOp == RHS) {
          Value *V = CollectShuffleElements(EI->getOperand(0), Mask, RHS);
          // Update Mask to reflect that `ScalarOp' has been inserted at
          // position `InsertedIdx' within the vector returned by IEI.
          Mask[InsertedIdx % NumElts] = Mask[ExtractedIdx];

          // Everything but the extracted element is replaced with the RHS.
          for (unsigned i = 0; i != NumElts; ++i) {
            if (i != InsertedIdx)
              Mask[i] = ConstantInt::get(Type::getInt32Ty(V->getContext()),
                                         NumElts+i);
          }
          return V;
        }

        // If this insertelement is a chain that comes from exactly these two
        // vectors, return the vector and the effective shuffle.
        if (CollectSingleShuffleElements(IEI, EI->getOperand(0), RHS, Mask))
          return EI->getOperand(0);
      }
    }
  }
  // TODO: Handle shufflevector here!

  // Otherwise, can't do anything fancy.  Return an identity vector.
  for (unsigned i = 0; i != NumElts; ++i)
    Mask.push_back(ConstantInt::get(Type::getInt32Ty(V->getContext()), i));
  return V;
}

class BackendCanonicalize : public FunctionPass,
                            public InstVisitor<BackendCanonicalize, bool> {
public:
  static char ID; // Pass identification, replacement for typeid
  BackendCanonicalize() : FunctionPass(ID), DL(0), TLI(0) {
    initializeBackendCanonicalizePass(*PassRegistry::getPassRegistry());
  }
  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<DataLayout>();
    AU.addRequired<TargetLibraryInfo>();
    FunctionPass::getAnalysisUsage(AU);
  }

  virtual bool runOnFunction(Function &F);

  // InstVisitor implementation. Unhandled instructions stay as-is.
  bool visitInstruction(Instruction &I) { return false; }
  bool visitInsertElementInst(InsertElementInst &IE);
  bool visitBitCastInst(BitCastInst &C);
  bool visitLoadInst(LoadInst &L);

private:
  const DataLayout *DL;
  const TargetLibraryInfo *TLI;

  // List of instructions that are now obsolete, and should be DCE'd.
  typedef SmallVector<Instruction *, 512> KillList;
  KillList Kill;

  /// Helper that constant folds an instruction.
  bool visitConstantFoldableInstruction(Instruction *I);

  /// Empty the kill list, making sure that all other dead instructions
  /// up the chain (but in the current basic block) also get killed.
  static void emptyKillList(KillList &Kill);
};

} // anonymous namespace

char BackendCanonicalize::ID = 0;
INITIALIZE_PASS(BackendCanonicalize, "backend-canonicalize",
                "Canonicalize PNaCl bitcode for LLVM backends", false, false)

bool BackendCanonicalize::runOnFunction(Function &F) {
  bool Modified = false;
  DL = &getAnalysis<DataLayout>();
  TLI = &getAnalysis<TargetLibraryInfo>();

  for (Function::iterator FI = F.begin(), FE = F.end(); FI != FE; ++FI)
    for (BasicBlock::iterator BI = FI->begin(), BE = FI->end(); BI != BE; ++BI)
      Modified |= visit(&*BI);
  emptyKillList(Kill);
  return Modified;
}

// This function is *almost* as-is from instcombine, avoiding silly
// cases that should already have been optimized.
bool BackendCanonicalize::visitInsertElementInst(InsertElementInst &IE) {
  Value *ScalarOp = IE.getOperand(1);
  Value *IdxOp = IE.getOperand(2);

  // If the inserted element was extracted from some other vector, and if the
  // indexes are constant, try to turn this into a shufflevector operation.
  if (ExtractElementInst *EI = dyn_cast<ExtractElementInst>(ScalarOp)) {
    if (isa<ConstantInt>(EI->getOperand(1)) && isa<ConstantInt>(IdxOp) &&
        EI->getOperand(0)->getType() == IE.getType()) {
      unsigned NumVectorElts = IE.getType()->getNumElements();
      unsigned ExtractedIdx =
          cast<ConstantInt>(EI->getOperand(1))->getZExtValue();
      unsigned InsertedIdx = cast<ConstantInt>(IdxOp)->getZExtValue();

      if (ExtractedIdx >= NumVectorElts) // Out of range extract.
        return false;

      if (InsertedIdx >= NumVectorElts) // Out of range insert.
        return false;

      // If this insertelement isn't used by some other insertelement, turn it
      // (and any insertelements it points to), into one big shuffle.
      if (!IE.hasOneUse() || !isa<InsertElementInst>(IE.use_back())) {
        typedef SmallVector<Constant *, 16> MaskT;
        MaskT Mask;
        Value *RHS = 0;
        Value *LHS = CollectShuffleElements(&IE, Mask, RHS);
        if (RHS == 0)
          RHS = UndefValue::get(LHS->getType());
        // We now have a shuffle of LHS, RHS, Mask.

        if (isa<UndefValue>(LHS) && !isa<UndefValue>(RHS)) {
          // Canonicalize shufflevector to always have undef on the RHS,
          // and adjust the mask.
          std::swap(LHS, RHS);
          for (MaskT::iterator I = Mask.begin(), E = Mask.end(); I != E; ++I) {
            unsigned Idx = cast<ConstantInt>(*I)->getZExtValue();
            unsigned NewIdx = Idx >= NumVectorElts ? Idx - NumVectorElts
                                                   : Idx + NumVectorElts;
            *I = ConstantInt::get(Type::getInt32Ty(RHS->getContext()), NewIdx);
          }
        }

        IRBuilder<> IRB(&IE);
        IE.replaceAllUsesWith(
            IRB.CreateShuffleVector(LHS, RHS, ConstantVector::get(Mask)));
        // The chain of now-dead insertelement / extractelement
        // instructions can be deleted.
        Kill.push_back(&IE);

        return true;
      }
    }
  }

  return false;
}

bool BackendCanonicalize::visitBitCastInst(BitCastInst &B) {
  return visitConstantFoldableInstruction(&B);
}

bool BackendCanonicalize::visitLoadInst(LoadInst &L) {
  return visitConstantFoldableInstruction(&L);
}

bool BackendCanonicalize::visitConstantFoldableInstruction(Instruction *I) {
  if (Constant *Folded = ConstantFoldInstruction(I, DL, TLI)) {
    I->replaceAllUsesWith(Folded);
    Kill.push_back(I);
    return true;
  }
  return false;
}

void BackendCanonicalize::emptyKillList(KillList &Kill) {
  while (!Kill.empty())
    RecursivelyDeleteTriviallyDeadInstructions(Kill.pop_back_val());
}

FunctionPass *llvm::createBackendCanonicalizePass() {
  return new BackendCanonicalize();
}
