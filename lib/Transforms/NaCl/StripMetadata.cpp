//===- StripMetadata.cpp - Strip non-stable non-debug metadata       ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// The StripMetadata transformation strips instruction attachment
// metadata, such as !tbaa and !prof metadata.
// TODO: Strip NamedMetadata too.
//
// It does not strip debug metadata.  Debug metadata is used by debug
// intrinsic functions and calls to those intrinsic functions.  Use the
// -strip-debug or -strip pass to strip that instead.
//
// The goal of this pass is to reduce bitcode ABI surface area.
// We don't know yet which kind of metadata is considered stable.
//===----------------------------------------------------------------------===//

#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

namespace {
  class StripMetadata : public ModulePass {
  public:
    static char ID;
    explicit StripMetadata() : ModulePass(ID) {
      initializeStripMetadataPass(*PassRegistry::getPassRegistry());
    }

    virtual bool runOnModule(Module &M);

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.setPreservesCFG();
    }
  };
}

char StripMetadata::ID = 0;
INITIALIZE_PASS(StripMetadata, "strip-metadata",
                "Strip all non-stable non-debug metadata from a module.",
                false, false)

ModulePass *llvm::createStripMetadataPass() {
  return new StripMetadata();
}

static bool IsWhitelistedMetadata(const NamedMDNode *node) {
  // Leave debug metadata to the -strip-debug pass.
  return node->getName().startswith("llvm.dbg.");
}

static bool DoStripMetadata(Module &M) {
  bool Changed = false;

  for (Module::iterator MI = M.begin(), ME = M.end(); MI != ME; ++MI) {
    for (Function::iterator FI = MI->begin(), FE = MI->end(); FI != FE; ++FI) {
      for (BasicBlock::iterator BI = FI->begin(), BE = FI->end(); BI != BE;
           ++BI) {
        SmallVector<std::pair<unsigned, MDNode *>, 8> InstMeta;
        // Let the debug metadata be stripped by the -strip-debug pass.
        BI->getAllMetadataOtherThanDebugLoc(InstMeta);
        for (size_t i = 0; i < InstMeta.size(); ++i) {
          BI->setMetadata(InstMeta[i].first, NULL);
          Changed = true;
        }
      }
    }
  }

  // Strip unsupported named metadata.
  SmallVector<NamedMDNode*, 8> ToErase;
  for (Module::NamedMDListType::iterator I = M.named_metadata_begin(),
           E = M.named_metadata_end(); I != E; ++I) {
    if (!IsWhitelistedMetadata(I))
      ToErase.push_back(I);
  }
  for (size_t i = 0; i < ToErase.size(); ++i)
    M.eraseNamedMetadata(ToErase[i]);

  return Changed;
}

bool StripMetadata::runOnModule(Module &M) {
  return DoStripMetadata(M);
}
