//===-- JSTargetMachine.cpp - Define TargetMachine for the JS -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines the JS specific subclass of TargetMachine.
//
//===----------------------------------------------------------------------===//

#include "JSTargetMachine.h"
#include "JSTargetTransformInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Support/TargetRegistry.h"
using namespace llvm;

JSTargetMachine::JSTargetMachine(const Target &T, StringRef Triple,
                                 StringRef CPU, StringRef FS, const TargetOptions &Options,
                                 Reloc::Model RM, CodeModel::Model CM,
                                 CodeGenOpt::Level OL)
    : TargetMachine(T, "e-p:32:32-i64:64-v128:32:128-n32-S128", Triple, CPU,
                    FS, Options),
      ST(*this) {
  CodeGenInfo = T.createMCCodeGenInfo(Triple, RM, CM, OL);
}


TargetIRAnalysis JSTargetMachine::getTargetIRAnalysis() {
  return TargetIRAnalysis([this](Function &F) {
    return TargetTransformInfo(JSTTIImpl(this));
  });
}

