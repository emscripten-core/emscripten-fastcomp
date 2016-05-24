//===-- BinaryenTargetMachine.h - TargetMachine for the Binaryen Backend ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===---------------------------------------------------------------------===//
//
// This file declares the TargetMachine that is used by the Binaryen/wasm/
// emscripten backend.
//
//===---------------------------------------------------------------------===//

#ifndef BINARYENTARGETMACHINE_H
#define BINARYENTARGETMACHINE_H

#include "Binaryen.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetSubtargetInfo.h"
#include "llvm/Target/TargetLowering.h"

namespace llvm {

class formatted_raw_ostream;

class BinaryenTargetLowering : public TargetLowering {
public:
  explicit BinaryenTargetLowering(const TargetMachine& TM) : TargetLowering(TM) {}
};

class BinaryenSubtarget : public TargetSubtargetInfo {
  BinaryenTargetLowering TL;

public:
  BinaryenSubtarget(const TargetMachine& TM, const Triple &TT);

  const TargetLowering *getTargetLowering() const override {
    return &TL;
  }
};

class BinaryenTargetMachine : public TargetMachine {
  const BinaryenSubtarget ST;

public:
  BinaryenTargetMachine(const Target &T, const Triple &TT,
                  StringRef CPU, StringRef FS, const TargetOptions &Options,
                  Reloc::Model RM, CodeModel::Model CM,
                  CodeGenOpt::Level OL);

  bool addPassesToEmitFile(
      PassManagerBase &PM, raw_pwrite_stream &Out, CodeGenFileType FileType,
      bool DisableVerify = true, AnalysisID StartBefore = nullptr,
      AnalysisID StartAfter = nullptr, AnalysisID StopAfter = nullptr,
      MachineFunctionInitializer *MFInitializer = nullptr) override;

  TargetIRAnalysis getTargetIRAnalysis() override;

  const TargetSubtargetInfo *getBinaryenSubtargetImpl() const {
    return &ST;
  }

  const BinaryenSubtarget *getSubtargetImpl(const Function &F) const override {
    return &ST;
  }
};

} // End llvm namespace

#endif
