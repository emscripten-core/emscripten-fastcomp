//===- ExpandI64.cpp - Expand out variable argument function calls-----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===------------------------------------------------------------------===//
//
// This pass expands and lowers all i64 operations, into 32-bit operations
// that can be handled by JS in a natural way.
//
// 64-bit variables become pairs of 2 32-bit variables, for the low and
// high 32 bit chunks. This happens for both registers and function
// arguments. Function return values become a return of the low 32 bits
// and a store of the high 32-bits in tempRet0, a global helper variable.
//
// Many operations then become simple pairs of operations, for example
// bitwise AND becomes and AND of each 32-bit chunk. More complex operations
// like addition are lowered into calls into library support code in
// Emscripten (i64Add for example).
//
//===------------------------------------------------------------------===//

#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/NaCl.h"

#include "llvm/Support/raw_ostream.h"
#include <stdio.h>
#define dump(x) fprintf(stderr, x "\n")
#define dumpv(x, ...) fprintf(stderr, x "\n", __VA_ARGS__)
#define dumpfail(x)       { fprintf(stderr, x "\n");              fprintf(stderr, "%s : %d\n", __FILE__, __LINE__); report_fatal_error("fail"); }
#define dumpfailv(x, ...) { fprintf(stderr, x "\n", __VA_ARGS__); fprintf(stderr, "%s : %d\n", __FILE__, __LINE__); report_fatal_error("fail"); }
#define dumpIR(value) { \
  std::string temp; \
  raw_string_ostream stream(temp); \
  stream << *(value); \
  fprintf(stderr, "%s\n", temp.c_str()); \
}
#undef assert
#define assert(x) { if (!(x)) dumpfail(#x); }

using namespace llvm;

namespace {

  struct LowHighPair {
    Value *Low, *High;
  };

  typedef std::vector<Instruction*> SplitInstrs;

  struct SplitInfo {
    SplitInstrs ToFix;  // new instrs, which we fix up later with proper legalized input (if they received illegal input)
    LowHighPair LowHigh; // low and high parts of the legalized output, if the output was illegal
  };

  typedef std::map<Instruction*, SplitInfo> SplitsMap;

  // This is a ModulePass because the pass recreates functions in
  // order to expand i64 arguments to pairs of i32s.
  class ExpandI64 : public ModulePass {
    SplitsMap Splits; // old i64 value to new insts

    // splits a 64-bit instruction into 32-bit chunks. We do
    // not yet have the values yet, as they depend on other
    // splits, so store the parts in Splits, for FinalizeInst.
    void splitInst(Instruction *I, DataLayout& DL);

    // For a 64-bit value, returns the split out chunks
    // representing the low and high parts, that splitInst
    // generated.
    // The value can also be a constant, in which case we just
    // split it.
    LowHighPair getLowHigh(Value *V);

    void finalizeInst(Instruction *I);

    Function *Add, *GetHigh;

    void ensureFuncs();

    Module *TheModule;

  public:
    static char ID;
    ExpandI64() : ModulePass(ID) {
      initializeExpandI64Pass(*PassRegistry::getPassRegistry());

      Add = GetHigh = NULL;
    }

    virtual bool runOnModule(Module &M);
  };
}

char ExpandI64::ID = 0;
INITIALIZE_PASS(ExpandI64, "expand-i64",
                "Expand and lower i64 operations into 32-bit chunks",
                false, false)

//static void ExpandI64Func(Function *Func) {
//}

void ExpandI64::splitInst(Instruction *I, DataLayout& DL) {
  Type *i32 = Type::getInt32Ty(I->getContext());
  Type *i32P = i32->getPointerTo(); // XXX DL->getIntPtrType(I->getContext())
  Value *Zero  = Constant::getNullValue(i32);
  Value *Ones  = Constant::getAllOnesValue(i32);

  switch (I->getOpcode()) {
    case Instruction::SExt: {
      Value *Input = I->getOperand(0);
      Type *T = Input->getType();
      Value *Low;
      if (T->getIntegerBitWidth() < 32) {
        Low = CopyDebug(new SExtInst(Input, i32, "", I), I);
      } else {
        Low = Input;
      }
      Instruction *Check = CopyDebug(new ICmpInst(I, ICmpInst::ICMP_SLE, Low, Zero), I);
      Instruction *High  = CopyDebug(SelectInst::Create(Check, Ones, Zero, "", I), I);
      SplitInfo &Split = Splits[I];
      Split.LowHigh.Low = Low;
      Split.LowHigh.High = High;
      break;
    }
    case Instruction::Store: {
      // store i64 A, i64* P  =>  ai = P ; P4 = ai+4 ; lp = P to i32* ; hp = P4 to i32* ; store l, lp ; store h, hp
      StoreInst *SI = dyn_cast<StoreInst>(I);

      Instruction *AI = CopyDebug(new PtrToIntInst(SI->getPointerOperand(), i32, "", I), I);
      Instruction *P4 = CopyDebug(BinaryOperator::Create(Instruction::Add, AI, ConstantInt::get(i32, 4), "", I), I);
      Instruction *LP = CopyDebug(new IntToPtrInst(AI, i32P, "", I), I);
      Instruction *HP = CopyDebug(new IntToPtrInst(P4, i32P, "", I), I);
      StoreInst *SL = new StoreInst(Zero, LP, I); CopyDebug(SL, I); // will be fixed
      StoreInst *SH = new StoreInst(Zero, HP, I); CopyDebug(SH, I); // will be fixed
      SplitInfo &Split = Splits[I];
      Split.ToFix.push_back(SL);
      Split.ToFix.push_back(SH);

      SL->setAlignment(SI->getAlignment());
      SH->setAlignment(SI->getAlignment());
      break;
    }
    case Instruction::Add: {
      break; // XXX XXX XXX
      SmallVector<Value *, 4> Args;
      for (int i = 0; i < 4; i++) Args.push_back(Zero); // will be fixed 
      ensureFuncs();
      //Instruction *Low = CopyDebug(CallInst::Create(Add, Args, "", I), I);
      //Instruction *High = CopyDebug(CallInst::Create(GetHigh, "", I), I);
      //SplitInfo &Split = Splits[I];
      //Split.push_back(Low);
      //Split.push_back(High);
      break;
    }
    //default: // FIXME: abort if we hit something we don't support
  }
}

