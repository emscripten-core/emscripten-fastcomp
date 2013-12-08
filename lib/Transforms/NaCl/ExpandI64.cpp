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

  // The tricky part in all this pass is that we legalize many instructions that interdepend on each
  // other. So we do one pass where we create the new legal instructions but leave the illegal ones
  // in place, then a second where we hook up the legal ones to the other legal ones, and only
  // then do we remove the illegal ones.
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

    Function *Add, *Sub, *Mul, *SDiv, *UDiv, *SRem, *URem, *LShr, *AShr, *GetHigh, *SetHigh;

    void ensureFuncs();

    Module *TheModule;

  public:
    static char ID;
    ExpandI64() : ModulePass(ID) {
      initializeExpandI64Pass(*PassRegistry::getPassRegistry());

      Add = Sub = Mul = SDiv = UDiv = SRem = URem = LShr = AShr = GetHigh = SetHigh = NULL;
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
  Type *i32P = i32->getPointerTo();
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
    case Instruction::ZExt: {
      Value *Input = I->getOperand(0);
      Type *T = Input->getType();
      Value *Low;
      if (T->getIntegerBitWidth() < 32) {
        Low = CopyDebug(new SExtInst(Input, i32, "", I), I);
      } else {
        Low = Input;
      }
      SplitInfo &Split = Splits[I];
      Split.LowHigh.Low = Low;
      Split.LowHigh.High = Zero;
      break;
    }
    case Instruction::Trunc: {
      assert(I->getType()->getIntegerBitWidth() == 32);
      Splits[I];
      break;
    }
    case Instruction::Load: {
      LoadInst *LI = dyn_cast<LoadInst>(I);

      Instruction *AI = CopyDebug(new PtrToIntInst(LI->getPointerOperand(), i32, "", I), I);
      Instruction *P4 = CopyDebug(BinaryOperator::Create(Instruction::Add, AI, ConstantInt::get(i32, 4), "", I), I);
      Instruction *LP = CopyDebug(new IntToPtrInst(AI, i32P, "", I), I);
      Instruction *HP = CopyDebug(new IntToPtrInst(P4, i32P, "", I), I);
      LoadInst *LL = new LoadInst(LP, "", I); CopyDebug(LL, I);
      LoadInst *LH = new LoadInst(HP, "", I); CopyDebug(LH, I);
      SplitInfo &Split = Splits[I];
      Split.LowHigh.Low = LL;
      Split.LowHigh.High = LH;

      LL->setAlignment(LI->getAlignment());
      LH->setAlignment(LI->getAlignment());
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
    case Instruction::Ret: {
      ensureFuncs();
      SmallVector<Value *, 1> Args;
      Args.push_back(Zero); // will be fixed 
      Instruction *Low = CopyDebug(CallInst::Create(SetHigh, Args, "", I), I);
      Instruction *High = CopyDebug(ReturnInst::Create(I->getContext(), Zero, I), I); // will be fixed
      SplitInfo &Split = Splits[I];
      Split.ToFix.push_back(Low);
      Split.ToFix.push_back(High);
      break;
    }
    case Instruction::Add:
    case Instruction::Sub:
    case Instruction::Mul:
    case Instruction::SDiv:
    case Instruction::UDiv:
    case Instruction::SRem:
    case Instruction::URem:
    case Instruction::LShr:
    case Instruction::AShr: {
      ensureFuncs();
      Function *F;
      switch (I->getOpcode()) {
        case Instruction::Add:  F = Add;  break;
        case Instruction::Sub:  F = Sub;  break;
        case Instruction::Mul:  F = Mul;  break;
        case Instruction::SDiv: F = SDiv; break;
        case Instruction::UDiv: F = UDiv; break;
        case Instruction::SRem: F = SRem; break;
        case Instruction::URem: F = URem; break;
        case Instruction::LShr: F = LShr; break;
        case Instruction::AShr: F = AShr; break;
        default: assert(0);
      }
      SmallVector<Value *, 4> Args;
      for (unsigned i = 0; i < 4; i++) Args.push_back(Zero); // will be fixed 
      Instruction *Low = CopyDebug(CallInst::Create(F, Args, "", I), I);
      Instruction *High = CopyDebug(CallInst::Create(GetHigh, "", I), I);
      SplitInfo &Split = Splits[I];
      Split.ToFix.push_back(Low);
      Split.LowHigh.Low = Low;
      Split.LowHigh.High = High;
      break;
    }
    case Instruction::ICmp: {
      Instruction *A, *B, *C = NULL, *D = NULL, *Final;
      ICmpInst *CE = dyn_cast<ICmpInst>(I);
      ICmpInst::Predicate Pred = CE->getPredicate();
      switch (Pred) {
        case ICmpInst::ICMP_EQ: {
          A = CopyDebug(new ICmpInst(I, ICmpInst::ICMP_EQ, Zero, Zero), I);
          B = CopyDebug(new ICmpInst(I, ICmpInst::ICMP_EQ, Zero, Zero), I);
          Final = CopyDebug(BinaryOperator::Create(Instruction::And, A, B, "", I), I);
          break;
        }
        case ICmpInst::ICMP_NE: {
          A = CopyDebug(new ICmpInst(I, ICmpInst::ICMP_NE, Zero, Zero), I);
          B = CopyDebug(new ICmpInst(I, ICmpInst::ICMP_NE, Zero, Zero), I);
          Final = CopyDebug(BinaryOperator::Create(Instruction::Or, A, B, "", I), I);
          break;
        }
        case ICmpInst::ICMP_ULT:
        case ICmpInst::ICMP_SLT:
        case ICmpInst::ICMP_UGT:
        case ICmpInst::ICMP_SGT:
        case ICmpInst::ICMP_ULE:
        case ICmpInst::ICMP_SLE:
        case ICmpInst::ICMP_UGE:
        case ICmpInst::ICMP_SGE: {
          A = CopyDebug(new ICmpInst(I, Pred, Zero, Zero), I);
          B = CopyDebug(new ICmpInst(I, ICmpInst::ICMP_EQ, Zero, Zero), I);
          C = CopyDebug(new ICmpInst(I, Pred, Zero, Zero), I);
          D = CopyDebug(BinaryOperator::Create(Instruction::And, B, C, "", I), I);
          Final = CopyDebug(BinaryOperator::Create(Instruction::Or, A, D, "", I), I);
          break;
        }
        default: assert(0);
      }
      SplitInfo &Split = Splits[I];
      Split.ToFix.push_back(A);
      Split.ToFix.push_back(B);
      Split.ToFix.push_back(C);
      // D is NULL or a logical operator, no need to fix it
      Split.ToFix.push_back(Final);
      break;
    }
    case Instruction::Select: {
      Value *Cond = I->getOperand(0);

      Instruction *L = CopyDebug(SelectInst::Create(Cond, Zero, Zero, "", I), I); // will be fixed
      Instruction *H = CopyDebug(SelectInst::Create(Cond, Zero, Zero, "", I), I); // will be fixed
      SplitInfo &Split = Splits[I];
      Split.ToFix.push_back(L); Split.LowHigh.Low  = L;
      Split.ToFix.push_back(H); Split.LowHigh.High = H;
      break;
    }
    case Instruction::PHI: {
      PHINode *P = dyn_cast<PHINode>(I);
      int Num = P->getNumIncomingValues();

      PHINode *L = PHINode::Create(i32, Num, "", I); CopyDebug(L, I);
      PHINode *H = PHINode::Create(i32, Num, "", I); CopyDebug(H, I);
      for (int i = 0; i < Num; i++) {
        L->addIncoming(Zero, P->getIncomingBlock(i)); // will be fixed
        H->addIncoming(Zero, P->getIncomingBlock(i)); // will be fixed
      }
      SplitInfo &Split = Splits[I];
      Split.ToFix.push_back(L); Split.LowHigh.Low  = L;
      Split.ToFix.push_back(H); Split.LowHigh.High = H;
      break;
    }
    default: {
      dumpIR(I);
      assert(0 && "some i64 thing we can't legalize yet");
    }
  }
}

