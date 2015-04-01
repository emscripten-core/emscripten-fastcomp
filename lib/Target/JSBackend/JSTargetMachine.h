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
#include "llvm/IR/DataLayout.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetSubtargetInfo.h"

namespace llvm {

class formatted_raw_ostream;

class JSSubtarget : public TargetSubtargetInfo {
  const DataLayout *DL;
public:
  JSSubtarget(const DataLayout *DL_) : DL(DL_) {}
  virtual const DataLayout *getDataLayout() const { return DL; }
};

class JSTargetMachine : public TargetMachine {
  const DataLayout DL;
  JSSubtarget Subtarget;

public:
  JSTargetMachine(const Target &T, StringRef Triple,
                  StringRef CPU, StringRef FS, const TargetOptions &Options,
                  Reloc::Model RM, CodeModel::Model CM,
                  CodeGenOpt::Level OL);

  virtual bool addPassesToEmitFile(PassManagerBase &PM,
                                   formatted_raw_ostream &Out,
                                   CodeGenFileType FileType,
                                   bool DisableVerify,
                                   AnalysisID StartAfter,
                                   AnalysisID StopAfter);

  virtual const DataLayout *getDataLayout() const { return &DL; }
  const JSSubtarget *getSubtargetImpl() const override { return &Subtarget; }

  /// \brief Register X86 analysis passes with a pass manager.
  virtual void addAnalysisPasses(PassManagerBase &PM);
};

} // End llvm namespace

#endif
