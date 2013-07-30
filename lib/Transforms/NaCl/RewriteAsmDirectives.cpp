//===- RewriteAsmDirectives.cpp - Handle Architecture-Independent Assembly-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass rewrites any inline assembly directive which is portable
// into LLVM bitcode.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Twine.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/InstVisitor.h"
#include "llvm/Pass.h"
#include <string>

using namespace llvm;

namespace {
class RewriteAsmDirectives : public FunctionPass {
public:
  static char ID; // Pass identification, replacement for typeid
  RewriteAsmDirectives() : FunctionPass(ID) {
    initializeRewriteAsmDirectivesPass(*PassRegistry::getPassRegistry());
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
  AsmDirectivesVisitor &operator=(const AsmDirectivesVisitor &) LLVM_DELETED_FUNCTION;
};
}

char RewriteAsmDirectives::ID = 0;
INITIALIZE_PASS(
    RewriteAsmDirectives, "rewrite-asm-directives",
    "rewrite portable inline assembly directives into non-asm LLVM IR",
    false, false)

bool RewriteAsmDirectives::runOnFunction(Function &F) {
  AsmDirectivesVisitor AV(F);
  AV.visit(F);
  return AV.modifiedFunction();
}

void AsmDirectivesVisitor::visitCallInst(CallInst &CI) {
  if (!CI.isInlineAsm())
    return;

  Instruction *Replacement = NULL;

  InlineAsm *IA = cast<InlineAsm>(CI.getCalledValue());
  std::string AsmStr(IA->getAsmString());
  std::string ConstraintStr(IA->getConstraintString());
  Type *T = CI.getType();

  bool isEmptyAsm = AsmStr.empty();
  // Different triples will encode "touch everything" differently, e.g.:
  //  - le32-unknown-nacl has "~{memory}".
  //  - x86 "~{memory},~{dirflag},~{fpsr},~{flags}".
  // The following code therefore only searches for memory: this pass
  // deals with portable assembly, touching anything else than memory in
  // an empty assembly statement is meaningless.
  bool touchesMemory = ConstraintStr.find("~{memory}") != std::string::npos;

  if (T->isVoidTy() && IA->hasSideEffects() && isEmptyAsm && touchesMemory) {
    // asm("":::"memory") => fence seq_cst
    // This transformation is safe and strictly stronger: the former is
    // purely a compiler fence, whereas the latter is a compiler fence
    // as well as a hardware fence which orders all loads and stores on
    // the current thread of execution.
    Replacement = new FenceInst(C, SequentiallyConsistent, CrossThread, &CI);
  }

  if (Replacement) {
    Replacement->setDebugLoc(CI.getDebugLoc());
    CI.replaceAllUsesWith(Replacement);
    CI.eraseFromParent();
    ModifiedFunction = true;
  }
}

namespace llvm {
FunctionPass *createRewriteAsmDirectivesPass() {
  return new RewriteAsmDirectives();
}
}
