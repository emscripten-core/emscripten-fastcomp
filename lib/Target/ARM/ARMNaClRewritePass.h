//===-- ARMNaClRewritePass.h - NaCl Sandboxing Pass    ------- --*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef TARGET_ARMNACLREWRITEPASS_H
#define TARGET_ARMNACLREWRITEPASS_H

#include "llvm/Target/TargetRegisterInfo.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/Support/CommandLine.h"

namespace llvm {
  extern cl::opt<bool> FlagSfiZeroMask;
  extern cl::opt<bool> FlagSfiData;
  extern cl::opt<bool> FlagSfiLoad;
  extern cl::opt<bool> FlagSfiStore;
  extern cl::opt<bool> FlagSfiStack;
  extern cl::opt<bool> FlagSfiBranch;
}

namespace ARM_SFI {

bool IsStackChange(const llvm::MachineInstr &MI,
                   const llvm::TargetRegisterInfo *TRI);
bool IsSandboxedStackChange(const llvm::MachineInstr &MI);
bool NeedSandboxStackChange(const llvm::MachineInstr &MI,
                               const llvm::TargetRegisterInfo *TRI);

} // namespace ARM_SFI

#endif
