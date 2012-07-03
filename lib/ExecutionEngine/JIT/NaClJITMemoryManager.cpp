//===-- NaClJITMemoryManager.cpp - Memory Allocator for JIT'd code --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the NaClJITMemoryManager class.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "jit"
#include "llvm/ExecutionEngine/NaClJITMemoryManager.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/DynamicLibrary.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/raw_ostream.h"
#include <vector>

#if defined(__linux__) || defined(__native_client__)
#if defined(HAVE_SYS_STAT_H)
#include <sys/stat.h>
#endif
#include <fcntl.h>
#include <unistd.h>
#endif

using namespace llvm;

#ifdef __native_client__
// etext is guarded by ifdef so the code still compiles on non-ELF platforms
extern char etext;
#endif

// The way NaCl linking is currently setup, there is a gap between the text
// segment and the rodata segment where we can fill dyncode. The text ends
// at etext, but there's no symbol for the start of rodata. Currently the
// linker script puts it at 0x11000000
// If we run out of space there, we can also allocate below the text segment
// and keep going downward until we run into code loaded by the dynamic
// linker. (TODO(dschuff): make that work)
// For now, just start at etext and go until we hit rodata

// It's an open issue that lazy jitting is not thread safe (PR5184). However
// NaCl's dyncode_create solves exactly this problem, so in the future
// this allocator could (should?) be made thread safe

const size_t NaClJITMemoryManager::kStubSlabSize;
const size_t NaClJITMemoryManager::kDataSlabSize;
const size_t NaClJITMemoryManager::kCodeSlabSize;

// TODO(dschuff) fix allocation start (etext + 64M is hopefully after where
// glibc is loaded) and limit (maybe need a linker-provide symbol for the start
// of the IRT or end of the segment gap)
// (also fix allocateCodeSlab and maybe allocateStubSlab at that time)
// what we really need is a usable nacl_dyncode_alloc(), but this could still
// be improved upon using dl_iterate_phdr
const static intptr_t kNaClSegmentGapEnd = 0x11000000;

NaClJITMemoryManager::NaClJITMemoryManager() :
    AllocatableRegionLimit((uint8_t *)kNaClSegmentGapEnd),
    NextCode(AllocatableRegionStart), GOTBase(NULL) {
#ifdef __native_client__
  AllocatableRegionStart = (uint8_t *)&etext + 1024*1024*64;
#else
    assert(false && "NaClJITMemoryManager will not work outside NaCl sandbox");
#endif
  AllocatableRegionStart =
      (uint8_t *)RoundUpToAlignment((uint64_t)AllocatableRegionStart,
                                    kBundleSize);
  NextCode = AllocatableRegionStart;

  // Allocate 1 stub slab to get us started
  CurrentStubSlab = allocateStubSlab(0);
  InitFreeList(&CodeFreeListHead);
  InitFreeList(&DataFreeListHead);

  DEBUG(dbgs() << "NaClJITMemoryManager: AllocatableRegionStart " <<
        AllocatableRegionStart << " Limit " << AllocatableRegionLimit << "\n");
}

NaClJITMemoryManager::~NaClJITMemoryManager() {
  delete [] GOTBase;
  DestroyFreeList(CodeFreeListHead);
  DestroyFreeList(DataFreeListHead);
}

FreeListNode *NaClJITMemoryManager::allocateCodeSlab(size_t MinSize) {
  FreeListNode *node = new FreeListNode();
  if (AllocatableRegionLimit - NextCode < (int)kCodeSlabSize) {
    // TODO(dschuff): might be possible to try the space below text segment?
    report_fatal_error("Ran out of code space");
  }
  node->address = NextCode;
  node->size = std::max(kCodeSlabSize, MinSize);
  NextCode += node->size;
  DEBUG(dbgs() << "allocated code slab " << NextCode - node->size << "-" <<
        NextCode << "\n");
  return node;
}

SimpleSlab NaClJITMemoryManager::allocateStubSlab(size_t MinSize) {
  SimpleSlab s;
  DEBUG(dbgs() << "allocateStubSlab: ");
  // It's a little weird to just allocate and throw away the FreeListNode, but
  // since code region allocation is still a bit ugly and magical, I decided
  // it's better to reuse allocateCodeSlab than duplicate the logic.
  FreeListNode *n = allocateCodeSlab(MinSize);
  s.address = n->address;
  s.size = n->size;
  s.next_free = n->address;
  delete n;
  return s;
}

