//===- ExpandConstantExpr.cpp - Convert ConstantExprs to Instructions------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass expands out ConstantExprs into Instructions.
//
// Note that this only converts ConstantExprs that are referenced by
// Instructions.  It does not convert ConstantExprs that are used as
// initializers for global variables.
//
// This simplifies the language so that the PNaCl translator does not
// need to handle ConstantExprs as part of a stable wire format for
// PNaCl.
//
//===----------------------------------------------------------------------===//

#include <map>

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

static bool expandInstruction(Instruction *Inst);

namespace {
  // This is a FunctionPass because our handling of PHI nodes means
  // that our modifications may cross BasicBlocks.
  struct ExpandConstantExpr : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid
    ExpandConstantExpr() : FunctionPass(ID) {
      initializeExpandConstantExprPass(*PassRegistry::getPassRegistry());
    }

    virtual bool runOnFunction(Function &Func);
  };
}

char ExpandConstantExpr::ID = 0;
INITIALIZE_PASS(ExpandConstantExpr, "expand-constant-expr",
                "Expand out ConstantExprs into Instructions",
                false, false)

static Value *expandConstantExpr(Instruction *InsertPt, ConstantExpr *Expr) {
  Instruction *NewInst = Expr->getAsInstruction();
  NewInst->insertBefore(InsertPt);
  NewInst->setName("expanded");
  expandInstruction(NewInst);
  return NewInst;
}

static bool expandInstruction(Instruction *Inst) {
  // A landingpad can only accept ConstantExprs, so it should remain
  // unmodified.
  if (isa<LandingPadInst>(Inst))
    return false;

  bool Modified = false;
  for (unsigned OpNum = 0; OpNum < Inst->getNumOperands(); OpNum++) {
    if (ConstantExpr *Expr =
        dyn_cast<ConstantExpr>(Inst->getOperand(OpNum))) {
      Modified = true;
      Use *U = &Inst->getOperandUse(OpNum);
      PhiSafeReplaceUses(U, expandConstantExpr(PhiSafeInsertPt(U), Expr));
    }
  }
  return Modified;
}

bool ExpandConstantExpr::runOnFunction(Function &Func) {
  bool Modified = false;
  for (llvm::Function::iterator BB = Func.begin(), E = Func.end();
       BB != E;
       ++BB) {
    for (BasicBlock::InstListType::iterator Inst = BB->begin(), E = BB->end();
         Inst != E;
         ++Inst) {
      Modified |= expandInstruction(Inst);
    }
  }
  return Modified;
}

FunctionPass *llvm::createExpandConstantExprPass() {
  return new ExpandConstantExpr();
}
