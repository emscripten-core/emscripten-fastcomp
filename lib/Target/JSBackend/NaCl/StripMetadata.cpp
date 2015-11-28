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

#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

namespace {
  class StripMetadata : public ModulePass {
  public:
    static char ID;
    StripMetadata() : ModulePass(ID), ShouldStripModuleFlags(false) {
      initializeStripMetadataPass(*PassRegistry::getPassRegistry());
    }

    virtual bool runOnModule(Module &M);

    virtual void getAnalysisUsage(AnalysisUsage &AU) const {
      AU.setPreservesCFG();
    }

  protected:
    bool ShouldStripModuleFlags;
  };

  class StripModuleFlags : public StripMetadata {
  public:
    static char ID;
    StripModuleFlags() : StripMetadata() {
      initializeStripModuleFlagsPass(*PassRegistry::getPassRegistry());
      ShouldStripModuleFlags = true;
    }
  };

// In certain cases, linked bitcode files can have DISupbrogram metadata which
// points to a Function that has no dbg attachments. This causes problem later
// (e.g. in inlining). See https://llvm.org/bugs/show_bug.cgi?id=23874
// Until that bug is fixed upstream (the fix will involve infrastructure that we
// don't have in our branch yet) we have to ensure we don't expose this case
// to further optimizations. So we'd like to strip out such debug info.
// Unfortunately once created the metadata is not easily deleted or even
// modified; the best we can easily do is to set the Function object it points
// to to null. Fortunately this is legitimate (declarations have no Function
// either) and should be workable until the fix lands.
class StripDanglingDISubprograms : public ModulePass {
 public:
  static char ID;
  StripDanglingDISubprograms() : ModulePass(ID) {
    initializeStripDanglingDISubprogramsPass(*PassRegistry::getPassRegistry());
  }
  bool runOnModule(Module &M) override;
};
}

char StripMetadata::ID = 0;
INITIALIZE_PASS(StripMetadata, "strip-metadata",
                "Strip all non-stable non-debug metadata from a module.",
                false, false)

char StripModuleFlags::ID = 0;
INITIALIZE_PASS(StripModuleFlags, "strip-module-flags",
                "Strip all non-stable non-debug metadata from a module, "
                "including the llvm.module.flags metadata.",
                false, false)

char StripDanglingDISubprograms::ID = 0;
INITIALIZE_PASS(StripDanglingDISubprograms, "strip-dangling-disubprograms",
                "Strip DISubprogram metadata for functions with no debug info",
                false, false)

ModulePass *llvm::createStripMetadataPass() {
  return new StripMetadata();
}

ModulePass *llvm::createStripModuleFlagsPass() {
  return new StripModuleFlags();
}

ModulePass *llvm::createStripDanglingDISubprogramsPass() {
  return new StripDanglingDISubprograms();
}

static bool IsWhitelistedMetadata(const NamedMDNode *node,
                                  bool StripModuleFlags) {
  // Leave debug metadata to the -strip-debug pass.
  return (node->getName().startswith("llvm.dbg.") ||
          // "Debug Info Version" is in llvm.module.flags.
          (!StripModuleFlags && node->getName().equals("llvm.module.flags")));
}

static bool DoStripMetadata(Module &M, bool StripModuleFlags) {
  bool Changed = false;

  if (!StripModuleFlags)
    for (Function &F : M)
      for (BasicBlock &B : F)
        for (Instruction &I : B) {
          SmallVector<std::pair<unsigned, MDNode *>, 8> InstMeta;
          // Let the debug metadata be stripped by the -strip-debug pass.
          I.getAllMetadataOtherThanDebugLoc(InstMeta);
          for (size_t i = 0; i < InstMeta.size(); ++i) {
            I.setMetadata(InstMeta[i].first, NULL);
            Changed = true;
          }
        }

  // Strip unsupported named metadata.
  SmallVector<NamedMDNode*, 8> ToErase;
  for (Module::NamedMDListType::iterator I = M.named_metadata_begin(),
           E = M.named_metadata_end(); I != E; ++I) {
    if (!IsWhitelistedMetadata(&*I, StripModuleFlags))
      ToErase.push_back(&*I);
  }
  for (size_t i = 0; i < ToErase.size(); ++i)
    M.eraseNamedMetadata(ToErase[i]);

  return Changed;
}

bool StripMetadata::runOnModule(Module &M) {
  return DoStripMetadata(M, ShouldStripModuleFlags);
}

static bool functionHasDbgAttachment(const Function &F) {
  for (const BasicBlock &BB : F) {
    for (const Instruction &I : BB) {
      if (I.getDebugLoc()) {
        return true;
      }
    }
  }
  return false;
}

bool StripDanglingDISubprograms::runOnModule(Module &M) {
  NamedMDNode *CU_Nodes = M.getNamedMetadata("llvm.dbg.cu");
  if (!CU_Nodes)
    return false;

  return false; // TODO: we don't need this anymore
}
