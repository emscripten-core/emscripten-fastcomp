//===-- MipsNaClRewritePass.h - NaCl Sandboxing Pass    ---------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
//===----------------------------------------------------------------------===//

#ifndef TARGET_MIPSNACLREWRITEPASS_H
#define TARGET_MIPSNACLREWRITEPASS_H

#include "llvm/Target/TargetRegisterInfo.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/Support/CommandLine.h"

namespace llvm {
  extern cl::opt<bool> FlagSfiLoad;
  extern cl::opt<bool> FlagSfiStore;
  extern cl::opt<bool> FlagSfiStack;
  extern cl::opt<bool> FlagSfiBranch;
}

#endif
