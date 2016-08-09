//===-- AllocaManager.cpp -------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the AllocaManager class.
//
// The AllocaManager computes a frame layout, assigning every static alloca an
// offset. It does alloca liveness analysis in order to reuse stack memory,
// using lifetime intrinsics.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "allocamanager"
#include "AllocaManager.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Timer.h"
#include "llvm/ADT/Statistic.h"
using namespace llvm;

STATISTIC(NumAllocas, "Number of allocas eliminated");

// Return the size of the given alloca.
uint64_t AllocaManager::getSize(const AllocaInst *AI) {
  assert(AI->isStaticAlloca());
  return DL->getTypeAllocSize(AI->getAllocatedType()) *
         cast<ConstantInt>(AI->getArraySize())->getValue().getZExtValue();
}

// Return the alignment of the given alloca.
unsigned AllocaManager::getAlignment(const AllocaInst *AI) {
  assert(AI->isStaticAlloca());
  unsigned Alignment = std::max(AI->getAlignment(),
                                DL->getABITypeAlignment(AI->getAllocatedType()));
  MaxAlignment = std::max(Alignment, MaxAlignment);
  return Alignment;
}

AllocaManager::AllocaInfo AllocaManager::getInfo(const AllocaInst *AI, unsigned Index) {
  assert(AI->isStaticAlloca());
  return AllocaInfo(AI, getSize(AI), getAlignment(AI), Index);
}

// Given a lifetime_start or lifetime_end intrinsic, determine if it's
// describing a single pointer suitable for our analysis. If so,
// return the pointer, otherwise return NULL.
const Value *
AllocaManager::getPointerFromIntrinsic(const CallInst *CI) {
  const IntrinsicInst *II = cast<IntrinsicInst>(CI);
  assert(II->getIntrinsicID() == Intrinsic::lifetime_start ||
         II->getIntrinsicID() == Intrinsic::lifetime_end);

  // Lifetime intrinsics have a size as their first argument and a pointer as
  // their second argument.
  const Value *Size = II->getArgOperand(0);
  const Value *Ptr = II->getArgOperand(1);

  // Check to see if we can convert the size to a host integer. If we can't,
  // it's probably not worth worrying about.
  const ConstantInt *SizeCon = dyn_cast<ConstantInt>(Size);
  if (!SizeCon) return NULL;
  const APInt &SizeAP = SizeCon->getValue();
  if (SizeAP.getActiveBits() > 64) return NULL;
  uint64_t MarkedSize = SizeAP.getZExtValue();

  // Test whether the pointer operand is an alloca. This ought to be pretty
  // simple, but e.g. PRE can decide to PRE bitcasts and no-op geps and
  // split critical edges and insert phis for them, even though it's all
  // just no-ops, so we have to dig through phis to see whether all the
  // inputs are in fact the same pointer after stripping away casts.
  const Value *Result = NULL;
  SmallPtrSet<const PHINode *, 8> VisitedPhis;
  SmallVector<const Value *, 8> Worklist;
  Worklist.push_back(Ptr);
  do {
      const Value *P = Worklist.pop_back_val()->stripPointerCasts();

      if (const PHINode *Phi = dyn_cast<PHINode>(P)) {
        if (!VisitedPhis.insert(Phi).second)
          continue;
        for (unsigned i = 0, e = Phi->getNumOperands(); i < e; ++i)
          Worklist.push_back(Phi->getOperand(i));
        continue;
      }
      if (const SelectInst *Select = dyn_cast<SelectInst>(P)) {
        Worklist.push_back(Select->getTrueValue());
        Worklist.push_back(Select->getFalseValue());
        continue;
      }

      if (Result == NULL)
        Result = P;
      else if (Result != P)
        return NULL;
  } while (!Worklist.empty());

  // If it's a static Alloca, make sure the size is suitable. We test this here
  // because if this fails, we need to be as conservative as if we don't know
  // what the pointer is.
  if (const AllocaInst *AI = dyn_cast<AllocaInst>(Result)) {
    if (AI->isStaticAlloca() && MarkedSize < getSize(AI))
      return NULL;
  } else if (isa<Instruction>(Result)) {
    // And if it's any other kind of non-object/argument, we have to be
    // similarly conservative, because we may be dealing with an escaped alloca
    // that we can't see.
    return NULL;
  }

  // Yay, it's all just one Value!
  return Result;
}

// Test whether the given value is an alloca which we have a hope of
const AllocaInst *AllocaManager::isFavorableAlloca(const Value *V) {
  const AllocaInst *AI = dyn_cast<AllocaInst>(V);
  if (!AI) return NULL;

  if (!AI->isStaticAlloca()) return NULL;

  return AI;
}

