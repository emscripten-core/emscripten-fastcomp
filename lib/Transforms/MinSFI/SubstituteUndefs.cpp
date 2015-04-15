//===- SubstituteUndefs.cpp - Replace undefs with deterministic constants -===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// PNaCl bitcode may contain undefined values inside function bodies, i.e. as
// a placeholder for numerical constants and constant vectors. Their actual
// value at runtime will most likely be the current value from one of the
// registers or from the native stack.
//
// Using undefined values, the sandboxed code could obtain protected values,
// such as the base address of the address subspace or a value from another
// protection domain left in the register file. Additionally, undefined values
// may introduce undesirable non-determinism.
//
// This pass therefore substitutes all undefined expressions with predefined
// constants.
//
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/Transforms/MinSFI.h"

using namespace llvm;

static const uint64_t SubstInt = 0xBAADF00DCAFEBABE;
static const double SubstFloat = 3.14159265359;

namespace {
class SubstituteUndefs : public FunctionPass {
 public:
  static char ID;
  SubstituteUndefs() : FunctionPass(ID) {
    initializeSubstituteUndefsPass(*PassRegistry::getPassRegistry());
  }

  virtual bool runOnFunction(Function &Func);
};
}  // namespace

static inline bool isScalarOrVectorInteger(Type *T) {
  if (T->isIntegerTy())
    return true;
  else if (T->isVectorTy() && T->getVectorElementType()->isIntegerTy())
    return true;
  else
    return false;
}

static inline bool isScalarOrVectorFloatingPoint(Type *T) {
  if (T->isFloatingPointTy())
    return true;
  else if (T->isVectorTy() && T->getVectorElementType()->isFloatingPointTy())
    return true;
  else
    return false;
}

bool SubstituteUndefs::runOnFunction(Function &Func) {
  bool HadUndefs = false;
  for (Function::iterator BB = Func.begin(), E = Func.end(); BB != E; ++BB) {
    for (BasicBlock::iterator Inst = BB->begin(), E = BB->end(); Inst != E;
         ++Inst) {
      for (int Index = 0, NumOps = Inst->getNumOperands(); Index < NumOps;
           ++Index) {
        Value *Operand = Inst->getOperand(Index);
        if (isa<UndefValue>(Operand)) {
          HadUndefs = true;
          Type *OpType = Operand->getType();
          if (isScalarOrVectorInteger(OpType))
            Inst->setOperand(Index, ConstantInt::get(OpType, SubstInt));
          else if (isScalarOrVectorFloatingPoint(OpType))
            Inst->setOperand(Index, ConstantFP::get(OpType, SubstFloat));
          else
            assert(false && "Type of undef not permitted by the PNaCl ABI");
        }
      }
    }
  }
  return HadUndefs;
}

char SubstituteUndefs::ID = 0;
INITIALIZE_PASS(SubstituteUndefs, "minsfi-substitute-undefs",
                "Replace undef values with deterministic constants",
                false, false)

FunctionPass *llvm::createSubstituteUndefsPass() {
  return new SubstituteUndefs();
}
