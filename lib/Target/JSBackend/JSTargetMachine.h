// JSTargetMachine.h - Define TargetMachine for JS -*- C++ -*-
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief This file declares the JS-specific subclass of
/// TargetMachine.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_JS_JSTARGETMACHINE_H
#define LLVM_LIB_TARGET_JS_JSTARGETMACHINE_H

#include "JSSubtarget.h"
#include "llvm/Target/TargetMachine.h"

namespace llvm {

class JSTargetMachine final : public LLVMTargetMachine {
  mutable StringMap<std::unique_ptr<JSSubtarget>> SubtargetMap;

public:
  JSTargetMachine(const Target &T, const Triple &TT, StringRef CPU,
                           StringRef FS, const TargetOptions &Options,
                           Optional<Reloc::Model> RM,
                           Optional<CodeModel::Model> CM, CodeGenOpt::Level OL,
                           bool JIT);

  ~JSTargetMachine() override;
  const JSSubtarget *
  getSubtargetImpl(const Function &F) const override;

  TargetTransformInfo getTargetTransformInfo(const Function &F) override;

  bool usesPhysRegsForPEI() const override { return false; }

  bool addPassesToEmitFile(PassManagerBase &PM, raw_pwrite_stream &Out,
                           CodeGenFileType FileType, bool DisableVerify = true,
                           MachineModuleInfo *MMI = nullptr) override;
};

} // end namespace llvm

#endif