FreeListNode *NaClJITMemoryManager::allocateDataSlab(size_t MinSize) {
  FreeListNode *node = new FreeListNode;
  size_t size = std::max(kDataSlabSize, MinSize);
  node->address = (uint8_t*)DataAllocator.Allocate(size, kBundleSize);
  node->size = size;
  return node;
}

void NaClJITMemoryManager::InitFreeList(FreeListNode **Head) {
  // Make sure there is always at least one entry in the free list
  *Head = new FreeListNode;
  (*Head)->Next = (*Head)->Prev = *Head;
  (*Head)->size = 0;
}

void NaClJITMemoryManager::DestroyFreeList(FreeListNode *Head) {
  FreeListNode *n = Head->Next;
  while(n != Head) {
    FreeListNode *next = n->Next;
    delete n;
    n = next;
  }
  delete Head;
}

FreeListNode *NaClJITMemoryManager::FreeListAllocate(uintptr_t &ActualSize,
    FreeListNode *Head,
    FreeListNode * (NaClJITMemoryManager::*allocate)(size_t)) {
  FreeListNode *candidateBlock = Head;
  FreeListNode *iter = Head->Next;

  uintptr_t largest = candidateBlock->size;
  // Search for the largest free block
  while (iter != Head) {
    if (iter->size > largest) {
      largest = iter->size;
      candidateBlock = iter;
    }
    iter = iter->Next;
  }

  if (largest < ActualSize || largest == 0) {
    candidateBlock = (this->*allocate)(ActualSize);
  } else {
    candidateBlock->RemoveFromFreeList();
  }
  return candidateBlock;
}

void NaClJITMemoryManager::FreeListFinishAllocation(FreeListNode *Block,
    FreeListNode *Head, uint8_t *AllocationStart, uint8_t *AllocationEnd,
    AllocationTable &Table) {
  assert(AllocationEnd > AllocationStart);
  assert(Block->address == AllocationStart);
  uint8_t *End = (uint8_t *)RoundUpToAlignment((uint64_t)AllocationEnd,
                                               kBundleSize);
  assert(End <= Block->address + Block->size);
  int AllocationSize = End - Block->address;
  Table[AllocationStart] = AllocationSize;

  Block->size -= AllocationSize;
  if (Block->size >= kBundleSize * 2) {//TODO(dschuff): better heuristic?
    Block->address = End;
    Block->AddToFreeList(Head);
  } else {
    delete Block;
  }
  DEBUG(dbgs()<<"FinishAllocation size "<< AllocationSize <<" end "<<End<<"\n");
}

void NaClJITMemoryManager::FreeListDeallocate(FreeListNode *Head,
                                              AllocationTable &Table,
                                              void *Body) {
  uint8_t *Allocation = (uint8_t *)Body;
  DEBUG(dbgs() << "deallocating "<<Body<<" ");
  assert(Table.count(Allocation) && "FreeList Deallocation not found in table");
  FreeListNode *Block = new FreeListNode;
  Block->address = Allocation;
  Block->size = Table[Allocation];
  Block->AddToFreeList(Head);
  DEBUG(dbgs() << "deallocated "<< Allocation<< " size " << Block->size <<"\n");
}

uint8_t *NaClJITMemoryManager::startFunctionBody(const Function *F,
                                                 uintptr_t &ActualSize) {
  CurrentCodeBlock = FreeListAllocate(ActualSize, CodeFreeListHead,
                                  &NaClJITMemoryManager::allocateCodeSlab);
  DEBUG(dbgs() << "startFunctionBody CurrentBlock " << CurrentCodeBlock <<
        " addr " << CurrentCodeBlock->address << "\n");
  ActualSize = CurrentCodeBlock->size;
  return CurrentCodeBlock->address;
}

void NaClJITMemoryManager::endFunctionBody(const Function *F,
                                           uint8_t *FunctionStart,
                                           uint8_t *FunctionEnd) {
  DEBUG(dbgs() << "endFunctionBody ");
  FreeListFinishAllocation(CurrentCodeBlock, CodeFreeListHead,
                           FunctionStart, FunctionEnd, AllocatedFunctions);

}

uint8_t *NaClJITMemoryManager::allocateCodeSection(uintptr_t Size,
                                                   unsigned Alignment,
                                                   unsigned SectionID) {
  llvm_unreachable("Implement me! (or don't.)");
}

uint8_t *NaClJITMemoryManager::allocateDataSection(uintptr_t Size,
                                                   unsigned Alignment,
                                                   unsigned SectionID) {
  return (uint8_t *)DataAllocator.Allocate(Size, Alignment);
}

void NaClJITMemoryManager::deallocateFunctionBody(void *Body) {
  DEBUG(dbgs() << "deallocateFunctionBody, ");
  if (Body) FreeListDeallocate(CodeFreeListHead, AllocatedFunctions, Body);
}

