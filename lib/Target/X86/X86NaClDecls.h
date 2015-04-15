//===-- X86NaClDecls.h - Common X86 NaCl declarations -----------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file provides various NaCl-related declarations for the X86-32
// and X86-64 architectures.
//
//===----------------------------------------------------------------------===//

#ifndef X86NACLDECLS_H
#define X86NACLDECLS_H

#include "llvm/Support/CommandLine.h"

using namespace llvm;

extern const int kNaClX86InstructionBundleSize;

extern cl::opt<bool> FlagRestrictR15;
extern cl::opt<bool> FlagUseZeroBasedSandbox;
extern cl::opt<bool> FlagHideSandboxBase;

#endif    // X86NACLDECLS_H
