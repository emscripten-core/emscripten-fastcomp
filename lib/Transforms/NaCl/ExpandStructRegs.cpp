//===- ExpandStructRegs.cpp - Expand out variables with struct type--------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass expands out some uses of LLVM variables
// (a.k.a. registers) of struct type.  It replaces loads and stores of
// structs with separate loads and stores of the structs' fields.  The
// motivation is to omit struct types from PNaCl's stable ABI.
//
// ExpandStructRegs does not yet handle all possible uses of struct
// values.  It is intended to handle the uses that Clang and the SROA
// pass generate.  Clang generates struct loads and stores, along with
// extractvalue instructions, in its implementation of C++ method
// pointers, and the SROA pass sometimes converts this code to using
// insertvalue instructions too.
//
// ExpandStructRegs does not handle:
//
//  * Nested struct types.
//  * Array types.
//  * Function types containing arguments or return values of struct
//    type without the "byval" or "sret" attributes.  Since by-value
//    struct-passing generally uses "byval"/"sret", this does not
//    matter.
//
// Other limitations:
//
//  * ExpandStructRegs does not attempt to use memcpy() where that
//    might be more appropriate than copying fields individually.
//  * ExpandStructRegs does not preserve the contents of padding
//    between fields when copying structs.  However, the contents of
//    padding fields are not defined anyway.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

namespace {
  struct ExpandStructRegs : public FunctionPass {
    static char ID; // Pass identification, replacement for typeid
    ExpandStructRegs() : FunctionPass(ID) {
      initializeExpandStructRegsPass(*PassRegistry::getPassRegistry());
    }

    virtual bool runOnFunction(Function &F);
  };
}

char ExpandStructRegs::ID = 0;
INITIALIZE_PASS(ExpandStructRegs, "expand-struct-regs",
                "Expand out variables with struct types", false, false)

static void SplitUpPHINode(PHINode *Phi) {
  StructType *STy = cast<StructType>(Phi->getType());

  Value *NewStruct = UndefValue::get(STy);
  Instruction *NewStructInsertPt = Phi->getParent()->getFirstInsertionPt();

  // Create a separate PHINode for each struct field.
  for (unsigned Index = 0; Index < STy->getNumElements(); ++Index) {
    SmallVector<unsigned, 1> EVIndexes;
    EVIndexes.push_back(Index);

    PHINode *NewPhi = PHINode::Create(
        STy->getElementType(Index), Phi->getNumIncomingValues(),
        Phi->getName() + ".index", Phi);
    CopyDebug(NewPhi, Phi);
    for (unsigned PhiIndex = 0; PhiIndex < Phi->getNumIncomingValues();
         ++PhiIndex) {
      BasicBlock *IncomingBB = Phi->getIncomingBlock(PhiIndex);
      Value *EV = CopyDebug(
          ExtractValueInst::Create(
              Phi->getIncomingValue(PhiIndex), EVIndexes,
              Phi->getName() + ".extract", IncomingBB->getTerminator()), Phi);
      NewPhi->addIncoming(EV, IncomingBB);
    }

    // Reconstruct the original struct value.
    NewStruct = CopyDebug(
        InsertValueInst::Create(NewStruct, NewPhi, EVIndexes,
                                Phi->getName() + ".insert", NewStructInsertPt),
        Phi);
  }
  Phi->replaceAllUsesWith(NewStruct);
  Phi->eraseFromParent();
}