uint8_t *NaClJITMemoryManager::allocateStub(const GlobalValue* F,
                                            unsigned StubSize,
                                            unsigned Alignment) {
  uint8_t *StartAddress = (uint8_t *)(uintptr_t)
      RoundUpToAlignment((uintptr_t)CurrentStubSlab.next_free, Alignment);
  if (StartAddress + StubSize >
      CurrentStubSlab.address + CurrentStubSlab.size) {
    CurrentStubSlab = allocateStubSlab(kStubSlabSize);
    StartAddress = (uint8_t *)(uintptr_t)
        RoundUpToAlignment((uintptr_t)CurrentStubSlab.next_free, Alignment);
  }
  CurrentStubSlab.next_free = StartAddress + StubSize;
  DEBUG(dbgs() <<"allocated stub "<<StartAddress<< " size "<<StubSize<<"\n");
  return StartAddress;
}

uint8_t *NaClJITMemoryManager::allocateSpace(intptr_t Size,
                                             unsigned Alignment) {
  uint8_t *r = (uint8_t*)DataAllocator.Allocate(Size, Alignment);
  DEBUG(dbgs() << "allocateSpace " << Size <<"/"<<Alignment<<" ret "<<r<<"\n");
  return r;
}

uint8_t *NaClJITMemoryManager::allocateGlobal(uintptr_t Size,
                                              unsigned Alignment) {
  uint8_t *r = (uint8_t*)DataAllocator.Allocate(Size, Alignment);
  DEBUG(dbgs() << "allocateGlobal " << Size <<"/"<<Alignment<<" ret "<<r<<"\n");
  return r;
}

uint8_t* NaClJITMemoryManager::startExceptionTable(const Function* F,
                                                   uintptr_t &ActualSize) {
  CurrentDataBlock = FreeListAllocate(ActualSize, DataFreeListHead,
                                      &NaClJITMemoryManager::allocateDataSlab);
  DEBUG(dbgs() << "startExceptionTable CurrentBlock " << CurrentDataBlock <<
        " addr " << CurrentDataBlock->address << "\n");
  ActualSize = CurrentDataBlock->size;
  return CurrentDataBlock->address;
}

void NaClJITMemoryManager::endExceptionTable(const Function *F,
                                           uint8_t *TableStart,
                       uint8_t *TableEnd, uint8_t* FrameRegister) {
  DEBUG(dbgs() << "endExceptionTable ");
  FreeListFinishAllocation(CurrentDataBlock, DataFreeListHead,
                           TableStart, TableEnd, AllocatedTables);
}

void NaClJITMemoryManager::deallocateExceptionTable(void *ET) {
  DEBUG(dbgs() << "deallocateExceptionTable, ");
  if (ET) FreeListDeallocate(DataFreeListHead, AllocatedTables, ET);
}

// Copy of DefaultJITMemoryManager's implementation
void NaClJITMemoryManager::AllocateGOT() {
  assert(GOTBase == 0 && "Cannot allocate the got multiple times");
  GOTBase = new uint8_t[sizeof(void*) * 8192];
  HasGOT = true;
}

//===----------------------------------------------------------------------===//
// getPointerToNamedFunction() implementation.
// This code is pasted directly from r153607 of JITMemoryManager.cpp and has
// never been tested. It most likely doesn't work inside the sandbox.
//===----------------------------------------------------------------------===//

// AtExitHandlers - List of functions to call when the program exits,
// registered with the atexit() library function.
static std::vector<void (*)()> AtExitHandlers;

/// runAtExitHandlers - Run any functions registered by the program's
/// calls to atexit(3), which we intercept and store in
/// AtExitHandlers.
///
static void runAtExitHandlers() {
  while (!AtExitHandlers.empty()) {
    void (*Fn)() = AtExitHandlers.back();
    AtExitHandlers.pop_back();
    Fn();
  }
}

//===----------------------------------------------------------------------===//
// Function stubs that are invoked instead of certain library calls
//
// Force the following functions to be linked in to anything that uses the
// JIT. This is a hack designed to work around the all-too-clever Glibc
// strategy of making these functions work differently when inlined vs. when
// not inlined, and hiding their real definitions in a separate archive file
// that the dynamic linker can't see. For more info, search for
// 'libc_nonshared.a' on Google, or read http://llvm.org/PR274.
#if defined(__linux__)
/* stat functions are redirecting to __xstat with a version number.  On x86-64
 * linking with libc_nonshared.a and -Wl,--export-dynamic doesn't make 'stat'
 * available as an exported symbol, so we have to add it explicitly.
 */
