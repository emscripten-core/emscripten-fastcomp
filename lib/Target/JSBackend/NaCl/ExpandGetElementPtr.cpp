//===- ExpandGetElementPtr.cpp - Expand GetElementPtr into arithmetic------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass expands out GetElementPtr instructions into ptrtoint,
// inttoptr and arithmetic instructions.
//
// This simplifies the language so that the PNaCl translator does not
// need to handle GetElementPtr and struct types as part of a stable
// wire format for PNaCl.
//
// Note that we drop the "inbounds" attribute of GetElementPtr.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

namespace {
  class ExpandGetElementPtr : public BasicBlockPass {
  public:
    static char ID; // Pass identification, replacement for typeid
    ExpandGetElementPtr() : BasicBlockPass(ID) {
      initializeExpandGetElementPtrPass(*PassRegistry::getPassRegistry());
    }

    virtual bool runOnBasicBlock(BasicBlock &BB);
  };
}

char ExpandGetElementPtr::ID = 0;
INITIALIZE_PASS(ExpandGetElementPtr, "expand-getelementptr",
                "Expand out GetElementPtr instructions into arithmetic",
                false, false)

static Value *CastToPtrSize(Value *Val, Instruction *InsertPt,
                            const DebugLoc &Debug, Type *PtrType) {
  unsigned ValSize = Val->getType()->getIntegerBitWidth();
  unsigned PtrSize = PtrType->getIntegerBitWidth();
  if (ValSize == PtrSize)
    return Val;
  Instruction *Inst;
  if (ValSize > PtrSize) {
    Inst = new TruncInst(Val, PtrType, "gep_trunc", InsertPt);
  } else {
    // GEP indexes must be sign-extended.
    Inst = new SExtInst(Val, PtrType, "gep_sext", InsertPt);
  }
  Inst->setDebugLoc(Debug);
  return Inst;
}

static void FlushOffset(Instruction **Ptr, uint64_t *CurrentOffset,
                        Instruction *InsertPt, const DebugLoc &Debug,
                        Type *PtrType) {
  if (*CurrentOffset) {
    *Ptr = BinaryOperator::Create(Instruction::Add, *Ptr,
                                  ConstantInt::get(PtrType, *CurrentOffset),
                                  "gep", InsertPt);
    (*Ptr)->setDebugLoc(Debug);
    *CurrentOffset = 0;
  }
}

static void ExpandGEP(GetElementPtrInst *GEP, DataLayout *DL, Type *PtrType) {
  const DebugLoc &Debug = GEP->getDebugLoc();
  Instruction *Ptr = new PtrToIntInst(GEP->getPointerOperand(), PtrType,
                                      "gep_int", GEP);
  Ptr->setDebugLoc(Debug);

  Type *CurrentTy = GEP->getPointerOperand()->getType();
  // We do some limited constant folding ourselves.  An alternative
  // would be to generate verbose, unfolded output (e.g. multiple
  // adds; adds of zero constants) and use a later pass such as
  // "-instcombine" to clean that up.  However, "-instcombine" can
  // reintroduce GetElementPtr instructions.
  uint64_t CurrentOffset = 0;

  for (GetElementPtrInst::op_iterator Op = GEP->op_begin() + 1;
       Op != GEP->op_end();
       ++Op) {
    Value *Index = *Op;
    if (StructType *StTy = dyn_cast<StructType>(CurrentTy)) {
      uint64_t Field = cast<ConstantInt>(Op)->getZExtValue();
      CurrentTy = StTy->getElementType(Field);
      CurrentOffset += DL->getStructLayout(StTy)->getElementOffset(Field);
    } else {
      CurrentTy = cast<SequentialType>(CurrentTy)->getElementType();
      uint64_t ElementSize = DL->getTypeAllocSize(CurrentTy);
      if (ConstantInt *C = dyn_cast<ConstantInt>(Index)) {
        CurrentOffset += C->getSExtValue() * ElementSize;
      } else {
        FlushOffset(&Ptr, &CurrentOffset, GEP, Debug, PtrType);
        Index = CastToPtrSize(Index, GEP, Debug, PtrType);
        if (ElementSize != 1) {
          Index = CopyDebug(
              BinaryOperator::Create(Instruction::Mul, Index,
                                     ConstantInt::get(PtrType, ElementSize),
                                     "gep_array", GEP),
              GEP);
        }
        Ptr = BinaryOperator::Create(Instruction::Add, Ptr,
                                     Index, "gep", GEP);
        Ptr->setDebugLoc(Debug);
      }
    }
  }
  FlushOffset(&Ptr, &CurrentOffset, GEP, Debug, PtrType);

  assert(CurrentTy == GEP->getType()->getElementType());
  Instruction *Result = new IntToPtrInst(Ptr, GEP->getType(), "", GEP);
  Result->setDebugLoc(Debug);
  Result->takeName(GEP);
  GEP->replaceAllUsesWith(Result);
  GEP->eraseFromParent();
}

bool ExpandGetElementPtr::runOnBasicBlock(BasicBlock &BB) {
  bool Modified = false;
  DataLayout DL(BB.getParent()->getParent());
  Type *PtrType = DL.getIntPtrType(BB.getContext());

  for (BasicBlock::InstListType::iterator Iter = BB.begin();
       Iter != BB.end(); ) {
    Instruction *Inst = &*Iter++;
    if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(Inst)) {
      Modified = true;
      ExpandGEP(GEP, &DL, PtrType);
    }
  }
  return Modified;
}

BasicBlockPass *llvm::createExpandGetElementPtrPass() {
  return new ExpandGetElementPtr();
}
