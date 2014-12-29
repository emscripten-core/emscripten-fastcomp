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

  virtual bool runOnFunction(Function &F);
};

class AsmDirectivesVisitor : public InstVisitor<AsmDirectivesVisitor> {
public:
  AsmDirectivesVisitor(Function &F)
      : F(F), C(F.getParent()->getContext()), ModifiedFunction(false) {}
  ~AsmDirectivesVisitor() {}
  bool modifiedFunction() const { return ModifiedFunction; }

  /// Only Call Instructions are ever inline assembly directives.
  void visitCallInst(CallInst &CI);

private:
  Function &F;
  LLVMContext &C;
  bool ModifiedFunction;

  AsmDirectivesVisitor() LLVM_DELETED_FUNCTION;
  AsmDirectivesVisitor(const AsmDirectivesVisitor &) LLVM_DELETED_FUNCTION;
  AsmDirectivesVisitor &operator=(
      const AsmDirectivesVisitor &) LLVM_DELETED_FUNCTION;
};
}

char RemoveAsmMemory::ID = 0;
INITIALIZE_PASS(RemoveAsmMemory, "remove-asm-memory",
                "remove all instances of ``asm(\"\":::\"memory\")``", false,
                false)

bool RemoveAsmMemory::runOnFunction(Function &F) {
  AsmDirectivesVisitor AV(F);
  AV.visit(F);
  return AV.modifiedFunction();
}

void AsmDirectivesVisitor::visitCallInst(CallInst &CI) {
  if (!CI.isInlineAsm() ||
      !cast<InlineAsm>(CI.getCalledValue())->isAsmMemory())
    return;

  // In NaCl ``asm("":::"memory")`` always comes in pairs, straddling a
  // sequentially consistent fence. Other passes rewrite this fence to
  // an equivalent stable NaCl intrinsic, meaning that this assembly can
  // be removed.
  CI.eraseFromParent();
  ModifiedFunction = true;
}

namespace llvm {
FunctionPass *createRemoveAsmMemoryPass() { return new RemoveAsmMemory(); }
}
