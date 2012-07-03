//=-- NaClJITMemoryManager.h - Interface JIT uses to Allocate Mem -*- C++ -*-=//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//


#ifndef LLVM_EXECUTION_ENGINE_NACL_JIT_MEMMANAGER_H
#define LLVM_EXECUTION_ENGINE_NACL_JIT_MEMMANAGER_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/ExecutionEngine/JITMemoryManager.h"
#include "llvm/Support/Allocator.h"

namespace llvm {

class Function;
class GlobalValue;

struct SimpleSlab {
  uint8_t *address;
  size_t size;
  uint8_t *next_free;
};

struct FreeListNode {
  uint8_t *address;
  uintptr_t size;
  FreeListNode *Prev;
  FreeListNode *Next;
  FreeListNode *RemoveFromFreeList() {
    assert(Next->Prev == this && Prev->Next == this && "Freelist broken!");
    Next->Prev = Prev;
    return Prev->Next = Next;
  }
  void AddToFreeList(FreeListNode *FreeList) {
     Next = FreeList;
     Prev = FreeList->Prev;
     Prev->Next = this;
     Next->Prev = this;
  }
};

class NaClJITMemoryManager : public JITMemoryManager {
  // NaCl disallows writing into any code region, and disallows executing any
  // data region. Thus we can never get any RWX memory and the the strategy
  // used by the other allocators of colocation of allocation metadata
  // with the allocated code won't work.
  // Currently with NaCl we have one single pool of usable space between the
  // text and rodata segments, defined by the linker
  // so to support stub allocation in the middle of a function, we allocate
  // them in slabs interspersed with the functions.

  static const size_t kStubSlabSize = 16 * 1024;
  static const size_t kDataSlabSize = 16 * 1024;
  static const size_t kCodeSlabSize = 64 * 1024;

  typedef DenseMap<uint8_t *, size_t> AllocationTable;

  uint8_t *AllocatableRegionStart;
  uint8_t *AllocatableRegionLimit;
  uint8_t *NextCode;
  SimpleSlab CurrentStubSlab;

  // Allocation metadata must be kept separate from code, so the free list is
  // allocated with new rather than being a header in the code blocks
  FreeListNode *CodeFreeListHead;
  FreeListNode *CurrentCodeBlock;
  // Mapping from pointer to allocated function, to size of allocation
  AllocationTable AllocatedFunctions;

  // Since Exception tables are allocated like functions (i.e. we don't know
  // ahead of time how large they are) we use the same allocation method for
  // simplicity even though it's not strictly necessary to separate the
  // allocation metadata from the allocated data.
  FreeListNode *DataFreeListHead;
  FreeListNode *CurrentDataBlock;
  AllocationTable AllocatedTables;
  BumpPtrAllocator DataAllocator;

  uint8_t *GOTBase;     // Target Specific reserved memory

  FreeListNode *allocateCodeSlab(size_t MinSize);
  FreeListNode *allocateDataSlab(size_t MinSize);
  SimpleSlab allocateStubSlab(size_t MinSize);

  // Functions for allocations using one of the free lists
  void InitFreeList(FreeListNode **Head);
  void DestroyFreeList(FreeListNode *Head);
  FreeListNode *FreeListAllocate(uintptr_t &ActualSize, FreeListNode *Head,
      FreeListNode * (NaClJITMemoryManager::*allocate)(size_t));
  void FreeListFinishAllocation(FreeListNode *Block, FreeListNode *Head,
      uint8_t *AllocationStart, uint8_t *AllocationEnd, AllocationTable &table);
  void FreeListDeallocate(FreeListNode *Head, AllocationTable &Table,
                          void *Body);
 public:
  // TODO(dschuff): how to find the real value? is it a flag?
  static const int kBundleSize = 32;
  static const intptr_t kJumpMask = -32;
  NaClJITMemoryManager();
  virtual ~NaClJITMemoryManager();
  static inline bool classof(const JITMemoryManager*) { return true; }

  /// setMemoryWritable - No-op on NaCl - code is never writable
  virtual void setMemoryWritable() {}

  /// setMemoryExecutable - No-op on NaCl - data is never executable
  virtual void setMemoryExecutable() {}

  /// setPoisonMemory - No-op on NaCl - nothing unvalidated is ever executable
  virtual void setPoisonMemory(bool poison) {}

  /// getPointerToNamedFunction - This method returns the address of the
  /// specified function. As such it is only useful for resolving library
  /// symbols, not code generated symbols.
  ///
  /// If AbortOnFailure is false and no function with the given name is
  /// found, this function silently returns a null pointer. Otherwise,
  /// it prints a message to stderr and aborts.
  ///
  virtual void *getPointerToNamedFunction(const std::string &Name,
                                          bool AbortOnFailure = true) ;

  //===--------------------------------------------------------------------===//
  // Global Offset Table Management
  //===--------------------------------------------------------------------===//

