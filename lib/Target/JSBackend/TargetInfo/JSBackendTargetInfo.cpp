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

extern "C" void LLVMInitializeJSBackendTargetInfo() { 
  RegisterTarget<Triple::asmjs, /*HasJIT=*/false> X(TheJSBackendTarget, "js", "JavaScript (asm.js, emscripten) backend");
}
