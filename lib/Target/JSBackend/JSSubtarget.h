//=- JSSubtarget.h - Define Subtarget for the JS -*- C++ -*-//
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
/// TargetSubtarget.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_JS_JSSUBTARGET_H
#define LLVM_LIB_TARGET_JS_JSSUBTARGET_H

#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include <string>

namespace llvm {

class JSTargetLowering : public TargetLowering {
public:
  explicit JSTargetLowering(const TargetMachine& TM) : TargetLowering(TM) {}
};

class JSSubtarget final : public TargetSubtargetInfo {
  bool HasSIMD128;
  bool HasAtomics;
  bool HasNontrappingFPToInt;

  /// String name of used CPU.
  std::string CPUString;

  /// What processor and OS we're targeting.
  Triple TargetTriple;

  JSTargetLowering TLInfo;

  /// Initializes using CPUString and the passed in feature string so that we
  /// can use initializer lists for subtarget initialization.
  JSSubtarget &initializeSubtargetDependencies(StringRef FS);

public:
  /// This constructor initializes the data members to match that
  /// of the specified triple.
  JSSubtarget(const Triple &TT, const std::string &CPU,
                       const std::string &FS, const TargetMachine &TM);

  const JSTargetLowering *getTargetLowering() const override {
    return &TLInfo;
  }
  const Triple &getTargetTriple() const { return TargetTriple; }
  bool enableMachineScheduler() const override;
  bool useAA() const override;

  // Predicates used by JSInstrInfo.td.
  bool hasAddr64() const { return TargetTriple.isArch64Bit(); }
  bool hasSIMD128() const { return HasSIMD128; }
  bool hasAtomics() const { return HasAtomics; }
  bool hasNontrappingFPToInt() const { return HasNontrappingFPToInt; }
};

} // end namespace llvm

#endif