  /// AllocateGOT - If the current table requires a Global Offset Table, this
  /// method is invoked to allocate it.  This method is required to set HasGOT
  /// to true.
  virtual void AllocateGOT();

  /// getGOTBase - If this is managing a Global Offset Table, this method should
  /// return a pointer to its base.
  virtual uint8_t *getGOTBase() const {
    return GOTBase;
  }

  //===--------------------------------------------------------------------===//
  // Main Allocation Functions
  //===--------------------------------------------------------------------===//

  /// startFunctionBody - When we start JITing a function, the JIT calls this
  /// method to allocate a block of free RWX memory, which returns a pointer to
  /// it.  If the JIT wants to request a block of memory of at least a certain
  /// size, it passes that value as ActualSize, and this method returns a block
  /// with at least that much space.  If the JIT doesn't know ahead of time how
  /// much space it will need to emit the function, it passes 0 for the
  /// ActualSize.  In either case, this method is required to pass back the size
  /// of the allocated block through ActualSize.  The JIT will be careful to
  /// not write more than the returned ActualSize bytes of memory.
  virtual uint8_t *startFunctionBody(const Function *F,
                                     uintptr_t &ActualSize);

  /// allocateStub - This method is called by the JIT to allocate space for a
  /// function stub (used to handle limited branch displacements) while it is
  /// JIT compiling a function.  For example, if foo calls bar, and if bar
  /// either needs to be lazily compiled or is a native function that exists too
  /// far away from the call site to work, this method will be used to make a
  /// thunk for it.  The stub should be "close" to the current function body,
  /// but should not be included in the 'actualsize' returned by
  /// startFunctionBody.
  virtual uint8_t *allocateStub(const GlobalValue* F, unsigned StubSize,
                                unsigned Alignment);

  /// endFunctionBody - This method is called when the JIT is done codegen'ing
  /// the specified function.  At this point we know the size of the JIT
  /// compiled function.  This passes in FunctionStart (which was returned by
  /// the startFunctionBody method) and FunctionEnd which is a pointer to the
  /// actual end of the function.  This method should mark the space allocated
  /// and remember where it is in case the client wants to deallocate it.
  virtual void endFunctionBody(const Function *F, uint8_t *FunctionStart,
                               uint8_t *FunctionEnd);

  /// allocateCodeSection - Allocate a memory block of (at least) the given
  /// size suitable for executable code. The SectionID is a unique identifier
  /// assigned by the JIT and passed through to the memory manager for
  /// the instance class to use if it needs to communicate to the JIT about
  /// a given section after the fact.
  virtual uint8_t *allocateCodeSection(uintptr_t Size, unsigned Alignment,
                                       unsigned SectionID);

  /// allocateDataSection - Allocate a memory block of (at least) the given
  /// size suitable for data. The SectionID is a unique identifier
  /// assigned by the JIT and passed through to the memory manager for
  /// the instance class to use if it needs to communicate to the JIT about
  /// a given section after the fact.
  virtual uint8_t *allocateDataSection(uintptr_t Size, unsigned Alignment,
                                       unsigned SectionID);

  /// allocateSpace - Allocate a memory block of the given size.  This method
  /// cannot be called between calls to startFunctionBody and endFunctionBody.
  virtual uint8_t *allocateSpace(intptr_t Size, unsigned Alignment);

  /// allocateGlobal - Allocate memory for a global.
  virtual uint8_t *allocateGlobal(uintptr_t Size, unsigned Alignment);

  /// deallocateFunctionBody - Free the specified function body.  The argument
  /// must be the return value from a call to startFunctionBody() that hasn't
  /// been deallocated yet.  This is never called when the JIT is currently
  /// emitting a function.
  virtual void deallocateFunctionBody(void *Body);

  /// startExceptionTable - When we finished JITing the function, if exception
  /// handling is set, we emit the exception table.
  virtual uint8_t* startExceptionTable(const Function* F,
                                       uintptr_t &ActualSize);

  /// endExceptionTable - This method is called when the JIT is done emitting
  /// the exception table.
  virtual void endExceptionTable(const Function *F, uint8_t *TableStart,
                                 uint8_t *TableEnd, uint8_t* FrameRegister);

  /// deallocateExceptionTable - Free the specified exception table's memory.
  /// The argument must be the return value from a call to startExceptionTable()
  /// that hasn't been deallocated yet.  This is never called when the JIT is
  /// currently emitting an exception table.
  virtual void deallocateExceptionTable(void *ET);

  virtual size_t GetDefaultCodeSlabSize() {
    return kCodeSlabSize;
  }
  virtual size_t GetDefaultDataSlabSize() {
    return kDataSlabSize;
  }
  virtual size_t GetDefaultStubSlabSize() {
    return kStubSlabSize;
  }

};

}

#endif  // LLVM_EXECUTION_ENGINE_NACL_JIT_MEMMANAGER_H
