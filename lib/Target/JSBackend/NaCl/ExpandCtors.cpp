//===- ExpandCtors.cpp - Convert ctors/dtors to concrete arrays -----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass converts LLVM's special symbols llvm.global_ctors and
// llvm.global_dtors to concrete arrays, __init_array_start/end and
// __fini_array_start/end, that are usable by a C library.
//
// This pass sorts the contents of global_ctors/dtors according to the
// priority values they contain and removes the priority values.
//
//===----------------------------------------------------------------------===//

#include <vector>

#include "llvm/Pass.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/TypeBuilder.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

namespace {
  struct ExpandCtors : public ModulePass {
    static char ID; // Pass identification, replacement for typeid
    ExpandCtors() : ModulePass(ID) {
      initializeExpandCtorsPass(*PassRegistry::getPassRegistry());
    }

    virtual bool runOnModule(Module &M);
  };
}

char ExpandCtors::ID = 0;
INITIALIZE_PASS(ExpandCtors, "nacl-expand-ctors",
                "Hook up constructor and destructor arrays to libc",
                false, false)

static void setGlobalVariableValue(Module &M, const char *Name,
                                   Constant *Value) {
  if (GlobalVariable *Var = M.getNamedGlobal(Name)) {
    if (Var->hasInitializer()) {
      report_fatal_error(std::string("Variable ") + Name +
                         " already has an initializer");
    }
    Var->replaceAllUsesWith(ConstantExpr::getBitCast(Value, Var->getType()));
    Var->eraseFromParent();
  }
}

struct FuncArrayEntry {
  uint64_t priority;
  Constant *func;
};

static bool compareEntries(FuncArrayEntry Entry1, FuncArrayEntry Entry2) {
  return Entry1.priority < Entry2.priority;
}

static void readFuncList(GlobalVariable *Array, std::vector<Constant*> *Funcs) {
  if (!Array->hasInitializer())
    return;
  Constant *Init = Array->getInitializer();
  ArrayType *Ty = dyn_cast<ArrayType>(Init->getType());
  if (!Ty) {
    errs() << "Initializer: " << *Array->getInitializer() << "\n";
    report_fatal_error("ExpandCtors: Initializer is not of array type");
  }
  if (Ty->getNumElements() == 0)
    return;
  ConstantArray *InitList = dyn_cast<ConstantArray>(Init);
  if (!InitList) {
    errs() << "Initializer: " << *Array->getInitializer() << "\n";
    report_fatal_error("ExpandCtors: Unexpected initializer ConstantExpr");
  }
  std::vector<FuncArrayEntry> FuncsToSort;
  for (unsigned Index = 0; Index < InitList->getNumOperands(); ++Index) {
    ConstantStruct *CS = cast<ConstantStruct>(InitList->getOperand(Index));
    FuncArrayEntry Entry;
    Entry.priority = cast<ConstantInt>(CS->getOperand(0))->getZExtValue();
    Entry.func = CS->getOperand(1);
    FuncsToSort.push_back(Entry);
  }

  std::sort(FuncsToSort.begin(), FuncsToSort.end(), compareEntries);
  for (std::vector<FuncArrayEntry>::iterator Iter = FuncsToSort.begin();
       Iter != FuncsToSort.end();
       ++Iter) {
    Funcs->push_back(Iter->func);
  }
}

static void defineFuncArray(Module &M, const char *LlvmArrayName,
                            const char *StartSymbol,
                            const char *EndSymbol) {
  std::vector<Constant*> Funcs;

  GlobalVariable *Array = M.getNamedGlobal(LlvmArrayName);
  if (Array) {
    readFuncList(Array, &Funcs);
    // No code should be referencing global_ctors/global_dtors,
    // because this symbol is internal to LLVM.
    Array->eraseFromParent();
  }

  Type *FuncTy = FunctionType::get(Type::getVoidTy(M.getContext()), false);
  Type *FuncPtrTy = FuncTy->getPointerTo();
  ArrayType *ArrayTy = ArrayType::get(FuncPtrTy, Funcs.size());
  GlobalVariable *NewArray =
      new GlobalVariable(M, ArrayTy, /* isConstant= */ true,
                         GlobalValue::InternalLinkage,
                         ConstantArray::get(ArrayTy, Funcs));
  setGlobalVariableValue(M, StartSymbol, NewArray);
  // We do this last so that LLVM gives NewArray the name
  // "__{init,fini}_array_start" without adding any suffixes to
  // disambiguate from the original GlobalVariable's name.  This is
  // not essential -- it just makes the output easier to understand
  // when looking at symbols for debugging.
  NewArray->setName(StartSymbol);

  // We replace "__{init,fini}_array_end" with the address of the end
  // of NewArray.  This removes the name "__{init,fini}_array_end"
  // from the output, which is not ideal for debugging.  Ideally we
  // would convert "__{init,fini}_array_end" to being a GlobalAlias
  // that points to the end of the array.  However, unfortunately LLVM
  // does not generate correct code when a GlobalAlias contains a
  // GetElementPtr ConstantExpr.
  Constant *NewArrayEnd =
      ConstantExpr::getGetElementPtr(ArrayTy, NewArray,
                                     ConstantInt::get(M.getContext(),
                                                      APInt(32, 1)));
  setGlobalVariableValue(M, EndSymbol, NewArrayEnd);
}

bool ExpandCtors::runOnModule(Module &M) {
  defineFuncArray(M, "llvm.global_ctors",
                  "__init_array_start", "__init_array_end");
  defineFuncArray(M, "llvm.global_dtors",
                  "__fini_array_start", "__fini_array_end");
  return true;
}

ModulePass *llvm::createExpandCtorsPass() {
  return new ExpandCtors();
}