LowHighPair ExpandI64::getLowHigh(Value *V) {
  if (const ConstantInt *CI = dyn_cast<ConstantInt>(V)) {
    uint64_t C = CI->getZExtValue();
    Type *i32 = Type::getInt32Ty(V->getContext());
    LowHighPair LowHigh;
    LowHigh.Low = ConstantInt::get(i32, (uint32_t)C);
    LowHigh.High = ConstantInt::get(i32, (uint32_t)(C >> 32));
    assert(LowHigh.Low && LowHigh.High);
    return LowHigh;
  } else {
    Instruction *I = dyn_cast<Instruction>(V);
    // TODO assert(Splits.find(I) != Splits.end());
    if (Splits.find(I) == Splits.end()) { // debugging tool, for now FIXME remove
      Type *i32 = Type::getInt32Ty(V->getContext());
      LowHighPair LowHigh;
      LowHigh.Low = ConstantInt::get(i32, 13);
      LowHigh.High = ConstantInt::get(i32, 37);
      assert(LowHigh.Low && LowHigh.High);
      return LowHigh;
    }
    return Splits[I].LowHigh;
  }
}

void ExpandI64::finalizeInst(Instruction *I) {
  SplitInfo &Split = Splits[I];
  switch (I->getOpcode()) {
    case Instruction::Load:
    case Instruction::SExt:
    case Instruction::ZExt: break; // input was legal
    case Instruction::Trunc: {
      assert(I->getType()->getIntegerBitWidth() == 32);
      LowHighPair LowHigh = getLowHigh(I->getOperand(0));
      I->replaceAllUsesWith(LowHigh.Low);
      break;
    }
    case Instruction::Store:
    case Instruction::Ret: {
      // generic fix of an instruction with one 64-bit input, and consisting of two legal instructions, for low and high
      LowHighPair LowHigh = getLowHigh(I->getOperand(0));
      Split.ToFix[0]->setOperand(0, LowHigh.Low);
      Split.ToFix[1]->setOperand(0, LowHigh.High);
      break;
    }
    case Instruction::Add:
    case Instruction::Sub:
    case Instruction::Mul:
    case Instruction::SDiv:
    case Instruction::UDiv:
    case Instruction::SRem:
    case Instruction::URem:
    case Instruction::LShr:
    case Instruction::AShr: {
      LowHighPair LeftLH = getLowHigh(I->getOperand(0));
      LowHighPair RightLH = getLowHigh(I->getOperand(1));
      Instruction *Call = Split.ToFix[0];
      Call->setOperand(0, LeftLH.Low);
      Call->setOperand(1, LeftLH.High);
      Call->setOperand(2, RightLH.Low);
      Call->setOperand(3, RightLH.High);
      break;
    }
    case Instruction::ICmp: {
      LowHighPair LeftLH = getLowHigh(I->getOperand(0));
      LowHighPair RightLH = getLowHigh(I->getOperand(1));
      Instruction *A = Split.ToFix[0];
      Instruction *B = Split.ToFix[1];
      Instruction *C = Split.ToFix[2];
      Instruction *Final = Split.ToFix[4];
      if (!C) { // EQ, NE
        A->setOperand(0, LeftLH.Low);  A->setOperand(1, RightLH.Low);
        B->setOperand(0, LeftLH.High); B->setOperand(1, RightLH.High);
      } else {
        A->setOperand(0, LeftLH.Low);  A->setOperand(1, RightLH.Low);
        B->setOperand(0, LeftLH.Low);  B->setOperand(1, RightLH.Low);
        C->setOperand(0, LeftLH.High); C->setOperand(1, RightLH.High);
      }
      I->replaceAllUsesWith(Final);
      break;
    }
    case Instruction::Select: {
      LowHighPair TrueLH = getLowHigh(I->getOperand(1));
      LowHighPair FalseLH = getLowHigh(I->getOperand(2));
      Instruction *L = Split.ToFix[0];
      Instruction *H = Split.ToFix[1];
      L->setOperand(1, TrueLH.Low);  L->setOperand(2, FalseLH.Low);
      H->setOperand(1, TrueLH.High); H->setOperand(2, FalseLH.High);
      break;
    }
    case Instruction::PHI: {
      PHINode *P = dyn_cast<PHINode>(I);
      int Num = P->getNumIncomingValues();
      PHINode *L = dyn_cast<PHINode>(Split.ToFix[0]);
      PHINode *H = dyn_cast<PHINode>(Split.ToFix[1]);
      for (int i = 0; i < Num; i++) {
        LowHighPair LH = getLowHigh(P->getIncomingValue(i));
        L->setIncomingValue(i, LH.Low);
        H->setIncomingValue(i, LH.High);
      }
      break;
    }
    default: assert(0);
  }
}

