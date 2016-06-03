//===-- RemoveLLVMAssume.cpp ----------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===-----------------------------------------------------------------------===//
//
//===-----------------------------------------------------------------------===//

#include "OptPasses.h"

#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/Utils/Local.h"

namespace llvm {

// Remove all uses of llvm.assume; we don't need them anymore
struct RemoveLLVMAssume : public ModulePass {
  static char ID; // Pass identification, replacement for typeid
  RemoveLLVMAssume() : ModulePass(ID) {}
    // XXX initialize..(*PassRegistry::getPassRegistry()); }

  bool runOnModule(Module &M) override;

  const char *getPassName() const override { return "RemoveLLVMAssume"; }
};

char RemoveLLVMAssume::ID = 0;

bool RemoveLLVMAssume::runOnModule(Module &M) {
  bool Changed = false;

  Function *LLVMAssume = M.getFunction("llvm.assume");

  if (LLVMAssume) {
    SmallVector<CallInst*, 10> Assumes;
    for (Instruction::user_iterator UI = LLVMAssume->user_begin(), UE = LLVMAssume->user_end(); UI != UE; ++UI) {
      User *U = *UI;
      if (CallInst *CI = dyn_cast<CallInst>(U)) {
        Assumes.push_back(CI);
      }
    }

    for (auto CI : Assumes) {
      Value *V = CI->getOperand(0);
      CI->eraseFromParent();
      RecursivelyDeleteTriviallyDeadInstructions(V); // the single operand is likely dead
    }
  }
  return Changed;
}

//

extern ModulePass *createEmscriptenRemoveLLVMAssumePass() {
  return new RemoveLLVMAssume();
}

} // End llvm namespace
