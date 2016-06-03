//===- NoExitRuntime.cpp - Expand i64 and wider integer types -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===------------------------------------------------------------------===//
//
//===------------------------------------------------------------------===//

#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Analysis/ConstantFolding.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/IR/CFG.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Transforms/NaCl.h"
#include "llvm/Transforms/Utils/Local.h"
#include <map>

#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
  class NoExitRuntime : public ModulePass {
    Module *TheModule;

  public:
    static char ID;
    NoExitRuntime() : ModulePass(ID) {
      initializeNoExitRuntimePass(*PassRegistry::getPassRegistry());
    }

    virtual bool runOnModule(Module &M);
  };
}

char NoExitRuntime::ID = 0;
INITIALIZE_PASS(NoExitRuntime, "emscripten-no-exit-runtime",
                "Generate code which assumes the runtime is never exited (so atexit etc. is unneeded; see emscripten NO_EXIT_RUNTIME setting)",
                false, false)


// Implementation of NoExitRuntime

bool NoExitRuntime::runOnModule(Module &M) {
  TheModule = &M;

  Function *AtExit = TheModule->getFunction("__cxa_atexit");
  if (!AtExit || !AtExit->isDeclaration() || AtExit->getNumUses() == 0) return false;

  // The system atexit is used - let's remove calls to it

  Type *i32 = Type::getInt32Ty(TheModule->getContext());
  Value *Zero  = Constant::getNullValue(i32);

  std::vector<Instruction*> ToErase;

  for (Instruction::user_iterator UI = AtExit->user_begin(), UE = AtExit->user_end(); UI != UE; ++UI) {
    if (CallInst *CI = dyn_cast<CallInst>(*UI)) {
      if (CI->getCalledValue() == AtExit) {
        // calls to atexit can just be removed
        CI->replaceAllUsesWith(Zero);
        ToErase.push_back(CI);
        continue;
      }
    }
    // Possibly other uses of atexit are done - ptrtoint, etc. - but we leave those alone
  }

  for (unsigned i = 0; i < ToErase.size(); i++) {
    ToErase[i]->eraseFromParent();
  }

  return true;
}

ModulePass *llvm::createNoExitRuntimePass() {
  return new NoExitRuntime();
}
