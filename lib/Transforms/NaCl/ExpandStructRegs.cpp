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
// ExpandStructRegs does not handle all possible uses of struct
// values.  It is only intended to handle the uses that Clang
// generates.  Clang generates struct loads and stores, along with
// extractvalue instructions, in its implementation of C++ method
// pointers.
//
// ExpandStructRegs does not handle:
//
//  * The insertvalue instruction, which does not appear to be
//    generated anywhere.
//  * PHI nodes of struct type.
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

static void ExpandStore(StoreInst *Store) {
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
    Value *Field;
    if (Constant *C = dyn_cast<Constant>(Store->getValueOperand())) {
      Field = ConstantExpr::getExtractValue(C, EVIndexes);
    } else {
      Field = ExtractValueInst::Create(
          Store->getValueOperand(), EVIndexes, "", Store);
    }
    StoreInst *NewStore = new StoreInst(Field, GEP, Store);
    ProcessLoadOrStoreAttrs(NewStore, Store);
  }
  Store->eraseFromParent();
}

static void ExpandLoad(LoadInst *Load) {
  StructType *STy = cast<StructType>(Load->getType());
  // Create a separate load instruction for each struct field.
  SmallVector<Value *, 5> Fields;
  for (unsigned Index = 0; Index < STy->getNumElements(); ++Index) {
    SmallVector<Value *, 2> Indexes;
    Indexes.push_back(ConstantInt::get(Load->getContext(), APInt(32, 0)));
    Indexes.push_back(ConstantInt::get(Load->getContext(), APInt(32, Index)));
    Value *GEP = CopyDebug(
        GetElementPtrInst::Create(Load->getPointerOperand(), Indexes,
                                  Load->getName() + ".index", Load), Load);
    LoadInst *NewLoad = new LoadInst(GEP, Load->getName() + ".field", Load);
    ProcessLoadOrStoreAttrs(NewLoad, Load);
    Fields.push_back(NewLoad);
  }
  ReplaceUsesOfStructWithFields(Load, Fields);
  Load->eraseFromParent();
}

bool ExpandStructRegs::runOnFunction(Function &Func) {
  bool Changed = false;

  // It is not safe to iterate through the basic block while
  // deleting extractvalue instructions, so make a copy of the
  // instructions we will operate on first.
  SmallVector<StoreInst *, 10> Stores;
  SmallVector<LoadInst *, 10> Loads;
  for (Function::iterator BB = Func.begin(), E = Func.end();
       BB != E; ++BB) {
    for (BasicBlock::iterator Inst = BB->begin(), E = BB->end();
         Inst != E; ++Inst) {
      if (StoreInst *Store = dyn_cast<StoreInst>(Inst)) {
        if (Store->getValueOperand()->getType()->isStructTy()) {
          Stores.push_back(Store);
        }
      } else if (LoadInst *Load = dyn_cast<LoadInst>(Inst)) {
        if (Load->getType()->isStructTy()) {
          Loads.push_back(Load);
        }
      }
    }
  }

  // Expand stores first.  This will introduce extractvalue
  // instructions.
  for (SmallVectorImpl<StoreInst *>::iterator Inst = Stores.begin(),
         E = Stores.end(); Inst != E; ++Inst) {
    ExpandStore(*Inst);
    Changed = true;
  }
  // Expanding loads will remove the extractvalue instructions we
  // previously introduced.
  for (SmallVectorImpl<LoadInst *>::iterator Inst = Loads.begin(),
         E = Loads.end(); Inst != E; ++Inst) {
    ExpandLoad(*Inst);
    Changed = true;
  }
  return Changed;
}

FunctionPass *llvm::createExpandStructRegsPass() {
  return new ExpandStructRegs();
}
