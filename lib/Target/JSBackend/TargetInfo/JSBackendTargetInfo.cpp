//===-- JSBackendTargetInfo.cpp - JSBackend Target Implementation -------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===--------------------------------------------------------------------===//

#include "JSTargetMachine.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/TargetRegistry.h"
using namespace llvm;

Target llvm::TheJSBackendTarget;

static unsigned JSBackend_TripleMatchQuality(const std::string &TT) {
  // This class always works, but shouldn't be the default in most cases.
  return 1;
}

extern "C" void LLVMInitializeJSBackendTargetInfo() { 
  TargetRegistry::RegisterTarget(TheJSBackendTarget, "js",
                                 "JavaScript (asm.js, emscripten) backend",
                                 &JSBackend_TripleMatchQuality);
}

extern "C" void LLVMInitializeJSBackendTargetMC() {}
