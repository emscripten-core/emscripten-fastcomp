//===-- JSBackendTargetInfo.cpp - JSBackend Target Implementation -------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===--------------------------------------------------------------------===//

#include "JSTargetMachine.h"
#include "MCTargetDesc/JSBackendMCTargetDesc.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/TargetRegistry.h"
using namespace llvm;

Target llvm::TheJSBackendTarget;

static bool JSBackend_TripleMatchQuality(Triple::ArchType Arch) {
  switch (Arch) {
  case Triple::asmjs:
    // That's us!
    return true;

  case Triple::le32:
    // For compatibility with older versions of Emscripten, we also basically
    // support generating code for le32-unknown-nacl
    return true;

  case Triple::x86:
  default:
    return false;
  }
}

extern "C" void LLVMInitializeJSBackendTargetInfo() { 
  TargetRegistry::RegisterTarget(TheJSBackendTarget, "js",
                                 "JavaScript (asm.js, emscripten) backend",
                                 &JSBackend_TripleMatchQuality);
}
