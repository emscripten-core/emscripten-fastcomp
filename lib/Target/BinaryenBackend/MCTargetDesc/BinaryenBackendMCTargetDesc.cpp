//===-- BinaryenBackendMCTargetDesc.cpp - JS Backend Target Descriptions --------===//
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

#include "BinaryenBackendMCTargetDesc.h"
#include "llvm/MC/MCCodeGenInfo.h"
#include "llvm/Support/TargetRegistry.h"
using namespace llvm;


static MCCodeGenInfo *createBinaryenBackendMCCodeGenInfo(const Triple &TT, Reloc::Model RM,
                                                   CodeModel::Model CM,
                                                   CodeGenOpt::Level OL) {
  MCCodeGenInfo *X = new MCCodeGenInfo();
  X->initMCCodeGenInfo(RM, CM, OL);
  return X;
}

// Force static initialization.
extern "C" void LLVMInitializeBinaryenBackendTargetMC() {
  // Register the MC codegen info.
  RegisterMCCodeGenInfoFn C(TheBinaryenBackendTarget, createBinaryenBackendMCCodeGenInfo);
}
