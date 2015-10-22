//===- RemoveAsmMemory.cpp - Remove ``asm("":::"memory")`` ----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass removes all instances of ``asm("":::"memory")``.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Twine.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/Pass.h"
#include <string>

using namespace llvm;

namespace {
class RemoveAsmMemory : public FunctionPass {
public:
  static char ID; // Pass identification, replacement for typeid
  RemoveAsmMemory() : FunctionPass(ID) {
    initializeRemoveAsmMemoryPass(*PassRegistry::getPassRegistry());
  }

  bool runOnFunction(Function &F) override;
};

class AsmDirectivesVisitor : public InstVisitor<AsmDirectivesVisitor> {
public:
  AsmDirectivesVisitor() : ModifiedFunction(false) {}
  ~AsmDirectivesVisitor() {}
  bool modifiedFunction() const { return ModifiedFunction; }

  /// Only Call Instructions are ever inline assembly directives.
  void visitCallInst(CallInst &CI);

private:
  bool ModifiedFunction;

  AsmDirectivesVisitor(const AsmDirectivesVisitor &) = delete;
  AsmDirectivesVisitor &operator=(const AsmDirectivesVisitor &) = delete;
};
}

char RemoveAsmMemory::ID = 0;
INITIALIZE_PASS(RemoveAsmMemory, "remove-asm-memory",
                "remove all instances of ``asm(\"\":::\"memory\")``", false,
                false)

bool RemoveAsmMemory::runOnFunction(Function &F) {
  AsmDirectivesVisitor AV;
  AV.visit(F);
  return AV.modifiedFunction();
}

void AsmDirectivesVisitor::visitCallInst(CallInst &CI) {
  llvm_unreachable("no longer maintained");
}

namespace llvm {
FunctionPass *createRemoveAsmMemoryPass() { return new RemoveAsmMemory(); }
}
