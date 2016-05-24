//===-- AllocaManager.h ---------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass declares the AllocaManager class.
//
//===----------------------------------------------------------------------===//

#ifndef JSBACKEND_ALLOCAMANAGER_H
#define JSBACKEND_ALLOCAMANAGER_H

#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/SetVector.h"

namespace llvm {

class AllocaInst;
class BasicBlock;
class CallInst;
class DataLayout;
class Function;
class Value;

/// Compute frame layout for allocas.
class AllocaManager {
  const DataLayout *DL;
  const Function *LifetimeStart;
  const Function *LifetimeEnd;
  const Function *F;

  // Per-block lifetime information.
  struct BlockLifetimeInfo {
    BitVector Start;
    BitVector End;
    BitVector LiveIn;
    BitVector LiveOut;
  };
  typedef DenseMap<const BasicBlock *, BlockLifetimeInfo> LivenessMap;
  LivenessMap BlockLiveness;

  // Worklist for inter-block liveness analysis.
  typedef SmallSetVector<const BasicBlock *, 8> InterBlockWorklistVec;
  InterBlockWorklistVec InterBlockTopDownWorklist;
  InterBlockWorklistVec InterBlockBottomUpWorklist;

  // Map allocas to their index in AllocasByIndex.
  typedef DenseMap<const AllocaInst *, size_t> AllocaMap;
  AllocaMap Allocas;

  // Information about an alloca. Note that the size and alignment may vary
  // from what's in the actual AllocaInst when an alloca is also representing
  // another with perhaps greater size and/or alignment needs.
  //
  // When an alloca is represented by another, its AllocaInfo is marked as
  // "forwarded", at which point it no longer holds a size and alignment, but
  // the index of the representative AllocaInfo.
  class AllocaInfo {
    const AllocaInst *Inst;
    uint64_t Size;
    unsigned Alignment;
    unsigned Index;

  public:
    AllocaInfo(const AllocaInst *I, uint64_t S, unsigned A, unsigned X)
      : Inst(I), Size(S), Alignment(A), Index(X) {
      assert(I != NULL);
      assert(A != 0);
      assert(!isForwarded());
    }

    bool isForwarded() const { return Alignment == 0; }

    size_t getForwardedID() const {
      assert(isForwarded());
      return static_cast<size_t>(Size);
    }

    void forward(size_t i) {
      assert(!isForwarded());
      Alignment = 0;
      Size = i;
      assert(isForwarded());
      assert(getForwardedID() == i);
    }

    const AllocaInst *getInst() const { return Inst; }

    uint64_t getSize() const { assert(!isForwarded()); return Size; }
    unsigned getAlignment() const { assert(!isForwarded()); return Alignment; }
    unsigned getIndex() const { return Index; }

    void mergeSize(uint64_t S) {
      assert(!isForwarded());
      Size = std::max(Size, S);
      assert(!isForwarded());
    }
    void mergeAlignment(unsigned A) {
      assert(A != 0);
      assert(!isForwarded());
      Alignment = std::max(Alignment, A);
      assert(!isForwarded());
    }
  };
  typedef SmallVector<AllocaInfo, 32> AllocaVec;
  AllocaVec AllocasByIndex;

  // For each alloca, which allocas can it safely represent? Allocas are
  // identified by AllocasByIndex index.
  // TODO: Vector-of-vectors isn't the fastest data structure possible here.
  typedef SmallVector<BitVector, 32> AllocaCompatibilityVec;
  AllocaCompatibilityVec AllocaCompatibility;

  // This is for allocas that will eventually be sorted.
  SmallVector<AllocaInfo, 32> SortedAllocas;

  // Static allocation results.
  struct StaticAllocation {
    const AllocaInst *Representative;
    uint64_t Offset;
    StaticAllocation() {}
    StaticAllocation(const AllocaInst *A, uint64_t O)
      : Representative(A), Offset(O) {}
  };
  typedef DenseMap<const AllocaInst *, StaticAllocation> StaticAllocaMap;
  StaticAllocaMap StaticAllocas;
  uint64_t FrameSize;

  uint64_t getSize(const AllocaInst *AI);
  unsigned getAlignment(const AllocaInst *AI);
  AllocaInfo getInfo(const AllocaInst *AI, unsigned Index);
  const Value *getPointerFromIntrinsic(const CallInst *CI);
  const AllocaInst *isFavorableAlloca(const Value *V);
  static int AllocaSort(const AllocaInfo *l, const AllocaInfo *r);

  void collectMarkedAllocas();
  void collectBlocks();
  void computeInterBlockLiveness();
  void computeIntraBlockLiveness();
  void computeRepresentatives();
  void computeFrameOffsets();

  unsigned MaxAlignment;

public:
  AllocaManager();

  /// Analyze the given function and prepare for getRepresentative queries.
  void analyze(const Function &Func, const DataLayout &Layout,
               bool PerformColoring);

  /// Reset all stored state.
  void clear();

  /// Return the representative alloca for the given alloca. When allocas are
  /// merged, one is chosen as the representative to stand for the rest.
  /// References to the alloca should take the form of references to the
  /// representative.
  const AllocaInst *getRepresentative(const AllocaInst *AI) const;

  /// Set *offset to the frame offset for the given alloca. Return true if the
  /// given alloca is representative, meaning that it needs an explicit
  /// definition in the function entry. Return false if some other alloca
  /// represents this one.
  bool getFrameOffset(const AllocaInst *AI, uint64_t *offset) const;

  /// Return the total frame size for all static allocas and associated padding.
  uint64_t getFrameSize() const { return FrameSize; }

  /// Return the largest alignment seen.
  unsigned getMaxAlignment() const { return MaxAlignment; }
};

} // namespace llvm

#endif
