//===-- JSBackendMCTargetDesc.cpp - JS Backend Target Descriptions --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file provides asm.js specific target descriptions.
//
//===----------------------------------------------------------------------===//

#include "JSBackendMCTargetDesc.h"
#include "llvm/Support/TargetRegistry.h"
using namespace llvm;

// Force static initialization.
extern "C" void LLVMInitializeJSBackendTargetMC() {
  // nothing to register
}

