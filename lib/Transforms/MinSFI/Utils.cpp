//===-- Utils.cpp - Helper functions for MinSFI passes --------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/MinSFI.h"

using namespace llvm;

static cl::opt<uint32_t>
PointerSizeInBits("minsfi-ptrsize", cl::init(32),
                  cl::desc("Size of the address subspace in bits"));

uint32_t minsfi::GetPointerSizeInBits() {
  if (PointerSizeInBits < 20 || PointerSizeInBits > 32)
    report_fatal_error("MinSFI: Size of the sandboxed pointers is out of "
                       "bounds (20-32)");
  return PointerSizeInBits;
}

uint64_t minsfi::GetAddressSubspaceSize() {
  return 1LL << GetPointerSizeInBits();
}