int AllocaManager::AllocaSort(const AllocaInfo *li, const AllocaInfo *ri) {
  // Sort by alignment to minimize padding.
  if (li->getAlignment() > ri->getAlignment()) return -1;
  if (li->getAlignment() < ri->getAlignment()) return 1;

  // Ensure a stable sort by comparing an index value which we've kept for
  // this purpose.
  if (li->getIndex() > ri->getIndex()) return -1;
  if (li->getIndex() < ri->getIndex()) return 1;

  return 0;
}

// Collect allocas
void AllocaManager::collectMarkedAllocas() {
  NamedRegionTimer Timer("Collect Marked Allocas", "AllocaManager",
                         TimePassesIsEnabled);

  // Weird semantics: If an alloca *ever* appears in a lifetime start or end
  // within the same function, its lifetime begins only at the explicit lifetime
  // starts and ends only at the explicit lifetime ends and function exit
  // points. Otherwise, its lifetime begins in the entry block and it is live
  // everywhere.
  //
  // And so, instead of just walking the entry block to find all the static
  // allocas, we walk the whole body to find the intrinsics so we can find the
  // set of static allocas referenced in the intrinsics.
  for (Function::const_iterator FI = F->begin(), FE = F->end();
       FI != FE; ++FI) {
    for (BasicBlock::const_iterator BI = FI->begin(), BE = FI->end();
         BI != BE; ++BI) {
      const CallInst *CI = dyn_cast<CallInst>(BI);
      if (!CI) continue;

      const Value *Callee = CI->getCalledValue();
      if (Callee == LifetimeStart || Callee == LifetimeEnd) {
        if (const Value *Ptr = getPointerFromIntrinsic(CI)) {
          if (const AllocaInst *AI = isFavorableAlloca(Ptr))
            Allocas.insert(std::make_pair(AI, 0));
        } else if (isa<Instruction>(CI->getArgOperand(1)->stripPointerCasts())) {
          // Oh noes, There's a lifetime intrinsics with something that
          // doesn't appear to resolve to an alloca. This means that it's
          // possible that it may be declaring a lifetime for some escaping
          // alloca. Look out!
          Allocas.clear();
          assert(AllocasByIndex.empty());
          return;
        }
      }
    }
  }

  // All that said, we still want the intrinsics in the order they appear in the
  // block, so that we can represent later ones with earlier ones and skip
  // worrying about dominance, so run through the entry block and index those
  // allocas which we identified above.
  AllocasByIndex.reserve(Allocas.size());
  const BasicBlock *EntryBB = &F->getEntryBlock();
  for (BasicBlock::const_iterator BI = EntryBB->begin(), BE = EntryBB->end();
       BI != BE; ++BI) {
    const AllocaInst *AI = dyn_cast<AllocaInst>(BI);
    if (!AI || !AI->isStaticAlloca()) continue;

    AllocaMap::iterator I = Allocas.find(AI);
    if (I != Allocas.end()) {
      I->second = AllocasByIndex.size();
      AllocasByIndex.push_back(getInfo(AI, AllocasByIndex.size()));
    }
  }
  assert(AllocasByIndex.size() == Allocas.size());
}

