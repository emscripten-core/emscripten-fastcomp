//===-- JSTargetMachine.h - TargetMachine for the JS Backend ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===---------------------------------------------------------------------===//
//
// This file declares the TargetMachine that is used by the JS/asm.js/
// emscripten backend.
//
//===---------------------------------------------------------------------===//

#ifndef JSTARGETMACHINE_H
#define JSTARGETMACHINE_H

#include "JS.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/CodeGen/TargetLowering.h"

namespace llvm {

class formatted_raw_ostream;

class JSTargetLowering : public TargetLowering {
public:
  explicit JSTargetLowering(const TargetMachine& TM) : TargetLowering(TM) {}
};

class JSSubtarget : public TargetSubtargetInfo {
  JSTargetLowering TL;

public:
  JSSubtarget(const TargetMachine& TM, const Triple &TT);

  const TargetLowering *getTargetLowering() const override {
    return &TL;
  }
};

class JSTargetMachine : public LLVMTargetMachine {
  const JSSubtarget ST;

public:
  JSTargetMachine(const Target &T, const Triple &TT,
                  StringRef CPU, StringRef FS, const TargetOptions &Options,
                  Optional<Reloc::Model>& RM, Optional<CodeModel::Model> CM,
                  CodeGenOpt::Level OL, bool JIT);

  bool addPassesToEmitFile(PassManagerBase &PM, raw_pwrite_stream &Out,
                           CodeGenFileType FileType, bool DisableVerify = true,
                           MachineModuleInfo *MMI = nullptr) override;

  TargetTransformInfo getTargetTransformInfo(const Function &F) override;

  TargetIRAnalysis getTargetIRAnalysis(); // TODO: remove?

  const TargetSubtargetInfo *getJSSubtargetImpl() const {
    return &ST;
  }

  const JSSubtarget *getSubtargetImpl(const Function &F) const override {
    return &ST;
  }
};

} // End llvm namespace

#endif