LowHighPair ExpandI64::getLowHigh(Value *V) {
  if (const ConstantInt *CI = dyn_cast<ConstantInt>(V)) {
    uint64_t C = CI->getZExtValue();
    Type *i32 = Type::getInt32Ty(V->getContext());
    LowHighPair LowHigh;
    LowHigh.Low = ConstantInt::get(i32, (uint32_t)C);
    LowHigh.High = ConstantInt::get(i32, (uint32_t)(C >> 32));
    return LowHigh;
  } else {
    Instruction *I = dyn_cast<Instruction>(V);
    // TODO assert(Splits.find(I) != Splits.end());
    if (Splits.find(I) == Splits.end()) { // debugging tool, for now FIXME remove
      Type *i32 = Type::getInt32Ty(V->getContext());
      LowHighPair LowHigh;
      LowHigh.Low = ConstantInt::get(i32, 13);
      LowHigh.High = ConstantInt::get(i32, 37);
      return LowHigh;
    }
    return Splits[I].LowHigh;
  }
}

void ExpandI64::finalizeInst(Instruction *I) {
  SplitInfo &Split = Splits[I];
  switch (I->getOpcode()) {
    case Instruction::SExt: break; // input was legal
    case Instruction::Store: {
      LowHighPair LowHigh = getLowHigh(I->getOperand(0));
      assert(LowHigh.Low && LowHigh.High);
      Split.ToFix[0]->setOperand(0, LowHigh.Low);
      Split.ToFix[1]->setOperand(0, LowHigh.High);
      break;
    }
  }
}

void ExpandI64::ensureFuncs() {
  if (Add != NULL) return;
  SmallVector<Type*, 4> BinaryArgTypes;
  Type *i32 = Type::getInt32Ty(TheModule->getContext());
  BinaryArgTypes.push_back(i32);
  BinaryArgTypes.push_back(i32);
  BinaryArgTypes.push_back(i32);
  BinaryArgTypes.push_back(i32);
  FunctionType *BinaryFunc = FunctionType::get(i32, BinaryArgTypes, false);

  Add = Function::Create(BinaryFunc, GlobalValue::ExternalLinkage,
                         "i64Add", TheModule);

  SmallVector<Type*, 0> GetHighArgTypes;
  FunctionType *GetHighFunc = FunctionType::get(i32, GetHighArgTypes, false);
  GetHigh = Function::Create(GetHighFunc, GlobalValue::ExternalLinkage,
                             "getHigh", TheModule);
}

bool ExpandI64::runOnModule(Module &M) {
  TheModule = &M;

  bool Changed = false;
  DataLayout DL(&M);
  // first pass - split
  for (Module::iterator Iter = M.begin(), E = M.end(); Iter != E; ) {
    Function *Func = Iter++;
    for (Function::iterator BB = Func->begin(), E = Func->end();
         BB != E;
         ++BB) {
      for (BasicBlock::iterator Iter = BB->begin(), E = BB->end();
           Iter != E; ) {
        Instruction *I = Iter++;
        Type *T = I->getType();
        if (T->isIntegerTy() && T->getIntegerBitWidth() == 64) {
          Changed = true;
          splitInst(I, DL);
          continue;
        }
        if (I->getNumOperands() >= 1) {
          T = I->getOperand(0)->getType();
          if (T->isIntegerTy() && T->getIntegerBitWidth() == 64) {
            Changed = true;
            splitInst(I, DL);
            continue;
          }
        }
      }
    }
  }
  // second pass pass - finalize and connect
  if (Changed) {
    // Finalize each element
    for (SplitsMap::iterator I = Splits.begin(); I != Splits.end(); I++) {
      finalizeInst(I->first);
    }

    // Remove original illegal values
    for (SplitsMap::iterator I = Splits.begin(); I != Splits.end(); I++) {
      I->first->eraseFromParent(); // XXX must be commented out during development, to allow partial legalization
    }
  }
  return Changed;
}

ModulePass *llvm::createExpandI64Pass() {
  return new ExpandI64();
}