// Calculate the starting point from which inter-block liveness will be
// computed.
void AllocaManager::collectBlocks() {
  NamedRegionTimer Timer("Collect Blocks", "AllocaManager",
                         TimePassesIsEnabled);

  size_t AllocaCount = AllocasByIndex.size();

  BitVector Seen(AllocaCount);

  for (Function::const_iterator I = F->begin(), E = F->end(); I != E; ++I) {
    const BasicBlock *BB = &*I;

    BlockLifetimeInfo &BLI = BlockLiveness[BB];
    BLI.Start.resize(AllocaCount);
    BLI.End.resize(AllocaCount);

    // Track which allocas we've seen. This is used because if a lifetime start
    // is the first lifetime marker for an alloca in a block, the alloca is
    // live-in.
    Seen.reset();

    // Walk the instructions and compute the Start and End sets.
    for (BasicBlock::const_iterator BI = BB->begin(), BE = BB->end();
         BI != BE; ++BI) {
      const CallInst *CI = dyn_cast<CallInst>(BI);
      if (!CI) continue;

      const Value *Callee = CI->getCalledValue();
      if (Callee == LifetimeStart) {
        if (const Value *Ptr = getPointerFromIntrinsic(CI)) {
          if (const AllocaInst *AI = isFavorableAlloca(Ptr)) {
            AllocaMap::const_iterator MI = Allocas.find(AI);
            if (MI != Allocas.end()) {
              size_t AllocaIndex = MI->second;
              if (!Seen.test(AllocaIndex)) {
                BLI.Start.set(AllocaIndex);
              }
              BLI.End.reset(AllocaIndex);
              Seen.set(AllocaIndex);
            }
          }
        }
      } else if (Callee == LifetimeEnd) {
        if (const Value *Ptr = getPointerFromIntrinsic(CI)) {
          if (const AllocaInst *AI = isFavorableAlloca(Ptr)) {
            AllocaMap::const_iterator MI = Allocas.find(AI);
            if (MI != Allocas.end()) {
              size_t AllocaIndex = MI->second;
              BLI.End.set(AllocaIndex);
              Seen.set(AllocaIndex);
            }
          }
        }
      }
    }

    // Lifetimes that start in this block and do not end here are live-out.
    BLI.LiveOut = BLI.Start;
    BLI.LiveOut.reset(BLI.End);
    if (BLI.LiveOut.any()) {
      for (succ_const_iterator SI = succ_begin(BB), SE = succ_end(BB);
           SI != SE; ++SI) {
        InterBlockTopDownWorklist.insert(*SI);
      }
    }

    // Lifetimes that end in this block and do not start here are live-in.
    // TODO: Is this actually true? What are the semantics of a standalone
    // lifetime end? See also the code in computeInterBlockLiveness.
    BLI.LiveIn = BLI.End;
    BLI.LiveIn.reset(BLI.Start);
    if (BLI.LiveIn.any()) {
      for (const_pred_iterator PI = pred_begin(BB), PE = pred_end(BB);
           PI != PE; ++PI) {
        InterBlockBottomUpWorklist.insert(*PI);
      }
    }
  }
}

// Compute the LiveIn and LiveOut sets for each block in F.
void AllocaManager::computeInterBlockLiveness() {
  NamedRegionTimer Timer("Compute inter-block liveness", "AllocaManager",
                         TimePassesIsEnabled);

  size_t AllocaCount = AllocasByIndex.size();

  BitVector Temp(AllocaCount);

  // Proporgate liveness backwards.
  while (!InterBlockBottomUpWorklist.empty()) {
    const BasicBlock *BB = InterBlockBottomUpWorklist.pop_back_val();
    BlockLifetimeInfo &BLI = BlockLiveness[BB];

    // Compute the new live-out set.
    for (succ_const_iterator SI = succ_begin(BB), SE = succ_end(BB);
         SI != SE; ++SI) {
      Temp |= BlockLiveness[*SI].LiveIn;
    }

    // If it contains new live blocks, prepare to propagate them.
    // TODO: As above, what are the semantics of a standalone lifetime end?
    Temp.reset(BLI.Start);
    if (Temp.test(BLI.LiveIn)) {
      BLI.LiveIn |= Temp;
      for (const_pred_iterator PI = pred_begin(BB), PE = pred_end(BB);
           PI != PE; ++PI) {
        InterBlockBottomUpWorklist.insert(*PI);
      }
    }
    Temp.reset();
  }

  // Proporgate liveness forwards.
  while (!InterBlockTopDownWorklist.empty()) {
    const BasicBlock *BB = InterBlockTopDownWorklist.pop_back_val();
    BlockLifetimeInfo &BLI = BlockLiveness[BB];

    // Compute the new live-in set.
    for (const_pred_iterator PI = pred_begin(BB), PE = pred_end(BB);
         PI != PE; ++PI) {
      Temp |= BlockLiveness[*PI].LiveOut;
    }

    // Also record the live-in values.
    BLI.LiveIn |= Temp;

    // If it contains new live blocks, prepare to propagate them.
    Temp.reset(BLI.End);
    if (Temp.test(BLI.LiveOut)) {
      BLI.LiveOut |= Temp;
      for (succ_const_iterator SI = succ_begin(BB), SE = succ_end(BB);
           SI != SE; ++SI) {
        InterBlockTopDownWorklist.insert(*SI);
      }
    }
    Temp.reset();
  }
}

