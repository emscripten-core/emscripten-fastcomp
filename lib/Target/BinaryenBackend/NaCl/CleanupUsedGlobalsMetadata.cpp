//===- CleanupUsedGlobalsMetadata.cpp - Cleanup llvm.used -----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
// ===---------------------------------------------------------------------===//
//
// Remove llvm.used metadata.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

namespace {
class CleanupUsedGlobalsMetadata : public ModulePass {
public:
  static char ID;
  CleanupUsedGlobalsMetadata() : ModulePass(ID) {
    initializeCleanupUsedGlobalsMetadataPass(*PassRegistry::getPassRegistry());
  }
  bool runOnModule(Module &M) override;
};
}

char CleanupUsedGlobalsMetadata::ID = 0;
INITIALIZE_PASS(CleanupUsedGlobalsMetadata, "cleanup-used-globals-metadata",
                "Removes llvm.used metadata.", false, false)

bool CleanupUsedGlobalsMetadata::runOnModule(Module &M) {
  bool Modified = false;

  if (auto *GV = M.getNamedGlobal("llvm.used")) {
    GV->eraseFromParent();
    Modified = true;
  }

  return Modified;
}

ModulePass *llvm::createCleanupUsedGlobalsMetadataPass() {
  return new CleanupUsedGlobalsMetadata();
}