static void SplitUpSelect(SelectInst *Select) {
  StructType *STy = cast<StructType>(Select->getType());
  Value *NewStruct = UndefValue::get(STy);

  // Create a separate SelectInst for each struct field.
  for (unsigned Index = 0; Index < STy->getNumElements(); ++Index) {
    SmallVector<unsigned, 1> EVIndexes;
    EVIndexes.push_back(Index);

    Value *TrueVal = CopyDebug(
        ExtractValueInst::Create(Select->getTrueValue(), EVIndexes,
                                 Select->getName() + ".extract", Select),
        Select);
    Value *FalseVal = CopyDebug(
        ExtractValueInst::Create(Select->getFalseValue(), EVIndexes,
                                 Select->getName() + ".extract", Select),
        Select);
    Value *NewSelect = CopyDebug(
        SelectInst::Create(Select->getCondition(), TrueVal, FalseVal,
                           Select->getName() + ".index", Select), Select);

    // Reconstruct the original struct value.
    NewStruct = CopyDebug(
        InsertValueInst::Create(NewStruct, NewSelect, EVIndexes,
                                Select->getName() + ".insert", Select),
        Select);
  }
  Select->replaceAllUsesWith(NewStruct);
  Select->eraseFromParent();
}

template <class InstType>
static void ProcessLoadOrStoreAttrs(InstType *Dest, InstType *Src) {
  CopyDebug(Dest, Src);
  Dest->setVolatile(Src->isVolatile());
  if (Src->isAtomic()) {
    errs() << "Use: " << *Src << "\n";
    report_fatal_error("Atomic struct loads/stores not supported");
  }
  // Make a pessimistic assumption about alignment.  Preserving
  // alignment information here is tricky and is not really desirable
  // for PNaCl because mistakes here could lead to non-portable
  // behaviour.
  Dest->setAlignment(1);
}

static void SplitUpStore(StoreInst *Store) {
  StructType *STy = cast<StructType>(Store->getValueOperand()->getType());
  // Create a separate store instruction for each struct field.
  for (unsigned Index = 0; Index < STy->getNumElements(); ++Index) {
    SmallVector<Value *, 2> Indexes;
    Indexes.push_back(ConstantInt::get(Store->getContext(), APInt(32, 0)));
    Indexes.push_back(ConstantInt::get(Store->getContext(), APInt(32, Index)));
    Value *GEP = CopyDebug(GetElementPtrInst::Create(
                               Store->getPointerOperand(), Indexes,
                               Store->getPointerOperand()->getName() + ".index",
                               Store), Store);
    SmallVector<unsigned, 1> EVIndexes;
    EVIndexes.push_back(Index);
    Value *Field = ExtractValueInst::Create(Store->getValueOperand(),
                                            EVIndexes, "", Store);
    StoreInst *NewStore = new StoreInst(Field, GEP, Store);
    ProcessLoadOrStoreAttrs(NewStore, Store);
  }
  Store->eraseFromParent();
}

static void SplitUpLoad(LoadInst *Load) {
  StructType *STy = cast<StructType>(Load->getType());
  Value *NewStruct = UndefValue::get(STy);

  // Create a separate load instruction for each struct field.
  for (unsigned Index = 0; Index < STy->getNumElements(); ++Index) {
    SmallVector<Value *, 2> Indexes;
    Indexes.push_back(ConstantInt::get(Load->getContext(), APInt(32, 0)));
    Indexes.push_back(ConstantInt::get(Load->getContext(), APInt(32, Index)));
    Value *GEP = CopyDebug(
        GetElementPtrInst::Create(Load->getPointerOperand(), Indexes,
                                  Load->getName() + ".index", Load), Load);
    LoadInst *NewLoad = new LoadInst(GEP, Load->getName() + ".field", Load);
    ProcessLoadOrStoreAttrs(NewLoad, Load);

    // Reconstruct the struct value.
    SmallVector<unsigned, 1> EVIndexes;
    EVIndexes.push_back(Index);
    NewStruct = CopyDebug(
        InsertValueInst::Create(NewStruct, NewLoad, EVIndexes,
                                Load->getName() + ".insert", Load), Load);
  }
  Load->replaceAllUsesWith(NewStruct);
  Load->eraseFromParent();
}

