//===- CombineVectorInstructions.cpp --------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass cleans up some of the toolchain-side PNaCl ABI
// simplification passes relating to vectors. These passes allow PNaCl
// to have a simple and stable ABI, but they sometimes lead to
// harder-to-optimize code.
//
// It currently:
// - Re-generates shufflevector (not part of the PNaCl ABI) from
//   insertelement / extractelement combinations. This is done by
//   duplicating some of instcombine's implementation, and ignoring
//   optimizations that should already have taken place.
// - TODO(jfb) Re-combine load/store for vectors, which are transformed
//             into load/store of the underlying elements.
// - TODO(jfb) Re-materialize constant arguments, which are currently
//             loads from global constant vectors.
//
// The pass also performs limited DCE on instructions it knows to be
// dead, instead of performing a full global DCE. Note that it can also
// eliminate load/store instructions that it makes redundant, which DCE
// can't traditionally do without proving the redundancy (somewhat
// prohibitive).
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/BasicBlock.h"
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

class CombineVectorInstructions
    : public BasicBlockPass,
      public InstVisitor<CombineVectorInstructions, bool> {
public:
  static char ID; // Pass identification, replacement for typeid
  CombineVectorInstructions() : BasicBlockPass(ID) {
    initializeCombineVectorInstructionsPass(*PassRegistry::getPassRegistry());
  }
  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.addRequired<TargetLibraryInfo>();
    BasicBlockPass::getAnalysisUsage(AU);
  }

  virtual bool runOnBasicBlock(BasicBlock &B);

  // InstVisitor implementation. Unhandled instructions stay as-is.
  bool visitInstruction(Instruction &I) { return false; }
  bool visitInsertElementInst(InsertElementInst &IE);

 private:
  // List of instructions that are now obsolete, and should be DCE'd.
  typedef SmallVector<Instruction *, 16> KillListT;
  KillListT KillList;

  /// Empty the kill list, making sure that all other dead instructions
  /// up the chain (but in the current basic block) also get killed.
  void emptyKillList(BasicBlock &B);
};

} // anonymous namespace

char CombineVectorInstructions::ID = 0;
INITIALIZE_PASS(CombineVectorInstructions, "combine-vector-instructions",
                "Combine vector instructions", false, false)

bool CombineVectorInstructions::runOnBasicBlock(BasicBlock &B) {
  bool Modified = false;
  for (BasicBlock::iterator BI = B.begin(), BE = B.end(); BI != BE; ++BI)
    Modified |= visit(&*BI);
  emptyKillList(B);
  return Modified;
}

// This function is *almost* as-is from instcombine, avoiding silly
// cases that should already have been optimized.
bool CombineVectorInstructions::visitInsertElementInst(InsertElementInst &IE) {
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
        KillList.push_back(&IE);

        return true;
      }
    }
  }

  return false;
}

void CombineVectorInstructions::emptyKillList(BasicBlock &B) {
  const TargetLibraryInfo *TLI = &getAnalysis<TargetLibraryInfo>();
  while (!KillList.empty()) {
    Instruction *KillMe = KillList.pop_back_val();
    if (isa<LoadInst>(KillMe) || isa<StoreInst>(KillMe)) {
      // Load/store instructions can't traditionally be killed since
      // they have side-effects. This pass combines load/store
      // instructions and touches all the memory that the original
      // load/store touched, it's therefore legal to kill these
      // load/store instructions.
      //
      // TODO(jfb) Eliminate load/store once their combination is
      //           implemented.
    } else
      RecursivelyDeleteTriviallyDeadInstructions(KillMe, TLI);
  }
}

BasicBlockPass *llvm::createCombineVectorInstructionsPass() {
  return new CombineVectorInstructions();
}
