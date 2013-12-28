//===-- JSTargetMachine.h - TargetMachine for the C++ backend --*- C++ -*-===//
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

#include "llvm/IR/DataLayout.h"
#include "llvm/Target/TargetMachine.h"

namespace llvm {

class formatted_raw_ostream;

struct JSTargetMachine : public TargetMachine {
  JSTargetMachine(const Target &T, StringRef TT,
                  StringRef CPU, StringRef FS, const TargetOptions &Options,
                  Reloc::Model RM, CodeModel::Model CM,
                  CodeGenOpt::Level OL)
    : TargetMachine(T, TT, CPU, FS, Options) {}

  virtual bool addPassesToEmitFile(PassManagerBase &PM,
                                   formatted_raw_ostream &Out,
                                   CodeGenFileType FileType,
                                   bool DisableVerify,
                                   AnalysisID StartAfter,
                                   AnalysisID StopAfter);

  virtual const DataLayout *getDataLayout() const { return 0; }
};

extern Target TheJSBackendTarget;

} // End llvm namespace


#endif
