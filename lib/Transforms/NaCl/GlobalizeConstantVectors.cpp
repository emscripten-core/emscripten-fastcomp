//===- GlobalizeConstantVectors.cpp - Globalize constant vector -----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass replaces all constant vector operands by loads of the same
// vector value from a constant global. After this pass functions don't
// rely on ConstantVector and ConstantDataVector.
//
// The FlattenGlobals pass can be used to further simplify the globals
// that this pass creates.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/InstIterator.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

namespace {
// Must be a ModulePass since it adds globals.
class GlobalizeConstantVectors : public ModulePass {
public:
  static char ID; // Pass identification, replacement for typeid
  GlobalizeConstantVectors() : ModulePass(ID), DL(0) {
    initializeGlobalizeConstantVectorsPass(*PassRegistry::getPassRegistry());
  }
  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesCFG();
    AU.addRequired<DataLayout>();
  }
  virtual bool runOnModule(Module &M);

private:
  typedef SmallVector<Value *, 128> Values;
  const DataLayout *DL;

  void findConstantVectors(const Function &F, Values &CVs) const;
  void globalizeConstantVectors(Module &M, Function &F,
                                const Values &CVs) const;
};
} // anonymous namespace

char GlobalizeConstantVectors::ID = 0;
INITIALIZE_PASS(GlobalizeConstantVectors, "globalize-constant-vectors",
                "Replace constant vector operands with equivalent loads", false,
                false)

void GlobalizeConstantVectors::findConstantVectors(const Function &F,
                                                   Values &CVs) const {
  for (const_inst_iterator II = inst_begin(F), IE = inst_end(F); II != IE;
       ++II) {
    for (User::const_op_iterator OI = II->op_begin(), OE = II->op_end();
         OI != OE; ++OI) {
      Value *V = OI->get();
      if (isa<ConstantVector>(V) || isa<ConstantDataVector>(V))
        CVs.push_back(V);
    }
  }
}

void
GlobalizeConstantVectors::globalizeConstantVectors(Module &M, Function &F,
                                                   const Values &CVs) const {
  // The first instruction in a function dominates all others, it is
  // therefore a safe insertion point.
  // TODO(jfb) Sink values closer to their use?
  Instruction *FirstInst = F.getEntryBlock().getFirstNonPHI();
  for (Values::const_iterator VI = CVs.begin(), VE = CVs.end(); VI != VE;
       ++VI) {
    Value *V = *VI;
    static const char Name[] = "constant_vector";

    GlobalVariable *GV = new GlobalVariable(
        M, V->getType(), /* isConstant= */ true, GlobalValue::InternalLinkage,
        cast<Constant>(V), Name);
    GV->setAlignment(DL->getPrefTypeAlignment(V->getType()));
    LoadInst *MaterializedGV = new LoadInst(GV, Name, /* isVolatile= */ false,
                                            GV->getAlignment(), FirstInst);

    for (Value::use_iterator UI = V->use_begin(), UE = V->use_end(); UI != UE;
         ++UI) {
      User *U = *UI;
      if (Instruction *I = dyn_cast<Instruction>(U))
        if (I->getParent()->getParent() != &F)
          // Skip uses of the constant vector in other functions: we
          // need to materialize it in every function which has a use.
          continue;
      if (isa<GlobalVariable>(U))
        // Don't replace global uses of the constant vector: we just
        // created a new one. This avoid recursive references.
        continue;
      for (User::op_iterator OI = U->op_begin(), OE = U->op_end(); OI != OE;
           ++OI)
        if (dyn_cast<Value>(*OI) == V)
          // The current operand is a use of the constant vector,
          // replace it with the materialized one.
          *OI = MaterializedGV;
    }
  }
}

bool GlobalizeConstantVectors::runOnModule(Module &M) {
  bool Changed = false;
  DL = &getAnalysis<DataLayout>();
  for (Module::iterator FI = M.begin(), FE = M.end(); FI != FE; ++FI) {
    Function &F = *FI;
    Values ConstantVectors;
    findConstantVectors(F, ConstantVectors);
    if (!ConstantVectors.empty()) {
      Changed = true;
      globalizeConstantVectors(M, F, ConstantVectors);
    }
  }
  return Changed;
}

ModulePass *llvm::createGlobalizeConstantVectorsPass() {
  return new GlobalizeConstantVectors();
}