// Determine overlapping liveranges within blocks.
void AllocaManager::computeIntraBlockLiveness() {
  NamedRegionTimer Timer("Compute intra-block liveness", "AllocaManager",
                         TimePassesIsEnabled);

  size_t AllocaCount = AllocasByIndex.size();

  BitVector Current(AllocaCount);

  AllocaCompatibility.resize(AllocaCount, BitVector(AllocaCount, true));

  for (Function::const_iterator I = F->begin(), E = F->end(); I != E; ++I) {
    const BasicBlock *BB = &*I;
    const BlockLifetimeInfo &BLI = BlockLiveness[BB];

    Current = BLI.LiveIn;

    for (int i = Current.find_first(); i >= 0; i = Current.find_next(i)) {
      AllocaCompatibility[i].reset(Current);
    }

    for (BasicBlock::const_iterator BI = BB->begin(), BE = BB->end();
         BI != BE; ++BI) {
      const CallInst *CI = dyn_cast<CallInst>(BI);
      if (!CI) continue;

      const Value *Callee = CI->getCalledValue();
      if (Callee == LifetimeStart) {
        if (const Value *Ptr = getPointerFromIntrinsic(CI)) {
          if (const AllocaInst *AI = isFavorableAlloca(Ptr)) {
            size_t AIndex = Allocas[AI];
            // We conflict with everything else that's currently live.
            AllocaCompatibility[AIndex].reset(Current);
            // Everything else that's currently live conflicts with us.
            for (int i = Current.find_first(); i >= 0; i = Current.find_next(i)) {
              AllocaCompatibility[i].reset(AIndex);
            }
            // We're now live.
            Current.set(AIndex);
          }
        }
      } else if (Callee == LifetimeEnd) {
        if (const Value *Ptr = getPointerFromIntrinsic(CI)) {
          if (const AllocaInst *AI = isFavorableAlloca(Ptr)) {
            size_t AIndex = Allocas[AI];
            // We're no longer live.
            Current.reset(AIndex);
          }
        }
      }
    }
  }
}

// Decide which allocas will represent which other allocas, and if so what their
// size and alignment will need to be.
void AllocaManager::computeRepresentatives() {
  NamedRegionTimer Timer("Compute Representatives", "AllocaManager",
                         TimePassesIsEnabled);

  for (size_t i = 0, e = AllocasByIndex.size(); i != e; ++i) {
    // If we've already represented this alloca with another, don't visit it.
    if (AllocasByIndex[i].isForwarded()) continue;
    if (i > size_t(INT_MAX)) continue;

    // Find compatible allocas. This is a simple greedy algorithm.
    for (int j = int(i); ; ) {
      assert(j >= int(i));
      j = AllocaCompatibility[i].find_next(j);
      assert(j != int(i));
      if (j < 0) break;
      if (!AllocaCompatibility[j][i]) continue;

      DEBUG(dbgs() << "Allocas: "
                      "Representing "
                   << AllocasByIndex[j].getInst()->getName() << " "
                      "with "
                   << AllocasByIndex[i].getInst()->getName() << "\n");
      ++NumAllocas;

      assert(!AllocasByIndex[j].isForwarded());

      AllocasByIndex[i].mergeSize(AllocasByIndex[j].getSize());
      AllocasByIndex[i].mergeAlignment(AllocasByIndex[j].getAlignment());
      AllocasByIndex[j].forward(i);

      AllocaCompatibility[i] &= AllocaCompatibility[j];
      AllocaCompatibility[j].reset();
    }
  }
}

