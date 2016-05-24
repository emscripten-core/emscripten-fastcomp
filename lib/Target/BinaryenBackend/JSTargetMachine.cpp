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

extern const llvm::SubtargetFeatureKV JSSubTypeKV[] = {
  { "asmjs", "Select the asmjs processor", { }, { } }
};

static const llvm::SubtargetInfoKV JSProcSchedModels[] = {
  { "asmjs", &MCSchedModel::GetDefaultSchedModel() }
};

JSSubtarget::JSSubtarget(const TargetMachine& TM, const Triple &TT) :
  TargetSubtargetInfo(TT, "asmjs", "asmjs", None, makeArrayRef(JSSubTypeKV, 1), JSProcSchedModels, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr),
  TL(TM)
 {}


JSTargetMachine::JSTargetMachine(const Target &T, const Triple &TT,
                                 StringRef CPU, StringRef FS, const TargetOptions &Options,
                                 Reloc::Model RM, CodeModel::Model CM,
                                 CodeGenOpt::Level OL)
    : TargetMachine(T, "e-p:32:32-i64:64-v128:32:128-n32-S128", TT, CPU,
                    FS, Options),
      ST(*this, TT) {
  CodeGenInfo = T.createMCCodeGenInfo("asmjs", RM, CM, OL);
}

TargetIRAnalysis JSTargetMachine::getTargetIRAnalysis() {
  return TargetIRAnalysis([this](const Function &F) {
    return TargetTransformInfo(JSTTIImpl(this, F));
  });
}

