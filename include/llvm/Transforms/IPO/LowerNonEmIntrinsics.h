//===- LowerNonEmIntrinsics - Lower non-emscripten stuff        -----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass is a simple pass wrapper around the PromoteMemToReg function call
// exposed by the Utils library.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_UTILS_LOWERNONEMINTRINSICS_H
#define LLVM_TRANSFORMS_UTILS_LOWERNONEMINTRINSICS_H

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"

namespace llvm {

class Module;

class LowerNonEmIntrinsicsPass : public PassInfoMixin<LowerNonEmIntrinsicsPass> {
public:
  LowerNonEmIntrinsicsPass();

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

} // end namespace llvm

#endif // LLVM_TRANSFORMS_UTILS_LOWERNONEMINTRINSICS_H
