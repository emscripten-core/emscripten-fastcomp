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

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/NaCl.h"
#include <utility>
#include <vector>

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
  }
  virtual bool runOnModule(Module &M);

private:
  typedef SmallPtrSet<Constant *, 32> Constants;
  typedef std::pair<Function *, Constants> FunctionConstants;
  typedef std::vector<FunctionConstants> FunctionConstantList;
  typedef DenseMap<Constant *, GlobalVariable *> GlobalizedConstants;
  const DataLayout *DL;

  void findConstantVectors(const Function &F, Constants &Cs) const;
  void createGlobalConstantVectors(Module &M, const FunctionConstantList &FCs,
                                   GlobalizedConstants &GCs) const;
  void materializeConstantVectors(Function &F, const Constants &Cs,
                                  const GlobalizedConstants &GCs) const;
};

const char Name[] = "constant_vector";
} // anonymous namespace

char GlobalizeConstantVectors::ID = 0;
INITIALIZE_PASS(GlobalizeConstantVectors, "globalize-constant-vectors",
                "Replace constant vector operands with equivalent loads", false,
                false)

void GlobalizeConstantVectors::findConstantVectors(const Function &F,
                                                   Constants &Cs) const {
  for (const_inst_iterator II = inst_begin(F), IE = inst_end(F); II != IE;
       ++II) {
    for (User::const_op_iterator OI = II->op_begin(), OE = II->op_end();
         OI != OE; ++OI) {
      Value *V = OI->get();
      if (isa<ConstantVector>(V) || isa<ConstantDataVector>(V) ||
          isa<ConstantAggregateZero>(V))
        Cs.insert(cast<Constant>(V));
    }
  }
}

void GlobalizeConstantVectors::createGlobalConstantVectors(
    Module &M, const FunctionConstantList &FCs,
    GlobalizedConstants &GCs) const {
  for (FunctionConstantList::const_iterator FCI = FCs.begin(), FCE = FCs.end();
       FCI != FCE; ++FCI) {
    const Constants &Cs = FCI->second;

    for (Constants::const_iterator CI = Cs.begin(), CE = Cs.end(); CI != CE;
         ++CI) {
      Constant *C = *CI;
      if (GCs.find(C) != GCs.end())
        continue; // The vector has already been globalized.
      GlobalVariable *GV =
          new GlobalVariable(M, C->getType(), /* isConstant= */ true,
                             GlobalValue::InternalLinkage, C, Name);
      GV->setAlignment(DL->getPrefTypeAlignment(C->getType()));
      GV->setUnnamedAddr(true); // The content is significant, not the address.
      GCs[C] = GV;
    }
  }
}

void GlobalizeConstantVectors::materializeConstantVectors(
    Function &F, const Constants &Cs, const GlobalizedConstants &GCs) const {
  // The first instruction in a function dominates all others, it is therefore a
  // safe insertion point.
  Instruction *FirstInst = F.getEntryBlock().getFirstNonPHI();

  for (Constants::const_iterator CI = Cs.begin(), CE = Cs.end(); CI != CE;
       ++CI) {
    Constant *C = *CI;
    GlobalizedConstants::const_iterator GVI = GCs.find(C);
    assert(GVI != GCs.end());
    GlobalVariable *GV = GVI->second;
    LoadInst *MaterializedGV = new LoadInst(GV, Name, /* isVolatile= */ false,
                                            GV->getAlignment(), FirstInst);

    // Find users of the constant vector.
    typedef SmallVector<User *, 32> UserList;
    UserList CVUsers;
    for (auto U : C->users()) {
      if (Instruction *I = dyn_cast<Instruction>(U))
        if (I->getParent()->getParent() != &F)
          // Skip uses of the constant vector in other functions: we need to
          // materialize it in every function which has a use.
          continue;
      if (isa<Constant>(U))
        // Don't replace global uses of the constant vector: we just created a
        // new one. This avoid recursive references.
        // Also, it's not legal to replace a constant's operand with
        // a non-constant (the load instruction).
        continue;
      CVUsers.push_back(U);
    }

    // Replace these Users. Must be done separately to avoid invalidating the
    // User iterator.
    for (UserList::iterator UI = CVUsers.begin(), UE = CVUsers.end(); UI != UE;
         ++UI) {
      User *U = *UI;
      for (User::op_iterator OI = U->op_begin(), OE = U->op_end(); OI != OE;
           ++OI)
        if (dyn_cast<Constant>(*OI) == C)
          // The current operand is a use of the constant vector, replace it
          // with the materialized one.
          *OI = MaterializedGV;
    }
  }
}

bool GlobalizeConstantVectors::runOnModule(Module &M) {
  DL = &M.getDataLayout();

  FunctionConstantList FCs;
  FCs.reserve(M.size());
  for (Module::iterator FI = M.begin(), FE = M.end(); FI != FE; ++FI) {
    Constants Cs;
    findConstantVectors(*FI, Cs);
    if (!Cs.empty())
      FCs.push_back(std::make_pair(&*FI, Cs));
  }

  GlobalizedConstants GCs;
  createGlobalConstantVectors(M, FCs, GCs);

  for (FunctionConstantList::const_iterator FCI = FCs.begin(), FCE = FCs.end();
       FCI != FCE; ++FCI)
    materializeConstantVectors(*FCI->first, FCI->second, GCs);

  return FCs.empty();
}

ModulePass *llvm::createGlobalizeConstantVectorsPass() {
  return new GlobalizeConstantVectors();
}
