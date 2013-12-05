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
#define dumpIR(value) { \
  std::string temp; \
  raw_string_ostream stream(temp); \
  stream << *(value); \
  dumpv("%s\n", temp.c_str()); \
}

using namespace llvm;

namespace {
  // This is a ModulePass because the pass recreates functions in
  // order to expand i64 arguments to pairs of i32s.
  class ExpandI64 : public ModulePass {
    typedef std::vector<Value*> SplitParts;

    std::map<Value*, SplitParts> Splits; // old i64 value to new insts

    // splits a 64-bit instruction into 32-bit chunks. We do
    // not yet have the values yet, as they depend on other
    // splits, so store the parts in Splits, for FinalizeInst.
    void SplitInst(Instruction *I);

    void FinalizeInst(Instruction *I);

  public:
    static char ID;
    ExpandI64() : ModulePass(ID) {
      initializeExpandI64Pass(*PassRegistry::getPassRegistry());
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

void ExpandI64::SplitInst(Instruction *I) {
dumpv("si1 %d %d", I->getOpcode(), Instruction::SExt);
  switch (I->getOpcode()) {
    case Instruction::SExt: {
      Value *Input = I->getOperand(0);
      Type *T = Input->getType();
      assert(T->getIntegerBitWidth() == 32); // FIXME: sext from smaller
      // x = sext i32 to i64  ==>
      // xl = x ; test = x < 0 ; xh = test ? -1 : 0
      Value *Zero  = Constant::getNullValue(T);
      Value *Ones  = Constant::getAllOnesValue(T);

      Value *Check = CopyDebug(new ICmpInst(I, ICmpInst::ICMP_SLE, Input, Zero), I);
      Value *High  = CopyDebug(SelectInst::Create(Check, Ones, Zero, "", I), I);
      SplitParts &Split = Splits[I];
      Split.push_back(Check);
      Split.push_back(High);

      break;
    }
    //default: // FIXME: abort if we hit something we don't support
  }
}

bool ExpandI64::runOnModule(Module &M) {
  bool Changed = false;
  DataLayout DL(&M);
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
          SplitInst(I);
        }
      }
    }
  }
  return Changed;
}

ModulePass *llvm::createExpandI64Pass() {
  return new ExpandI64();
}

