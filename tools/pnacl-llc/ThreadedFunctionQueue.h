//===- ThreadedFunctionQueue.h - Function work units for threads -*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef THREADEDFUNCTIONQUEUE_H
#define THREADEDFUNCTIONQUEUE_H

#include <limits>

#include "llvm/IR/Module.h"
#include "llvm/Support/ErrorHandling.h"

namespace llvm {

// A "queue" that keeps track of which functions have been assigned to
// threads and which functions have not yet been assigned. It does not
// actually use a queue data structure and instead uses a number which
// tracks the minimum unassigned function ID, expecting each thread
// to have the same view of function IDs.
class ThreadedFunctionQueue {
 public:
  ThreadedFunctionQueue(Module *mod, unsigned NumThreads)
      : NumThreads(NumThreads),
        NumFunctions(0),
        CurrentFunction(0) {
    assert(NumThreads > 0);
    size_t Size = 0;
    for (Module::iterator I = mod->begin(), E = mod->end(); I != E; ++I) {
      // Only count functions with bodies. At this point nothing should
      // be "already materialized", so if functions with bodies are
      // materializable.
      if (I->isMaterializable() || !I->isDeclaration())
        Size++;
    }
    if (Size > static_cast<size_t>(std::numeric_limits<int>::max()))
      report_fatal_error("Too many functions");
    NumFunctions = Size;
  }

  ~ThreadedFunctionQueue() {}

  // Assign functions in a static manner between threads.
  bool GrabFunctionStatic(unsigned FuncIndex, unsigned ThreadIndex) const {
    // Note: This assumes NumThreads == SplitModuleCount, so that
    // (a) every function of every module is covered by the NumThreads and
    // (b) no function is covered twice by the threads.
    assert(ThreadIndex < NumThreads);
    return FuncIndex % NumThreads == ThreadIndex;
  }

  // Assign functions between threads dynamically.
  // Returns true if FuncIndex is unassigned and the calling thread
  // is assigned functions [FuncIndex, FuncIndex + ChunkSize).
  // Returns false if the calling thread is not assigned functions
  // [FuncIndex, FuncIndex + ChunkSize).
  //
  // NextIndex will be set to the next unassigned function ID, so that
  // the calling thread will know which function ID to attempt to grab
  // next. Each thread may have a different value for the ideal ChunkSize
  // so it is hard to predict the next available function solely based
  // on incrementing by ChunkSize.
  bool GrabFunctionDynamic(unsigned FuncIndex, unsigned ChunkSize,
                           unsigned &NextIndex) {
    unsigned Cur = CurrentFunction;
    if (FuncIndex < Cur) {
      NextIndex = Cur;
      return false;
    }
    NextIndex = Cur + ChunkSize;
    unsigned Index;
    if (Cur == (Index = __sync_val_compare_and_swap(&CurrentFunction,
                                                    Cur, NextIndex))) {
      return true;
    }
    // If this thread did not grab the function, its idea of NextIndex
    // may be incorrect since ChunkSize can vary between threads.
    // Reset NextIndex in that case.
    NextIndex = Index;
    return false;
  }

  // Returns a recommended ChunkSize for use in calling GrabFunctionDynamic().
  // ChunkSize starts out "large" to reduce synchronization cost. However,
  // it cannot be too large, otherwise it will encompass too many bytes
  // and defeats streaming translation. Assigning too many functions to
  // a single thread also throws off load balancing, so the ChunkSize is
  // reduced when the remaining number of functions is low so that
  // load balancing can be achieved near the end.
  unsigned RecommendedChunkSize() const {
    int RemainingFuncs = NumFunctions - CurrentFunction;
    int DynamicChunkSize = RemainingFuncs / (NumThreads * 4);
    return std::max(1, std::min(8, DynamicChunkSize));
  }

  // Total number of functions with bodies that should be processed.
  unsigned Size() const {
    return NumFunctions;
  }

 private:
  const unsigned NumThreads;
  unsigned NumFunctions;
  volatile unsigned CurrentFunction;

  ThreadedFunctionQueue(
      const ThreadedFunctionQueue&) LLVM_DELETED_FUNCTION;
  void operator=(const ThreadedFunctionQueue&) LLVM_DELETED_FUNCTION;
};

} // end namespace llvm

#endif
