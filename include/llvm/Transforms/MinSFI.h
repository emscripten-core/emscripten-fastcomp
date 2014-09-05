//===-- MinSFI.h - MinSFI Transformations -----------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_MINSFI_H
#define LLVM_TRANSFORMS_MINSFI_H

#include "llvm/CodeGen/Passes.h"
#include "llvm/Support/DataTypes.h"

namespace llvm {
namespace minsfi {

// Returns the bit size of sandboxed pointers. The default value is 32 bits
// and it can be set with the "-minsfi-ptrsize" command-line option.
uint32_t GetPointerSizeInBits();

// Returns the number of bytes addressable by the sandbox. This value is given
// by the size of the sandboxed pointers.
uint64_t GetAddressSubspaceSize();

// The name of MinSFI's entry function which will be invoked by the runtime.
extern const char EntryFunctionName[];

}  // namespace minsfi

FunctionPass *createSubstituteUndefsPass();
ModulePass *createAllocateDataSegmentPass();
ModulePass *createExpandAllocasPass();
ModulePass *createRenameEntryPointPass();
ModulePass *createSandboxIndirectCallsPass();
ModulePass *createSandboxMemoryAccessesPass();

void MinSFIPasses(PassManagerBase &PM);

}  // namespace llvm

#endif
