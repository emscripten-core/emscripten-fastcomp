//===- PromoteI1Ops.cpp - Promote various operations on the i1 type--------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass expands out various operations on the i1 type so that
// these i1 operations do not need to be supported by the PNaCl
// translator.
//
// This is similar to the PromoteIntegers pass in that it removes uses
// of an unusual-size integer type.  The difference is that i1 remains
// a valid type in other operations.  i1 can still be used in phi
// nodes, "select" instructions, in "sext" and "zext", and so on.  In
// contrast, the integer types that PromoteIntegers removes are not
// allowed in any context by PNaCl's ABI verifier.
//
// This pass expands out the following:
//
//  * i1 loads and stores.
//  * All i1 comparisons and arithmetic operations, with the exception
//    of "and", "or" and "xor", because these are used in practice and
//    don't overflow.
//
// "switch" instructions on i1 are also disallowed by the PNaCl ABI
// verifier, but they don't seem to be generated in practice and so
// they are not currently expanded out by this pass.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

namespace {
  class PromoteI1Ops : public BasicBlockPass {
  public:
    static char ID; // Pass identification, replacement for typeid
    PromoteI1Ops() : BasicBlockPass(ID) {
      initializePromoteI1OpsPass(*PassRegistry::getPassRegistry());
    }

    virtual bool runOnBasicBlock(BasicBlock &BB);
  };
}

char PromoteI1Ops::ID = 0;
INITIALIZE_PASS(PromoteI1Ops, "nacl-promote-i1-ops",
                "Promote various operations on the i1 type",
                false, false)

static Value *promoteValue(Value *Val, bool SignExt, Instruction *InsertPt) {
  Instruction::CastOps CastType =
      SignExt ? Instruction::SExt : Instruction::ZExt;
  return CopyDebug(CastInst::Create(CastType, Val,
                                    Type::getInt8Ty(Val->getContext()),
                                    Val->getName() + ".expand_i1_val",
                                    InsertPt), InsertPt);
}

bool PromoteI1Ops::runOnBasicBlock(BasicBlock &BB) {
  bool Changed = false;

  Type *I1Ty = Type::getInt1Ty(BB.getContext());
  Type *I8Ty = Type::getInt8Ty(BB.getContext());

  for (BasicBlock::iterator Iter = BB.begin(), E = BB.end(); Iter != E; ) {
    Instruction *Inst = Iter++;
    if (LoadInst *Load = dyn_cast<LoadInst>(Inst)) {
      if (Load->getType() == I1Ty) {
        Changed = true;
        Value *Ptr = CopyDebug(
            new BitCastInst(
                Load->getPointerOperand(), I8Ty->getPointerTo(),
                Load->getPointerOperand()->getName() + ".i8ptr", Load), Load);
        LoadInst *NewLoad = new LoadInst(
            Ptr, Load->getName() + ".pre_trunc", Load);
        CopyDebug(NewLoad, Load);
        CopyLoadOrStoreAttrs(NewLoad, Load);
        Value *Result = CopyDebug(new TruncInst(NewLoad, I1Ty, "", Load), Load);
        Result->takeName(Load);
        Load->replaceAllUsesWith(Result);
        Load->eraseFromParent();
      }
    } else if (StoreInst *Store = dyn_cast<StoreInst>(Inst)) {
      if (Store->getValueOperand()->getType() == I1Ty) {
        Changed = true;
        Value *Ptr = CopyDebug(
            new BitCastInst(
                Store->getPointerOperand(), I8Ty->getPointerTo(),
                Store->getPointerOperand()->getName() + ".i8ptr", Store),
            Store);
        Value *Val = promoteValue(Store->getValueOperand(), false, Store);
        StoreInst *NewStore = new StoreInst(Val, Ptr, Store);
        CopyDebug(NewStore, Store);
        CopyLoadOrStoreAttrs(NewStore, Store);
        Store->eraseFromParent();
      }
    } else if (BinaryOperator *Op = dyn_cast<BinaryOperator>(Inst)) {
      if (Op->getType() == I1Ty &&
          !(Op->getOpcode() == Instruction::And ||
            Op->getOpcode() == Instruction::Or ||
            Op->getOpcode() == Instruction::Xor)) {
        Value *Arg1 = promoteValue(Op->getOperand(0), false, Op);
        Value *Arg2 = promoteValue(Op->getOperand(1), false, Op);
        Value *NewOp = CopyDebug(
            BinaryOperator::Create(
                Op->getOpcode(), Arg1, Arg2,
                Op->getName() + ".pre_trunc", Op), Op);
        Value *Result = CopyDebug(new TruncInst(NewOp, I1Ty, "", Op), Op);
        Result->takeName(Op);
        Op->replaceAllUsesWith(Result);
        Op->eraseFromParent();
      }
    } else if (ICmpInst *Op = dyn_cast<ICmpInst>(Inst)) {
      if (Op->getOperand(0)->getType() == I1Ty) {
        Value *Arg1 = promoteValue(Op->getOperand(0), Op->isSigned(), Op);
        Value *Arg2 = promoteValue(Op->getOperand(1), Op->isSigned(), Op);
        Value *Result = CopyDebug(
            new ICmpInst(Op, Op->getPredicate(), Arg1, Arg2, ""), Op);
        Result->takeName(Op);
        Op->replaceAllUsesWith(Result);
        Op->eraseFromParent();
      }
    }
  }
  return Changed;
}

BasicBlockPass *llvm::createPromoteI1OpsPass() {
  return new PromoteI1Ops();
}
