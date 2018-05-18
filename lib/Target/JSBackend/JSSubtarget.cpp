//===-- JSSubtarget.cpp - JS Subtarget Information ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief This file implements the JS-specific subclass of
/// TargetSubtarget.
///
//===----------------------------------------------------------------------===//

#include "JSSubtarget.h"
#include "llvm/Support/TargetRegistry.h"
using namespace llvm;

JSSubtarget &
JSSubtarget::initializeSubtargetDependencies(StringRef FS) {
  // Determine default and user-specified characteristics

  if (CPUString.empty())
    CPUString = "generic";

  return *this;
}

extern const llvm::SubtargetFeatureKV JSSubTypeKV[] = {
  { "asmjs", "Select the asmjs processor", { }, { } }
};
 
static const llvm::SubtargetInfoKV JSProcSchedModels[] = {
  { "asmjs", &MCSchedModel::GetDefaultSchedModel() }
};

JSSubtarget::JSSubtarget(const Triple &TT,
                         const std::string &CPU,
                         const std::string &FS,
                         const TargetMachine &TM)
    : TargetSubtargetInfo(TT, "asmjs", "asmjs", None, makeArrayRef(JSSubTypeKV, 1), JSProcSchedModels, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr),
      HasSIMD128(false),
      HasAtomics(false), HasNontrappingFPToInt(false), CPUString(CPU),
      TargetTriple(TT),
      TLInfo(TM) {}

bool JSSubtarget::enableMachineScheduler() const {
  // Disable the MachineScheduler for now. Even with ShouldTrackPressure set and
  // enableMachineSchedDefaultSched overridden, it appears to have an overall
  // negative effect for the kinds of register optimizations we're doing.
  return false;
}

bool JSSubtarget::useAA() const { return true; }
