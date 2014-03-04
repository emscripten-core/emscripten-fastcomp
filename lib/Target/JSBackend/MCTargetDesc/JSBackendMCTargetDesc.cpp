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
#include "llvm/MC/MCCodeGenInfo.h"
#include "llvm/Support/TargetRegistry.h"
using namespace llvm;

static MCCodeGenInfo *createJSBackendMCCodeGenInfo(StringRef Triple, Reloc::Model RM,
                                                   CodeModel::Model CM,
                                                   CodeGenOpt::Level OL) {
  MCCodeGenInfo *X = new MCCodeGenInfo();
  X->InitMCCodeGenInfo(RM, CM, OL);
  return X;
}

// Force static initialization.
extern "C" void LLVMInitializeJSBackendTargetMC() {
  // Register the MC codegen info.
  RegisterMCCodeGenInfoFn C(TheJSBackendTarget, createJSBackendMCCodeGenInfo);
}