void ExpandI64::ensureFuncs() {
  if (Add != NULL) return;

  Type *i32 = Type::getInt32Ty(TheModule->getContext());

  SmallVector<Type*, 4> FourArgTypes;
  FourArgTypes.push_back(i32);
  FourArgTypes.push_back(i32);
  FourArgTypes.push_back(i32);
  FourArgTypes.push_back(i32);
  FunctionType *FourFunc = FunctionType::get(i32, FourArgTypes, false);

  Add = Function::Create(FourFunc, GlobalValue::ExternalLinkage,
                         "i64Add", TheModule);
  Sub = Function::Create(FourFunc, GlobalValue::ExternalLinkage,
                         "i64Subtract", TheModule);
  Mul = Function::Create(FourFunc, GlobalValue::ExternalLinkage,
                         "__muldi3", TheModule);
  SDiv = Function::Create(FourFunc, GlobalValue::ExternalLinkage,
                         "__divdi3", TheModule);
  UDiv = Function::Create(FourFunc, GlobalValue::ExternalLinkage,
                         "__udivdi3", TheModule);
  SRem = Function::Create(FourFunc, GlobalValue::ExternalLinkage,
                         "__remdi3", TheModule);
  URem = Function::Create(FourFunc, GlobalValue::ExternalLinkage,
                         "__uremdi3", TheModule);
  LShr = Function::Create(FourFunc, GlobalValue::ExternalLinkage,
                          "bitshift64Lshr", TheModule);
  AShr = Function::Create(FourFunc, GlobalValue::ExternalLinkage,
                          "bitshift64Ashr", TheModule);

  SmallVector<Type*, 0> GetHighArgTypes;
  FunctionType *GetHighFunc = FunctionType::get(i32, GetHighArgTypes, false);
  GetHigh = Function::Create(GetHighFunc, GlobalValue::ExternalLinkage,
                             "getHigh32", TheModule);

  Type *V = Type::getVoidTy(TheModule->getContext());

  SmallVector<Type*, 1> SetHighArgTypes;
  SetHighArgTypes.push_back(i32);
  FunctionType *SetHighFunc = FunctionType::get(V, SetHighArgTypes, false);
  SetHigh = Function::Create(SetHighFunc, GlobalValue::ExternalLinkage,
                             "setHigh32", TheModule);
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
        //dump("consider"); dumpIR(I);
        // FIXME: this could be optimized, we don't need all Num for all instructions
        int Num = I->getNumOperands();
        for (int i = -1; i < Num; i++) { // -1 is the type of I itself
          Type *T = i == -1 ? I->getType() : I->getOperand(i)->getType();
          if (T->isIntegerTy() && T->getIntegerBitWidth() == 64) {
            Changed = true;
            //dump("split"); dumpIR(I);
            splitInst(I, DL);
            break;
          }
        }
      }
    }
  }
  // second pass pass - finalize and connect
  if (Changed) {
    // Finalize each element
    for (SplitsMap::iterator I = Splits.begin(); I != Splits.end(); I++) {
      //dump("finalize"); dumpIR(I->first);
      finalizeInst(I->first);
    }

    // Remove original illegal values
    if (!getenv("I64DEV")) { // XXX during development
      for (SplitsMap::iterator I = Splits.begin(); I != Splits.end(); I++) {
        //dump("delete"); dumpIR(I->first);
        I->first->eraseFromParent();
      }
    }
  }
  return Changed;
}

ModulePass *llvm::createExpandI64Pass() {
  return new ExpandI64();
}

