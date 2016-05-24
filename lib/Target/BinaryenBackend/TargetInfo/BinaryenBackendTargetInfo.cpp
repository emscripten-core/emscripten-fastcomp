//===-- BinaryenBackendTargetInfo.cpp - BinaryenBackend Target Implementation -------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===--------------------------------------------------------------------===//

#include "JSTargetMachine.h"
#include "MCTargetDesc/BinaryenBackendMCTargetDesc.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/TargetRegistry.h"
using namespace llvm;

Target llvm::TheBinaryenBackendTarget;

extern "C" void LLVMInitializeBinaryenBackendTargetInfo() { 
  RegisterTarget<Triple::asmjs, /*HasJIT=*/false> X(TheBinaryenBackendTarget, "js", "JavaScript (asm.js, emscripten) backend");
}
