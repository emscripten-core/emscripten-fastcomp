//===- StripTls.cpp - Remove the thread_local attribute from variables ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Runtime support for thread-local storage depends on pthreads which are
// currently not supported by MinSFI. This pass removes the thread_local
// attribute from all global variables until thread support is in place.
//
// The pass should be invoked before the pnacl-abi-simplify passes.
//
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Module.h"

using namespace llvm;

namespace {
class StripTls : public ModulePass {
 public:
  static char ID;
  StripTls() : ModulePass(ID) {
    initializeStripTlsPass(*PassRegistry::getPassRegistry());
  }

  virtual bool runOnModule(Module &M);
};
}  // namespace

bool StripTls::runOnModule(Module &M) {
  bool Changed = false;
  for (Module::global_iterator GV = M.global_begin(), E = M.global_end();
       GV != E; ++GV) {
    if (GV->isThreadLocal()) {
      GV->setThreadLocal(false);
      Changed = true;
    }
  }
  return Changed;
}

char StripTls::ID = 0;
INITIALIZE_PASS(StripTls, "minsfi-strip-tls",
                "Remove the thread_local attribute from variables",
                false, false)
