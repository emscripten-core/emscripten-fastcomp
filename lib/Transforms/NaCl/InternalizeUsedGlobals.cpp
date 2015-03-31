//===- InternalizeUsedGlobals.cpp - mark used globals as internal      ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// The internalize pass does not mark internal globals marked as "used",
// which may be achieved with __attribute((used))__ in C++, for example.
// In PNaCl scenarios, we always perform whole program analysis, and
// the ABI requires all but entrypoint globals to be internal. This pass
// satisfies such requirements.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/PassRegistry.h"
#include "llvm/PassSupport.h"
#include "llvm/Transforms/NaCl.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"

using namespace llvm;

namespace {

class InternalizeUsedGlobals : public ModulePass {
public:
  static char ID;

  InternalizeUsedGlobals() : ModulePass(ID) {
    initializeInternalizeUsedGlobalsPass(*PassRegistry::getPassRegistry());
  }
  virtual bool runOnModule(Module &M);
};
}

char InternalizeUsedGlobals::ID = 0;

INITIALIZE_PASS(InternalizeUsedGlobals, "internalize-used-globals",
                "Mark internal globals in the llvm.used list", false, false)

bool InternalizeUsedGlobals::runOnModule(Module &M) {
  bool Changed = false;

  SmallPtrSet<GlobalValue *, 8> Used;
  collectUsedGlobalVariables(M, Used, /*CompilerUsed =*/false);
  for (GlobalValue *V : Used) {
    if (V->getLinkage() != GlobalValue::InternalLinkage) {
      // Setting Linkage to InternalLinkage also sets the visibility to
      // DefaultVisibility.
      // For explicitness, we do so upfront.
      V->setVisibility(GlobalValue::DefaultVisibility);
      V->setLinkage(GlobalValue::InternalLinkage);
      Changed = true;
    }
  }
  return Changed;
}

ModulePass *llvm::createInternalizeUsedGlobalsPass() {
  return new InternalizeUsedGlobals();
}