namespace {
class StatSymbols {
public:
  StatSymbols() {
    sys::DynamicLibrary::AddSymbol("stat", (void*)(intptr_t)stat);
    sys::DynamicLibrary::AddSymbol("fstat", (void*)(intptr_t)fstat);
    sys::DynamicLibrary::AddSymbol("lstat", (void*)(intptr_t)lstat);
    sys::DynamicLibrary::AddSymbol("stat64", (void*)(intptr_t)stat64);
    sys::DynamicLibrary::AddSymbol("\x1stat64", (void*)(intptr_t)stat64);
    sys::DynamicLibrary::AddSymbol("\x1open64", (void*)(intptr_t)open64);
    sys::DynamicLibrary::AddSymbol("\x1lseek64", (void*)(intptr_t)lseek64);
    sys::DynamicLibrary::AddSymbol("fstat64", (void*)(intptr_t)fstat64);
    sys::DynamicLibrary::AddSymbol("lstat64", (void*)(intptr_t)lstat64);
    sys::DynamicLibrary::AddSymbol("atexit", (void*)(intptr_t)atexit);
    sys::DynamicLibrary::AddSymbol("mknod", (void*)(intptr_t)mknod);
  }
};
}
static StatSymbols initStatSymbols;
#endif // __linux__

// jit_exit - Used to intercept the "exit" library call.
static void jit_exit(int Status) {
  runAtExitHandlers();   // Run atexit handlers...
  exit(Status);
}

// jit_atexit - Used to intercept the "atexit" library call.
static int jit_atexit(void (*Fn)()) {
  AtExitHandlers.push_back(Fn);    // Take note of atexit handler...
  return 0;  // Always successful
}

static int jit_noop() {
  return 0;
}

//===----------------------------------------------------------------------===//
//
/// getPointerToNamedFunction - This method returns the address of the specified
/// function by using the dynamic loader interface.  As such it is only useful
/// for resolving library symbols, not code generated symbols.
///
void *NaClJITMemoryManager::getPointerToNamedFunction(const std::string &Name,
                                     bool AbortOnFailure) {
  // Check to see if this is one of the functions we want to intercept.  Note,
  // we cast to intptr_t here to silence a -pedantic warning that complains
  // about casting a function pointer to a normal pointer.
  if (Name == "exit") return (void*)(intptr_t)&jit_exit;
  if (Name == "atexit") return (void*)(intptr_t)&jit_atexit;

  // We should not invoke parent's ctors/dtors from generated main()!
  // On Mingw and Cygwin, the symbol __main is resolved to
  // callee's(eg. tools/lli) one, to invoke wrong duplicated ctors
  // (and register wrong callee's dtors with atexit(3)).
  // We expect ExecutionEngine::runStaticConstructorsDestructors()
  // is called before ExecutionEngine::runFunctionAsMain() is called.
  if (Name == "__main") return (void*)(intptr_t)&jit_noop;

  const char *NameStr = Name.c_str();
  // If this is an asm specifier, skip the sentinal.
  if (NameStr[0] == 1) ++NameStr;

  // If it's an external function, look it up in the process image...
  void *Ptr = sys::DynamicLibrary::SearchForAddressOfSymbol(NameStr);
  if (Ptr) return Ptr;

  // If it wasn't found and if it starts with an underscore ('_') character,
  // try again without the underscore.
  if (NameStr[0] == '_') {
    Ptr = sys::DynamicLibrary::SearchForAddressOfSymbol(NameStr+1);
    if (Ptr) return Ptr;
  }

  // Darwin/PPC adds $LDBLStub suffixes to various symbols like printf.  These
  // are references to hidden visibility symbols that dlsym cannot resolve.
  // If we have one of these, strip off $LDBLStub and try again.
#if defined(__APPLE__) && defined(__ppc__)
  if (Name.size() > 9 && Name[Name.size()-9] == '$' &&
      memcmp(&Name[Name.size()-8], "LDBLStub", 8) == 0) {
    // First try turning $LDBLStub into $LDBL128. If that fails, strip it off.
    // This mirrors logic in libSystemStubs.a.
    std::string Prefix = std::string(Name.begin(), Name.end()-9);
    if (void *Ptr = getPointerToNamedFunction(Prefix+"$LDBL128", false))
      return Ptr;
    if (void *Ptr = getPointerToNamedFunction(Prefix, false))
      return Ptr;
  }
#endif

  if (AbortOnFailure) {
    report_fatal_error("Program used external function '"+Name+
                      "' which could not be resolved!");
  }
  return 0;
}