static void ExpandExtractValue(ExtractValueInst *EV) {
  // Search for the insertvalue instruction that inserts the struct
  // field referenced by this extractvalue instruction.
  Value *StructVal = EV->getAggregateOperand();
  Value *ResultField;
  for (;;) {
    if (InsertValueInst *IV = dyn_cast<InsertValueInst>(StructVal)) {
      if (EV->getNumIndices() != 1 || IV->getNumIndices() != 1) {
        errs() << "Value: " << *EV << "\n";
        errs() << "Value: " << *IV << "\n";
        report_fatal_error("ExpandStructRegs does not handle nested structs");
      }
      if (EV->getIndices()[0] == IV->getIndices()[0]) {
        ResultField = IV->getInsertedValueOperand();
        break;
      }
      // No match.  Try the next struct value in the chain.
      StructVal = IV->getAggregateOperand();
    } else if (Constant *C = dyn_cast<Constant>(StructVal)) {
      ResultField = ConstantExpr::getExtractValue(C, EV->getIndices());
      break;
    } else {
      errs() << "Value: " << *StructVal << "\n";
      report_fatal_error("Unrecognized struct value");
    }
  }
  EV->replaceAllUsesWith(ResultField);
  EV->eraseFromParent();
}

bool ExpandStructRegs::runOnFunction(Function &Func) {
  bool Changed = false;

  // Split up aggregate loads, stores and phi nodes into operations on
  // scalar types.  This inserts extractvalue and insertvalue
  // instructions which we will expand out later.
  for (Function::iterator BB = Func.begin(), E = Func.end();
       BB != E; ++BB) {
    for (BasicBlock::iterator Iter = BB->begin(), E = BB->end();
         Iter != E; ) {
      Instruction *Inst = Iter++;
      if (StoreInst *Store = dyn_cast<StoreInst>(Inst)) {
        if (Store->getValueOperand()->getType()->isStructTy()) {
          SplitUpStore(Store);
          Changed = true;
        }
      } else if (LoadInst *Load = dyn_cast<LoadInst>(Inst)) {
        if (Load->getType()->isStructTy()) {
          SplitUpLoad(Load);
          Changed = true;
        }
      } else if (PHINode *Phi = dyn_cast<PHINode>(Inst)) {
        if (Phi->getType()->isStructTy()) {
          SplitUpPHINode(Phi);
          Changed = true;
        }
      } else if (SelectInst *Select = dyn_cast<SelectInst>(Inst)) {
        if (Select->getType()->isStructTy()) {
          SplitUpSelect(Select);
          Changed = true;
        }
      }
    }
  }

  // Expand out all the extractvalue instructions.  Also collect up
  // the insertvalue instructions for later deletion so that we do not
  // need to make extra passes across the whole function.
  SmallVector<Instruction *, 10> ToErase;
  for (Function::iterator BB = Func.begin(), E = Func.end();
       BB != E; ++BB) {
    for (BasicBlock::iterator Iter = BB->begin(), E = BB->end();
         Iter != E; ) {
      Instruction *Inst = Iter++;
      if (ExtractValueInst *EV = dyn_cast<ExtractValueInst>(Inst)) {
        ExpandExtractValue(EV);
        Changed = true;
      } else if (isa<InsertValueInst>(Inst)) {
        ToErase.push_back(Inst);
        Changed = true;
      }
    }
  }
  // Delete the insertvalue instructions.  These can reference each
  // other, so we must do dropAllReferences() before doing
  // eraseFromParent(), otherwise we will try to erase instructions
  // that are still referenced.
  for (SmallVectorImpl<Instruction *>::iterator I = ToErase.begin(),
           E = ToErase.end();
       I != E; ++I) {
    (*I)->dropAllReferences();
  }
  for (SmallVectorImpl<Instruction *>::iterator I = ToErase.begin(),
           E = ToErase.end();
       I != E; ++I) {
    (*I)->eraseFromParent();
  }
  return Changed;
}

FunctionPass *llvm::createExpandStructRegsPass() {
  return new ExpandStructRegs();
}
