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

static unsigned JSBackend_TripleMatchQuality(const std::string &TT) {
  switch (Triple(TT).getArch()) {
  case Triple::asmjs:
    // That's us!
    return 20;

  case Triple::le32:
  case Triple::x86:
    // For compatibility with older versions of Emscripten, we also basically
    // support generating code for le32-unknown-nacl and i386-pc-linux-gnu,
    // but we use a low number here so that we're not the default.
    return 1;

  default:
    return 0;
  }
}

extern "C" void LLVMInitializeJSBackendTargetInfo() {
  TargetRegistry::RegisterTarget(TheJSBackendTarget, "js",
                                 "JavaScript (asm.js, emscripten) backend",
                                 &JSBackend_TripleMatchQuality);
}
