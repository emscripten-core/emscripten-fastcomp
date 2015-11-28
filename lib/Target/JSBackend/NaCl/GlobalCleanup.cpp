//===- GlobalCleanup.cpp - Cleanup global symbols post-bitcode-link -------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
// ===---------------------------------------------------------------------===//
//
// PNaCl executables should have no external symbols or aliases. These passes
// internalize (or otherwise remove/resolve) GlobalValues and resolve all
// GlobalAliases.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Triple.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

namespace {
class GlobalCleanup : public ModulePass {
public:
  static char ID;
  GlobalCleanup() : ModulePass(ID) {
    initializeGlobalCleanupPass(*PassRegistry::getPassRegistry());
  }
  bool runOnModule(Module &M) override;
};

class ResolveAliases : public ModulePass {
public:
  static char ID;
  ResolveAliases() : ModulePass(ID) {
    initializeResolveAliasesPass(*PassRegistry::getPassRegistry());
  }
  bool runOnModule(Module &M) override;
};
}

char GlobalCleanup::ID = 0;
INITIALIZE_PASS(GlobalCleanup, "nacl-global-cleanup",
                "GlobalValue cleanup for PNaCl "
                "(assumes all of the binary is linked statically)",
                false, false)

static bool CleanUpLinkage(GlobalValue *GV) {
  // TODO(dschuff): handle the rest of the linkage types as necessary without
  // running afoul of the IR verifier or breaking the native link
  switch (GV->getLinkage()) {
  case GlobalValue::ExternalWeakLinkage: {
    auto *NullRef = Constant::getNullValue(GV->getType());
    GV->replaceAllUsesWith(NullRef);
    GV->eraseFromParent();
    return true;
  }
  case GlobalValue::WeakAnyLinkage: {
    GV->setLinkage(GlobalValue::InternalLinkage);
    return true;
  }
  default:
    // default with fall through to avoid compiler warning
    return false;
  }
  return false;
}

bool GlobalCleanup::runOnModule(Module &M) {
  bool Modified = false;

  // Cleanup llvm.compiler.used. We leave llvm.used as-is,
  // because optimization passes feed off it to understand
  // what globals may/may not be optimized away. For PNaCl,
  // it is removed before ABI validation by CleanupUsedGlobalsMetadata.
  if (auto *GV = M.getNamedGlobal("llvm.compiler.used")) {
    GV->eraseFromParent();
    Modified = true;
  }

  for (auto I = M.global_begin(), E = M.global_end(); I != E;) {
    GlobalVariable *GV = &*I++;
    Modified |= CleanUpLinkage(GV);
  }

  for (auto I = M.begin(), E = M.end(); I != E;) {
    Function *F = &*I++;
    Modified |= CleanUpLinkage(F);
  }

  return Modified;
}

ModulePass *llvm::createGlobalCleanupPass() { return new GlobalCleanup(); }

char ResolveAliases::ID = 0;
INITIALIZE_PASS(ResolveAliases, "resolve-aliases",
                "resolve global variable and function aliases", false, false)

bool ResolveAliases::runOnModule(Module &M) {
  bool Modified = false;

  for (auto I = M.alias_begin(), E = M.alias_end(); I != E;) {
    GlobalAlias *Alias = &*I++;
    Alias->replaceAllUsesWith(Alias->getAliasee());
    Alias->eraseFromParent();
    Modified = true;
  }
  return Modified;
}

ModulePass *llvm::createResolveAliasesPass() { return new ResolveAliases(); }