void AllocaManager::computeFrameOffsets() {
  NamedRegionTimer Timer("Compute Frame Offsets", "AllocaManager",
                         TimePassesIsEnabled);

  // Walk through the entry block and collect all the allocas, including the
  // ones with no lifetime markers that we haven't looked at yet. We walk in
  // reverse order so that we can set the representative allocas as those that
  // dominate the others as we go.
  const BasicBlock *EntryBB = &F->getEntryBlock();
  for (BasicBlock::const_iterator BI = EntryBB->begin(), BE = EntryBB->end();
       BI != BE; ++BI) {
    const AllocaInst *AI = dyn_cast<AllocaInst>(BI);
    if (!AI || !AI->isStaticAlloca()) continue;

    AllocaMap::const_iterator I = Allocas.find(AI);
    if (I != Allocas.end()) {
      // An alloca with lifetime markers. Emit the record we've crafted for it,
      // if we've chosen to keep it as a representative.
      const AllocaInfo &Info = AllocasByIndex[I->second];
      if (!Info.isForwarded()) {
        SortedAllocas.push_back(Info);
      }
    } else {
      // An alloca with no lifetime markers.
      SortedAllocas.push_back(getInfo(AI, SortedAllocas.size()));
    }
  }

  // Sort the allocas to hopefully reduce padding.
  array_pod_sort(SortedAllocas.begin(), SortedAllocas.end(), AllocaSort);

  // Assign stack offsets.
  uint64_t CurrentOffset = 0;
  for (SmallVectorImpl<AllocaInfo>::const_iterator I = SortedAllocas.begin(),
       E = SortedAllocas.end(); I != E; ++I) {
    const AllocaInfo &Info = *I;
    uint64_t NewOffset = alignTo(CurrentOffset, Info.getAlignment());

    // For backwards compatibility, align every power-of-two multiple alloca to
    // its greatest power-of-two factor, up to 8 bytes. In particular, cube2hash
    // is known to depend on this.
    // TODO: Consider disabling this and making people fix their code.
    if (uint64_t Size = Info.getSize()) {
      uint64_t P2 = uint64_t(1) << countTrailingZeros(Size);
      unsigned CompatAlign = unsigned(std::min(P2, uint64_t(8)));
      NewOffset = alignTo(NewOffset, CompatAlign);
    }

    const AllocaInst *AI = Info.getInst();
    StaticAllocas[AI] = StaticAllocation(AI, NewOffset);

    CurrentOffset = NewOffset + Info.getSize();
  }

  // Add allocas that were represented by other allocas to the StaticAllocas map
  // so that our clients can look them up.
  for (unsigned i = 0, e = AllocasByIndex.size(); i != e; ++i) {
    const AllocaInfo &Info = AllocasByIndex[i];
    if (!Info.isForwarded()) continue;
    size_t j = Info.getForwardedID();
    assert(!AllocasByIndex[j].isForwarded());

    StaticAllocaMap::const_iterator I =
      StaticAllocas.find(AllocasByIndex[j].getInst());
    assert(I != StaticAllocas.end());

    std::pair<StaticAllocaMap::const_iterator, bool> Pair =
      StaticAllocas.insert(std::make_pair(AllocasByIndex[i].getInst(),
                                          I->second));
    assert(Pair.second); (void)Pair;
  }

  // Record the final frame size. Keep the stack pointer 16-byte aligned.
  FrameSize = CurrentOffset;
  FrameSize = alignTo(FrameSize, 16);

  DEBUG(dbgs() << "Allocas: "
                  "Statically allocated frame size is " << FrameSize << "\n");
}

AllocaManager::AllocaManager() : MaxAlignment(0) {
}

void AllocaManager::analyze(const Function &Func, const DataLayout &Layout,
                            bool PerformColoring) {
  NamedRegionTimer Timer("AllocaManager", TimePassesIsEnabled);
  assert(Allocas.empty());
  assert(AllocasByIndex.empty());
  assert(AllocaCompatibility.empty());
  assert(BlockLiveness.empty());
  assert(StaticAllocas.empty());
  assert(SortedAllocas.empty());

  DL = &Layout;
  F = &Func;

  // Get the declarations for the lifetime intrinsics so we can quickly test to
  // see if they are used at all, and for use later if they are.
  const Module *M = F->getParent();
  LifetimeStart = M->getFunction(Intrinsic::getName(Intrinsic::lifetime_start));
  LifetimeEnd = M->getFunction(Intrinsic::getName(Intrinsic::lifetime_end));

  // If we are optimizing and the module contains any lifetime intrinsics, run
  // the alloca coloring algorithm.
  if (PerformColoring &&
      ((LifetimeStart && !LifetimeStart->use_empty()) ||
       (LifetimeEnd   && !LifetimeEnd->use_empty()))) {

    collectMarkedAllocas();

    if (!AllocasByIndex.empty()) {
      DEBUG(dbgs() << "Allocas: "
                   << AllocasByIndex.size() << " marked allocas found\n");

      collectBlocks();
      computeInterBlockLiveness();
      computeIntraBlockLiveness();
      BlockLiveness.clear();

      computeRepresentatives();
      AllocaCompatibility.clear();
    }
  }

  computeFrameOffsets();
  SortedAllocas.clear();
  Allocas.clear();
  AllocasByIndex.clear();
}

void AllocaManager::clear() {
  StaticAllocas.clear();
}

bool
AllocaManager::getFrameOffset(const AllocaInst *AI, uint64_t *Offset) const {
  assert(AI->isStaticAlloca());
  StaticAllocaMap::const_iterator I = StaticAllocas.find(AI);
  assert(I != StaticAllocas.end());
  *Offset = I->second.Offset;
  return AI == I->second.Representative;
}

const AllocaInst *
AllocaManager::getRepresentative(const AllocaInst *AI) const {
  assert(AI->isStaticAlloca());
  StaticAllocaMap::const_iterator I = StaticAllocas.find(AI);
  assert(I != StaticAllocas.end());
  return I->second.Representative;
}
