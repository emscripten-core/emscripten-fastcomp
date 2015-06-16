//===-- JSTargetMachine.h - TargetMachine for the JS Backend ---*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===---------------------------------------------------------------------===//

#ifndef OPT_PASSES_H
#define OPT_PASSES_H

#include "llvm/Pass.h"

namespace llvm {

  extern FunctionPass *createEmscriptenSimplifyAllocasPass();
  extern ModulePass *createEmscriptenRemoveLLVMAssumePass();
  extern FunctionPass *createEmscriptenExpandBigSwitchesPass();

} // End llvm namespace

#endif

