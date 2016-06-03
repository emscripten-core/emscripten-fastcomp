//===-- ExpandBigSwitches.cpp - Alloca optimization ---------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===-----------------------------------------------------------------------===//
//
// Very large switches can be a problem for JS engines. We split them up here.
//
//===-----------------------------------------------------------------------===//

#include "OptPasses.h"

#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"

#include <vector>
#include <algorithm>

namespace llvm {

/*
 * Find cases where an alloca is used only to load and store a single value,
 * even though it is bitcast. Then replace it with a direct alloca of that
 * simple type, and avoid the bitcasts.
 */

struct ExpandBigSwitches : public FunctionPass {
  static char ID; // Pass identification, replacement for typeid
  ExpandBigSwitches() : FunctionPass(ID) {}
    // XXX initialize..(*PassRegistry::getPassRegistry()); }

  bool runOnFunction(Function &Func) override;

  const char *getPassName() const override { return "ExpandBigSwitches"; }
};

char ExpandBigSwitches::ID = 0;

// Check if we need to split a switch. If so, return the median, on which we will do so
static bool ConsiderSplit(const SwitchInst *SI, int64_t& Median) {
  int64_t Minn = INT64_MAX, Maxx = INT64_MIN;
  std::vector<int64_t> Values;
  for (SwitchInst::ConstCaseIt i = SI->case_begin(), e = SI->case_end(); i != e; ++i) {
    int64_t Curr = i.getCaseValue()->getSExtValue();
    if (Curr < Minn) Minn = Curr;
    if (Curr > Maxx) Maxx = Curr;
    Values.push_back(Curr);
  }
  int64_t Range = Maxx - Minn;
  int Num = SI->getNumCases();
  if (Num < 1024 && Range <= 10*1024 && (Range/Num) <= 1024) return false;
  // this is either too big, or too rangey
  std::sort(Values.begin(), Values.end());
  Median = Values[Values.size()/2];
  return true;
}

static void DoSplit(SwitchInst *SI, int64_t Median) {
  // switch (x) { ..very many.. }
  //
  //   ==>
  //
  // if (x < median) {
  //   switch (x) { ..first half.. }
  // } else {
  //   switch (x) { ..second half.. }
  // }

  BasicBlock *SwitchBB = SI->getParent();
  Function *F = SwitchBB->getParent();
  Value *Condition = SI->getOperand(0);
  BasicBlock *DD = SI->getDefaultDest();
  unsigned NumItems = SI->getNumCases();
  Type *T = Condition->getType();

  Instruction *Check = new ICmpInst(SI, ICmpInst::ICMP_SLT, Condition, ConstantInt::get(T, Median), "switch-split");
  BasicBlock *LowBB = BasicBlock::Create(SI->getContext(), "switchsplit_low", F);
  BasicBlock *HighBB = BasicBlock::Create(SI->getContext(), "switchsplit_high", F);
  BranchInst *Br = BranchInst::Create(LowBB, HighBB, Check, SwitchBB);

  SwitchInst *LowSI = SwitchInst::Create(Condition, DD, NumItems/2, LowBB);
  SwitchInst *HighSI = SwitchInst::Create(Condition, DD, NumItems/2, HighBB);

  for (SwitchInst::CaseIt i = SI->case_begin(), e = SI->case_end(); i != e; ++i) {
    BasicBlock *BB = i.getCaseSuccessor();
    auto Value = i.getCaseValue();
    SwitchInst *NewSI = Value->getSExtValue() < Median ? LowSI : HighSI;
    NewSI->addCase(Value, BB);
    // update phis
    BasicBlock *NewBB = NewSI->getParent();
    for (BasicBlock::iterator I = BB->begin(); I != BB->end(); ++I) {
      PHINode *Phi = dyn_cast<PHINode>(I);
      if (!Phi) break;
      int Index = Phi->getBasicBlockIndex(SwitchBB);
      if (Index < 0) continue;
      Phi->addIncoming(Phi->getIncomingValue(Index), NewBB);
      Phi->removeIncomingValue(Index);
    }
  }

  // fix default dest
  for (BasicBlock::iterator I = DD->begin(); I != DD->end(); ++I) {
    PHINode *Phi = dyn_cast<PHINode>(I);
    if (!Phi) break;
    int Index = Phi->getBasicBlockIndex(SwitchBB);
    if (Index < 0) continue;
    Phi->addIncoming(Phi->getIncomingValue(Index), LowBB);
    Phi->addIncoming(Phi->getIncomingValue(Index), HighBB);
    Phi->removeIncomingValue(Index);
  }

  // finish up
  SI->eraseFromParent();
  assert(SwitchBB->getTerminator() == Br);
  assert(LowSI->getNumCases() + HighSI->getNumCases() == NumItems);
  assert(LowSI->getNumCases() < HighSI->getNumCases() + 2);
  assert(HighSI->getNumCases() < LowSI->getNumCases() + 2);
}

bool ExpandBigSwitches::runOnFunction(Function &Func) {
  bool Changed = false;

  struct SplitInfo {
    SwitchInst *SI;
    int64_t Median;
  };

  while (1) { // repetively split in 2
    std::vector<SplitInfo> ToSplit;
    // find switches we need to split
    for (Function::iterator B = Func.begin(), E = Func.end(); B != E; ++B) {
      Instruction *I = B->getTerminator();
      SwitchInst *SI = dyn_cast<SwitchInst>(I);
      if (!SI) continue;
      SplitInfo Curr;
      if (!ConsiderSplit(SI, Curr.Median)) continue;
      Curr.SI = SI;
      Changed = true;
      ToSplit.push_back(Curr);
    }
    if (ToSplit.size() == 0) break;
    // split them
    for (auto& Curr : ToSplit) {
      DoSplit(Curr.SI, Curr.Median);
    }
  }

  return Changed;
}

//

extern FunctionPass *createEmscriptenExpandBigSwitchesPass() {
  return new ExpandBigSwitches();
}

} // End llvm namespace
