//===- JSTargetMachine.cpp - Define TargetMachine for JS -==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief This file defines the JS-specific subclass of TargetMachine.
///
//===----------------------------------------------------------------------===//

#include "JSTargetMachine.h"
#include "JSTargetTransformInfo.h"
#include "JS.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/Passes.h"
#include "llvm/CodeGen/RegAllocRegistry.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Transforms/Scalar.h"
using namespace llvm;

//===----------------------------------------------------------------------===//
// JS Lowering public interface.
//===----------------------------------------------------------------------===//

static Reloc::Model getEffectiveRelocModel(Optional<Reloc::Model> RM) {
  if (!RM.hasValue())
    return Reloc::PIC_;
  return *RM;
}

/// Create an JS architecture model.
///
JSTargetMachine::JSTargetMachine(
    const Target &T, const Triple &TT, StringRef CPU, StringRef FS,
    const TargetOptions &Options, Optional<Reloc::Model> RM,
    Optional<CodeModel::Model> CM, CodeGenOpt::Level OL, bool JIT)
    : LLVMTargetMachine(T,
                        "e-p:32:32-i64:64-v128:32:128-n32-S128",
                        // WASM: "e-m:e-p:32:32-i64:64-n32:64-S128",
                        TT, CPU, FS, Options, getEffectiveRelocModel(RM),
                        CM ? *CM : CodeModel::Large, OL) {
  // JS type-checks instructions, but a noreturn function with a return
  // type that doesn't match the context will cause a check failure. So we lower
  // LLVM 'unreachable' to ISD::TRAP and then lower that to JS's
  // 'unreachable' instructions which is meant for that case.
  this->Options.TrapUnreachable = true;

  // JS treats each function as an independent unit. Force
  // -ffunction-sections, effectively, so that we can emit them independently.
  if (!TT.isOSBinFormatELF()) {
    this->Options.FunctionSections = true;
    this->Options.DataSections = true;
    this->Options.UniqueSectionNames = true;
  }

  // XXX EMSCRIPTEN initAsmInfo();

  // Note that we don't use setRequiresStructuredCFG(true). It disables
  // optimizations than we're ok with, and want, such as critical edge
  // splitting and tail merging.
}

JSTargetMachine::~JSTargetMachine() {}

const JSSubtarget *
JSTargetMachine::getSubtargetImpl(const Function &F) const {
  Attribute CPUAttr = F.getFnAttribute("target-cpu");
  Attribute FSAttr = F.getFnAttribute("target-features");

  std::string CPU = !CPUAttr.hasAttribute(Attribute::None)
                        ? CPUAttr.getValueAsString().str()
                        : TargetCPU;
  std::string FS = !FSAttr.hasAttribute(Attribute::None)
                       ? FSAttr.getValueAsString().str()
                       : TargetFS;

  auto &I = SubtargetMap[CPU + FS];
  if (!I) {
    // This needs to be done before we create a new subtarget since any
    // creation will depend on the TM and the code generation flags on the
    // function that reside in TargetOptions.
    resetTargetOptions(F);
    I = llvm::make_unique<JSSubtarget>(TargetTriple, CPU, FS, *this);
  }
  return I.get();
}

namespace {
/// JS Code Generator Pass Configuration Options.
class JSPassConfig final : public TargetPassConfig {
public:
  JSPassConfig(JSTargetMachine &TM, PassManagerBase &PM)
      : TargetPassConfig(TM, PM) {}

  JSTargetMachine &getJSTargetMachine() const {
    return getTM<JSTargetMachine>();
  }

  FunctionPass *createTargetRegisterAllocator(bool) override;

  void addIRPasses() override;
  bool addInstSelector() override;
  void addPostRegAlloc() override;
  bool addGCPasses() override { return false; }
  void addPreEmitPass() override;
};
} // end anonymous namespace

TargetTransformInfo
JSTargetMachine::getTargetTransformInfo(const Function &F) {
  return TargetTransformInfo(JSTTIImpl(this, F));
}

