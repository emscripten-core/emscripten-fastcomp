//===- ExpandI64.cpp - Expand out variable argument function calls-----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===------------------------------------------------------------------===//
//
// This pass expands and lowers all operations on integers larger than i64
// into 32-bit operations that can be handled by JS in a natural way.
//
// 64-bit variables become pairs of 2 32-bit variables, for the low and
// high 32 bit chunks. This happens for both registers and function
// arguments. Function return values become a return of the low 32 bits
// and a store of the high 32-bits in tempRet0, a global helper variable.
// Larger values become more chunks of 32 bits. Currently we require that
// types be a multiple of 32 bits.
//
// Many operations then become simple pairs of operations, for example
// bitwise AND becomes and AND of each 32-bit chunk. More complex operations
// like addition are lowered into calls into library support code in
// Emscripten (i64Add for example).
//
//===------------------------------------------------------------------===//

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/NaCl.h"
#include <map>

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

  typedef SmallVector<Value*, 2> ChunksVec;

  typedef std::vector<Instruction*> SplitInstrs;

  // The tricky part in all this pass is that we legalize many instructions that interdepend on each
  // other. So we do one pass where we create the new legal instructions but leave the illegal ones
  // in place, then a second where we hook up the legal ones to the other legal ones, and only
  // then do we remove the illegal ones.
  struct SplitInfo {
    SplitInstrs ToFix; // new instrs, which we fix up later with proper legalized input (if they received illegal input)
    ChunksVec Chunks;  // 32-bit chunks of the legalized output, if the output was illegal
  };

  typedef std::map<Instruction*, SplitInfo> SplitsMap;
  typedef std::map<Value*, ChunksVec> ArgsMap;

  // This is a ModulePass because the pass recreates functions in
  // order to expand i64 arguments to pairs of i32s.
  class ExpandI64 : public ModulePass {
    SplitsMap Splits; // old illegal value to new insts
    ArgsMap SplitArgs; // old illegal function arguments, to split parts

    // If the function has an illegal return or argument, create a legal version
    void ensureLegalFunc(Function *F);

    // If a function is illegal, remove it
    void removeIllegalFunc(Function *F);

    // splits an illegal instruction into 32-bit chunks. We do
    // not yet have the values yet, as they depend on other
    // splits, so store the parts in Splits, for FinalizeInst.
    void splitInst(Instruction *I, DataLayout& DL);

    // For an illegal value, returns the split out chunks
    // representing the low and high parts, that splitInst
    // generated.
    // The value can also be a constant, in which case we just
    // split it, or a function argument, in which case we
    // map to the proper legalized new arguments
    ChunksVec getChunks(Value *V);

    void finalizeInst(Instruction *I);

    Function *Add, *Sub, *Mul, *SDiv, *UDiv, *SRem, *URem, *LShr, *AShr, *Shl, *GetHigh, *SetHigh, *FtoILow, *FtoIHigh, *DtoILow, *DtoIHigh, *SItoF, *UItoF, *SItoD, *UItoD, *BItoD, *BDtoILow, *BDtoIHigh;

    void ensureFuncs();

    Module *TheModule;

  public:
    static char ID;
    ExpandI64() : ModulePass(ID) {
      initializeExpandI64Pass(*PassRegistry::getPassRegistry());

      Add = Sub = Mul = SDiv = UDiv = SRem = URem = LShr = AShr = Shl = GetHigh = SetHigh = NULL;
    }

    virtual bool runOnModule(Module &M);
  };
}

char ExpandI64::ID = 0;
INITIALIZE_PASS(ExpandI64, "expand-illegal-ints",
                "Expand and lower illegal >i32 operations into 32-bit chunks",
                false, false)

// Utilities

static bool isIllegal(Type *T) {
  return T->isIntegerTy() && T->getIntegerBitWidth() > 32;
}

static FunctionType *getLegalizedFunctionType(FunctionType *FT) {
  SmallVector<Type*, 0> ArgTypes; // XXX
  int Num = FT->getNumParams();
  for (int i = 0; i < Num; i++) {
    Type *T = FT->getParamType(i);
    if (!isIllegal(T)) {
      ArgTypes.push_back(T);
    } else {
      Type *i32 = Type::getInt32Ty(FT->getContext());
      ArgTypes.push_back(i32);
      ArgTypes.push_back(i32);
    }
  }
  Type *RT = FT->getReturnType();
  Type *NewRT;
  if (!isIllegal(RT)) {
    NewRT = RT;
  } else {
    NewRT = Type::getInt32Ty(FT->getContext());
  }
  return FunctionType::get(NewRT, ArgTypes, false);
}

// Implementation of ExpandI64

bool okToRemainIllegal(Function *F) {
  const char *Name = F->getName().str().c_str();
  if (strcmp(Name, "llvm.dbg.value") == 0) return true;
  return false;
}

int getNumChunks(Type *T) {
  IntegerType *IT = cast<IntegerType>(T);
  int Num = IT->getBitWidth();
  if (Num%32 != 0) assert(0);
  return Num/32;
}

void ExpandI64::ensureLegalFunc(Function *F) {
  if (okToRemainIllegal(F)) return;

  FunctionType *FT = F->getFunctionType();
  int Num = FT->getNumParams();

  // XXX Emscripten TODO: Move this fix to PNacl upstream.
  // Allocate small names on stack, large ones on heap.
  // This is because on VS2010, arrays on the stack must
  // be compile-time constant sized. (no C99 dynamic-length array support)
  const int StackSize = 256;
  char StackArray[StackSize];
  char *AllocArray = 0;

  for (int i = -1; i < Num; i++) {
    Type *T = i == -1 ? FT->getReturnType() : FT->getParamType(i);
    if (isIllegal(T)) {
      Function *NF = RecreateFunction(F, getLegalizedFunctionType(FT));
      std::string Name = NF->getName();
      if (strncmp(Name.c_str(), "llvm.", 5) == 0) {
        // this is an intrinsic, and we are changing its signature, which will annoy LLVM, so rename
        const size_t len = Name.size()+1;
        char *NewName;
        if (len > StackSize)
          NewName = AllocArray = new char[len];
        else
          NewName = StackArray;
        const char *CName = Name.c_str();
        for (unsigned i = 0; i < len; i++) {
          NewName[i] = CName[i] != '.' ? CName[i] : '_';
        }
        NF->setName(NewName);
        delete[] AllocArray;
        AllocArray = 0;
      }
      // Move and update arguments
      for (Function::arg_iterator Arg = F->arg_begin(), E = F->arg_end(), NewArg = NF->arg_begin();
           Arg != E; ++Arg) {
        if (Arg->getType() == NewArg->getType()) {
          NewArg->takeName(Arg);
          Arg->replaceAllUsesWith(NewArg);
          NewArg++;
        } else {
          // This was legalized
          ChunksVec &Chunks = SplitArgs[&*Arg];
          int Num = getNumChunks(Arg->getType());
          assert(Num == 2);
          for (int i = 0; i < Num; i++) {
            Chunks.push_back(&*NewArg);
            if (NewArg->hasName()) Chunks[i]->setName(NewArg->getName() + "$" + utostr(i));
            NewArg++;
          }
        }
      }
      break;
    }
  }
}

void ExpandI64::removeIllegalFunc(Function *F) {
  if (okToRemainIllegal(F)) return;

  FunctionType *FT = F->getFunctionType();
  int Num = FT->getNumParams();
  for (int i = -1; i < Num; i++) {
    Type *T = i == -1 ? FT->getReturnType() : FT->getParamType(i);
    if (isIllegal(T)) {
      F->eraseFromParent();
      break;
    }
  }
}

void ExpandI64::splitInst(Instruction *I, DataLayout& DL) {
  Type *i32 = Type::getInt32Ty(I->getContext());
  Type *i32P = i32->getPointerTo();
  Type *i64 = Type::getInt64Ty(I->getContext());
  Value *Zero  = Constant::getNullValue(i32);
  Value *Ones  = Constant::getAllOnesValue(i32);

  SplitInfo &Split = Splits[I];

  switch (I->getOpcode()) {
    case Instruction::SExt: {
      Value *Input = I->getOperand(0);
      Type *T = Input->getType();
      Value *Low;
      if (T->getIntegerBitWidth() < 32) {
        Low = CopyDebug(new SExtInst(Input, i32, "", I), I);
      } else {
        assert(T->getIntegerBitWidth() == 32);
        Low = CopyDebug(BinaryOperator::Create(Instruction::Or, Input, Zero, "", I), I); // copy the input, hackishly XXX
      }
      Split.Chunks.push_back(Low);
      Instruction *Check = CopyDebug(new ICmpInst(I, ICmpInst::ICMP_SLT, Low, Zero), I);
      int Num = getNumChunks(I->getType());
      for (int i = 1; i < Num; i++) {
        Instruction *High = CopyDebug(SelectInst::Create(Check, Ones, Zero, "", I), I);
        Split.Chunks.push_back(High);
      }
      break;
    }
    case Instruction::ZExt: {
      Value *Input = I->getOperand(0);
      Type *T = Input->getType();
      Value *Low;
      if (T->getIntegerBitWidth() < 32) {
        Low = CopyDebug(new ZExtInst(Input, i32, "", I), I);
      } else {
        assert(T->getIntegerBitWidth() == 32);
        Low = CopyDebug(BinaryOperator::Create(Instruction::Or, Input, Zero, "", I), I); // copy the input, hackishly XXX
      }
      Split.Chunks.push_back(Low);
      int Num = getNumChunks(I->getType());
      for (int i = 1; i < Num; i++) {
        Split.Chunks.push_back(Zero);
      }
      break;
    }
    case Instruction::Trunc: {
      if (I->getType()->getIntegerBitWidth() < 32) {
        // we need to add a trunc of the low 32 bits
        Instruction *L = CopyDebug(new TruncInst(Zero, I->getType(), "", I), I);
        Split.ToFix.push_back(L);
      } else {
        assert(I->getType()->getIntegerBitWidth() == 32);
      }
      break;
    }
    case Instruction::Load: {
      LoadInst *LI = dyn_cast<LoadInst>(I);
      Instruction *AI = CopyDebug(new PtrToIntInst(LI->getPointerOperand(), i32, "", I), I);
      int Num = getNumChunks(I->getType());
      for (int i = 0; i < Num; i++) {
        Instruction *Add = i == 0 ? AI : CopyDebug(BinaryOperator::Create(Instruction::Add, AI, ConstantInt::get(i32, 4*i), "", I), I);
        Instruction *Ptr = CopyDebug(new IntToPtrInst(Add, i32P, "", I), I);
        LoadInst *Chunk = new LoadInst(Ptr, "", I); CopyDebug(Chunk, I);
        Chunk->setAlignment(std::min(4U, LI->getAlignment()));
        Split.Chunks.push_back(Chunk);
      }
      break;
    }
    case Instruction::Store: {
      StoreInst *SI = dyn_cast<StoreInst>(I);
      Instruction *AI = CopyDebug(new PtrToIntInst(SI->getPointerOperand(), i32, "", I), I);
      int Num = getNumChunks(SI->getValueOperand()->getType());
      for (int i = 0; i < Num; i++) {
        Instruction *Add = i == 0 ? AI : CopyDebug(BinaryOperator::Create(Instruction::Add, AI, ConstantInt::get(i32, 4*i), "", I), I);
        Instruction *Ptr = CopyDebug(new IntToPtrInst(Add, i32P, "", I), I);
        StoreInst *Chunk = new StoreInst(Zero, Ptr, "", I); CopyDebug(Chunk, I);
        Chunk->setAlignment(std::min(4U, SI->getAlignment()));
        Split.ToFix.push_back(Chunk);
      }
      break;
    }
    case Instruction::Ret: {
      assert(I->getOperand(0)->getType() == i64);
      ensureFuncs();
      SmallVector<Value *, 1> Args;
      Args.push_back(Zero); // will be fixed 
      Instruction *High = CopyDebug(CallInst::Create(SetHigh, Args, "", I), I);
      Instruction *Low  = CopyDebug(ReturnInst::Create(I->getContext(), Zero, I), I); // will be fixed
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
    case Instruction::AShr:
    case Instruction::Shl: {
      assert(I->getOperand(0)->getType() == i64); // FIXME; we need to support shifts on >64 bits
      ensureFuncs();
      Value *Low = NULL, *High = NULL;
      Function *F = NULL;
      switch (I->getOpcode()) {
        case Instruction::Add:  F = Add;  break;
        case Instruction::Sub:  F = Sub;  break;
        case Instruction::Mul:  F = Mul;  break;
        case Instruction::SDiv: F = SDiv; break;
        case Instruction::UDiv: F = UDiv; break;
        case Instruction::SRem: F = SRem; break;
        case Instruction::URem: F = URem; break;
        case Instruction::LShr: {
          if (ConstantInt *CI = dyn_cast<ConstantInt>(I->getOperand(1))) {
            unsigned Shifts = CI->getZExtValue();
            if (Shifts == 32) {
              Low = CopyDebug(BinaryOperator::Create(Instruction::Or, Zero, Zero, "", I), I); // copy hackishly XXX TODO: eliminate x|0 to x in post-pass
              High = Zero;
              break;
            }
          }
          F = LShr;
          break;
        }
        case Instruction::AShr: F = AShr; break;
        case Instruction::Shl: {
          if (ConstantInt *CI = dyn_cast<ConstantInt>(I->getOperand(1))) {
            unsigned Shifts = CI->getZExtValue();
            if (Shifts == 32) {
              Low = Zero;
              High = CopyDebug(BinaryOperator::Create(Instruction::Or, Zero, Zero, "", I), I); // copy hackishly XXX TODO: eliminate x|0 to x in post-pass
              break;
            }
          }
          F = Shl;
          break;
        }
        default: assert(0);
      }
      if (F) {
        // use a library call, no special optimization was found
        SmallVector<Value *, 4> Args;
        for (unsigned i = 0; i < 4; i++) Args.push_back(Zero); // will be fixed 
        Low = CopyDebug(CallInst::Create(F, Args, "", I), I);
        High = CopyDebug(CallInst::Create(GetHigh, "", I), I);
      }
      Split.Chunks.push_back(Low);
      Split.Chunks.push_back(High);
      break;
    }
    case Instruction::ICmp: {
      assert(I->getOperand(0)->getType() == i64); // FIXME
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
          ICmpInst::Predicate StrictPred = Pred;
          ICmpInst::Predicate UnsignedPred = Pred;
          switch (Pred) {
            case ICmpInst::ICMP_ULE: StrictPred = ICmpInst::ICMP_ULT; break;
            case ICmpInst::ICMP_UGE: StrictPred = ICmpInst::ICMP_UGT; break;
            case ICmpInst::ICMP_SLE: StrictPred = ICmpInst::ICMP_SLT; UnsignedPred = ICmpInst::ICMP_ULE; break;
            case ICmpInst::ICMP_SGE: StrictPred = ICmpInst::ICMP_SGT; UnsignedPred = ICmpInst::ICMP_UGE; break;
            case ICmpInst::ICMP_SLT:                                  UnsignedPred = ICmpInst::ICMP_ULT; break;
            case ICmpInst::ICMP_SGT:                                  UnsignedPred = ICmpInst::ICMP_UGT; break;
            case ICmpInst::ICMP_ULT: break;
            case ICmpInst::ICMP_UGT: break;
            default: assert(0);
          }
          A = CopyDebug(new ICmpInst(I, StrictPred, Zero, Zero), I);
          B = CopyDebug(new ICmpInst(I, ICmpInst::ICMP_EQ, Zero, Zero), I);
          C = CopyDebug(new ICmpInst(I, UnsignedPred, Zero, Zero), I);
          D = CopyDebug(BinaryOperator::Create(Instruction::And, B, C, "", I), I);
          Final = CopyDebug(BinaryOperator::Create(Instruction::Or, A, D, "", I), I);
          break;
        }
        default: assert(0);
      }
      Split.ToFix.push_back(A);
      Split.ToFix.push_back(B);
      Split.ToFix.push_back(C);
      // D is NULL or a logical operator, no need to fix it
      Split.ToFix.push_back(Final);
      break;
    }
    case Instruction::Select: {
      assert(I->getType() == i64);
      Value *Cond = I->getOperand(0);
      Instruction *L = CopyDebug(SelectInst::Create(Cond, Zero, Zero, "", I), I); // will be fixed
      Instruction *H = CopyDebug(SelectInst::Create(Cond, Zero, Zero, "", I), I); // will be fixed
      Split.Chunks.push_back(L);
      Split.Chunks.push_back(H);
      break;
    }
    case Instruction::PHI: {
      assert(I->getOperand(0)->getType() == i64);
      PHINode *P = dyn_cast<PHINode>(I);
      int Num = P->getNumIncomingValues();
      PHINode *L = PHINode::Create(i32, Num, "", I); CopyDebug(L, I);
      PHINode *H = PHINode::Create(i32, Num, "", I); CopyDebug(H, I);
      for (int i = 0; i < Num; i++) {
        L->addIncoming(Zero, P->getIncomingBlock(i)); // will be fixed
        H->addIncoming(Zero, P->getIncomingBlock(i)); // will be fixed
      }
      Split.Chunks.push_back(L);
      Split.Chunks.push_back(H);
      break;
    }
    case Instruction::And:
    case Instruction::Or:
    case Instruction::Xor: {
      assert(I->getOperand(0)->getType() == i64);
      Instruction::BinaryOps Op;
      switch (I->getOpcode()) { // XXX why does llvm make us do this?
        case Instruction::And: Op = Instruction::And; break;
        case Instruction::Or:  Op = Instruction::Or;  break;
        case Instruction::Xor: Op = Instruction::Xor; break;
      }
      Instruction *L = CopyDebug(BinaryOperator::Create(Op, Zero, Zero, "", I), I);
      Instruction *H = CopyDebug(BinaryOperator::Create(Op, Zero, Zero, "", I), I);
      Split.Chunks.push_back(L);
      Split.Chunks.push_back(H);
      break;
    }
    case Instruction::Call: {
      CallInst *CI = dyn_cast<CallInst>(I);
      Function *F = CI->getCalledFunction();
      if (F) {
        assert(okToRemainIllegal(F));
        break;
      }
      Value *CV = CI->getCalledValue();
      FunctionType *OFT = NULL;
      if (ConstantExpr *CE = dyn_cast<ConstantExpr>(CV)) {
        assert(CE);
        assert(CE->getOpcode() == Instruction::BitCast);
        OFT = cast<FunctionType>(cast<PointerType>(CE->getType())->getElementType());
        CV = CE->getOperand(0); // we are legalizing the arguments now, so no need to bitcast any more
      } else {
        // this is a function pointer call
        OFT = cast<FunctionType>(cast<PointerType>(CV->getType())->getElementType());
        // we need to add a bitcast
        CV = new BitCastInst(CV, getLegalizedFunctionType(OFT)->getPointerTo(), "", I);
      }
      // create a call with space for legal args
      SmallVector<Value *, 0> Args; // XXX
      int Num = OFT->getNumParams();
      for (int i = 0; i < Num; i++) {
        Type *T = OFT->getParamType(i);
        if (!isIllegal(T)) {
          Args.push_back(CI->getArgOperand(i));
        } else {
          assert(T == i64);
          Args.push_back(Zero); // will be fixed
          Args.push_back(Zero);
        }
      }
      Instruction *L = CopyDebug(CallInst::Create(CV, Args, "", I), I);
      Instruction *H = NULL;
      // legalize return value as well, if necessary
      if (isIllegal(I->getType())) {
        assert(I->getType() == i64);
        ensureFuncs();
        H = CopyDebug(CallInst::Create(GetHigh, "", I), I);
      }
      Split.Chunks.push_back(L);
      Split.Chunks.push_back(H);
      break;
    }
    case Instruction::FPToUI:
    case Instruction::FPToSI: {
      assert(I->getType() == i64);
      ensureFuncs();
      SmallVector<Value *, 1> Args;
      Value *Input = I->getOperand(0);
      Args.push_back(Input);
      Instruction *L, *H;
      if (Input->getType()->isFloatTy()) {
        L = CopyDebug(CallInst::Create(FtoILow, Args, "", I), I);
        H = CopyDebug(CallInst::Create(FtoIHigh, Args, "", I), I);
      } else {
        L = CopyDebug(CallInst::Create(DtoILow, Args, "", I), I);
        H = CopyDebug(CallInst::Create(DtoIHigh, Args, "", I), I);
      }
      Split.Chunks.push_back(L);
      Split.Chunks.push_back(H);
      break;
    }
    case Instruction::BitCast: {
      if (I->getType() == Type::getDoubleTy(TheModule->getContext())) {
        // fall through to itofp
      } else {
        // double to i64
        assert(I->getType() == i64);
        ensureFuncs();
        SmallVector<Value *, 1> Args;
        Args.push_back(I->getOperand(0));
        Instruction *L = CopyDebug(CallInst::Create(BDtoILow, Args, "", I), I);
        Instruction *H = CopyDebug(CallInst::Create(BDtoIHigh, Args, "", I), I);
        Split.Chunks.push_back(L);
        Split.Chunks.push_back(H);
        break;
      }
    }
    case Instruction::SIToFP:
    case Instruction::UIToFP: {
      assert(I->getOperand(0)->getType() == i64);
      ensureFuncs();
      SmallVector<Value *, 2> Args;
      Args.push_back(Zero);
      Args.push_back(Zero);
      Function *F;
      switch (I->getOpcode()) {
        case Instruction::SIToFP: F = I->getType() == Type::getDoubleTy(TheModule->getContext()) ? SItoD : SItoF; break;
        case Instruction::UIToFP: F = I->getType() == Type::getDoubleTy(TheModule->getContext()) ? UItoD : UItoF; break;
        case Instruction::BitCast: {
          assert(I->getType() == Type::getDoubleTy(TheModule->getContext()));
          F = BItoD;
          break;
        }
        default: assert(0);
      }
      Instruction *D = CopyDebug(CallInst::Create(F, Args, "", I), I);
      Split.ToFix.push_back(D);
      break;
    }
    case Instruction::Switch: {
      assert(I->getOperand(0)->getType() == i64);
      // do a switch on the lower 32 bits, into a different basic block for each target, then do a branch in each of those on the high 32 bits
      SwitchInst* SI = cast<SwitchInst>(I);
      BasicBlock *DD = SI->getDefaultDest();
      BasicBlock *SwitchBB = I->getParent();
      Function *F = SwitchBB->getParent();

      unsigned NumItems = 0;
      for (SwitchInst::CaseIt i = SI->case_begin(), e = SI->case_end(); i != e; ++i) {
        NumItems += i.getCaseValueEx().getNumItems();
      }
      SwitchInst *LowSI = SwitchInst::Create(Zero, DD, NumItems, I); // same default destination: if lower bits do not match, go straight to default
      CopyDebug(LowSI, I);
      Split.ToFix.push_back(LowSI);

      typedef std::pair<uint32_t, BasicBlock*> Pair;
      typedef std::vector<Pair> Vec; // vector of pairs of high 32 bits, basic block
      typedef std::map<uint32_t, Vec> Map; // maps low 32 bits to their Vec info
      Map Groups;                          // (as two 64-bit values in the switch may share their lower bits)

      for (SwitchInst::CaseIt i = SI->case_begin(), e = SI->case_end(); i != e; ++i) {
        BasicBlock *BB = i.getCaseSuccessor();
        const IntegersSubset CaseVal = i.getCaseValueEx();
        assert(CaseVal.isSingleNumbersOnly());
        for (unsigned Index = 0; Index < CaseVal.getNumItems(); Index++) {
          uint64_t Bits = CaseVal.getSingleNumber(Index).toConstantInt()->getZExtValue();
          uint32_t LowBits = (uint32_t)Bits;
          uint32_t HighBits = (uint32_t)(Bits >> 32);
          Vec& V = Groups[LowBits];
          V.push_back(Pair(HighBits, BB));
        }
      }

      unsigned Counter = 0;
      BasicBlock *InsertPoint = SwitchBB;

      for (Map::iterator GI = Groups.begin(); GI != Groups.end(); GI++) {
        uint32_t LowBits = GI->first;
        Vec &V = GI->second;

        BasicBlock *NewBB = BasicBlock::Create(F->getContext(), "switch64_" + utostr(Counter++), F);
        NewBB->moveAfter(InsertPoint);
        InsertPoint = NewBB;
        LowSI->addCase(cast<ConstantInt>(ConstantInt::get(i32, LowBits)), NewBB);

        /*if (V.size() == 1) {
          // just one option, create a branch
          Instruction *CheckHigh = CopyDebug(new ICmpInst(*NewBB, ICmpInst::ICMP_EQ, Zero, ConstantInt::get(i32, V[0]->first)), I);
          Split.ToFix.push_back(CheckHigh);
          CopyDebug(BranchInst::Create(V[0]->second, DD, CheckHigh, NewBB), I);
        } else {*/

        // multiple options, create a switch - we could also optimize and make an icmp/branch if just one, as in commented code above
        SwitchInst *HighSI = SwitchInst::Create(Zero, DD, V.size(), NewBB); // same default destination: if lower bits do not match, go straight to default
        Split.ToFix.push_back(HighSI);
        for (unsigned i = 0; i < V.size(); i++) {
          BasicBlock *BB = V[i].second;
          HighSI->addCase(cast<ConstantInt>(ConstantInt::get(i32, V[i].first)), BB);
          // fix phis, we used to go SwitchBB->BB, but now go SwitchBB->NewBB->BB, so we look like we arrived from NewBB. Replace the phi from the
          // now unneeded SwitchBB to the new BB
          for (BasicBlock::iterator I = BB->begin(); I != BB->end(); ++I) {
            PHINode *Phi = dyn_cast<PHINode>(I);
            if (!Phi) break;
            Phi->setIncomingBlock(Phi->getBasicBlockIndex(SwitchBB), NewBB);
          }
        }

        // We used to go SwitchBB->DD, but now go SwitchBB->NewBB->DD, fix that like with BB above. However here we do not replace,
        // as the switch BB is still possible to arrive from - we can arrive at the default if either the lower bits were wrong (we
        // arrive from the switchBB) or from the NewBB if the high bits were wrong.
        for (BasicBlock::iterator I = DD->begin(); I != DD->end(); ++I) {
          PHINode *Phi = dyn_cast<PHINode>(I);
          if (!Phi) break;
          Phi->addIncoming(Phi->getIncomingValue(Phi->getBasicBlockIndex(SwitchBB)), NewBB);
          // XXX note that we add a new i64 thing here. now the phi was already an i64 operation, and is being legalized anyhow, but we only notice the original inputs!
          //     we seem to be safe for now due to order of operation (phis show up after switches, but FIXME
        }
      }
      break;
    }
    default: {
      dumpIR(I);
      assert(0 && "some i64 thing we can't legalize yet");
    }
  }
}

ChunksVec ExpandI64::getChunks(Value *V) {
  Type *i32 = Type::getInt32Ty(V->getContext());
  if (const ConstantInt *CI = dyn_cast<ConstantInt>(V)) {
    uint64_t C = CI->getZExtValue();
    ChunksVec Chunks;
    Chunks.push_back(ConstantInt::get(i32, (uint32_t)C));
    Chunks.push_back(ConstantInt::get(i32, (uint32_t)(C >> 32)));
    return Chunks;
  } else if (Instruction *I = dyn_cast<Instruction>(V)) {
    assert(Splits.find(I) != Splits.end());
    return Splits[I].Chunks;
  } else if (isa<UndefValue>(V)) {
    ChunksVec Chunks;
    Chunks.push_back(ConstantInt::get(i32, 0));
    Chunks.push_back(ConstantInt::get(i32, 0));
    return Chunks;
  } else {
    assert(SplitArgs.find(V) != SplitArgs.end());
    return SplitArgs[V];
  }
}

void ExpandI64::finalizeInst(Instruction *I) {
  SplitInfo &Split = Splits[I];
  switch (I->getOpcode()) {
    case Instruction::Load:
    case Instruction::SExt:
    case Instruction::ZExt:
    case Instruction::FPToUI:
    case Instruction::FPToSI: {
      break; // input was legal
    }
    case Instruction::Trunc: {
      ChunksVec Chunks = getChunks(I->getOperand(0));
      if (I->getType()->getIntegerBitWidth() == 32) {
        I->replaceAllUsesWith(Chunks[0]); // just use the lower 32 bits and you're set
      } else {
        assert(I->getType()->getIntegerBitWidth() < 32);
        Instruction *L = Split.ToFix[0];
        L->setOperand(0, Chunks[0]);
        I->replaceAllUsesWith(L);
      }
      break;
    }
    case Instruction::Store:
    case Instruction::Ret: {
      // generic fix of an instruction with one 64-bit input, and consisting of two legal instructions, for low and high
      ChunksVec Chunks = getChunks(I->getOperand(0));
      Split.ToFix[0]->setOperand(0, Chunks[0]);
      Split.ToFix[1]->setOperand(0, Chunks[1]);
      break;
    }
    case Instruction::BitCast: {
      if (I->getType() == Type::getDoubleTy(TheModule->getContext())) {
        // fall through to itofp
      } else {
        break; // input was legal
      }
    }
    case Instruction::SIToFP:
    case Instruction::UIToFP: {
      // generic fix of an instruction with one 64-bit input, and a legal output
      ChunksVec Chunks = getChunks(I->getOperand(0));
      Instruction *D = Split.ToFix[0];
      D->setOperand(0, Chunks[0]);
      D->setOperand(1, Chunks[1]);
      I->replaceAllUsesWith(D);
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
    case Instruction::AShr:
    case Instruction::Shl: {
      ChunksVec Left = getChunks(I->getOperand(0));
      ChunksVec Right = getChunks(I->getOperand(1));
      CallInst *Call = dyn_cast<CallInst>(Split.Chunks[0]);
      if (Call) {
        Call->setOperand(0, Left[0]);
        Call->setOperand(1, Left[1]);
        Call->setOperand(2, Right[0]);
        Call->setOperand(3, Right[1]);
      } else {
        // optimized case, 32-bit shifts
        switch (I->getOpcode()) {
          case Instruction::LShr: {
            cast<Instruction>(Split.Chunks[0])->setOperand(0, Left[1]);
            break;
          }
          case Instruction::Shl: {
            cast<Instruction>(Split.Chunks[1])->setOperand(0, Left[0]);
            break;
          }
          default: assert(0);
        }
      }
      break;
    }
    case Instruction::ICmp: {
      ChunksVec Left = getChunks(I->getOperand(0));
      ChunksVec Right = getChunks(I->getOperand(1));
      Instruction *A = Split.ToFix[0];
      Instruction *B = Split.ToFix[1];
      Instruction *C = Split.ToFix[2];
      Instruction *Final = Split.ToFix[3];
      if (!C) { // EQ, NE
        A->setOperand(0, Left[0]);  A->setOperand(1, Right[0]);
        B->setOperand(0, Left[1]); B->setOperand(1, Right[1]);
      } else {
        A->setOperand(0, Left[1]);  A->setOperand(1, Right[1]);
        B->setOperand(0, Left[1]);  B->setOperand(1, Right[1]);
        C->setOperand(0, Left[0]); C->setOperand(1, Right[0]);
      }
      I->replaceAllUsesWith(Final);
      break;
    }
    case Instruction::Select: {
      ChunksVec True = getChunks(I->getOperand(1));
      ChunksVec False = getChunks(I->getOperand(2));
      Instruction *L = cast<Instruction>(Split.Chunks[0]);
      Instruction *H = cast<Instruction>(Split.Chunks[1]);
      L->setOperand(1, True[0]); L->setOperand(2, False[0]);
      H->setOperand(1, True[1]); H->setOperand(2, False[1]);
      break;
    }
    case Instruction::PHI: {
      PHINode *P = dyn_cast<PHINode>(I);
      int Num = P->getNumIncomingValues();
      PHINode *L = dyn_cast<PHINode>(Split.Chunks[0]);
      PHINode *H = dyn_cast<PHINode>(Split.Chunks[1]);
      for (int i = 0; i < Num; i++) {
        ChunksVec Chunks = getChunks(P->getIncomingValue(i));
        L->setIncomingValue(i, Chunks[0]);
        H->setIncomingValue(i, Chunks[1]);
      }
      break;
    }
    case Instruction::And:
    case Instruction::Or:
    case Instruction::Xor: {
      ChunksVec Left = getChunks(I->getOperand(0));
      ChunksVec Right = getChunks(I->getOperand(1));
      Instruction *L = cast<Instruction>(Split.Chunks[0]);
      Instruction *H = cast<Instruction>(Split.Chunks[1]);
      L->setOperand(0, Left[0]);  L->setOperand(1, Right[0]);
      H->setOperand(0, Left[1]); H->setOperand(1, Right[1]);
      break;
    }
    case Instruction::Call: {
      Instruction *L = cast<Instruction>(Split.Chunks[0]);
      // H doesn't need to be fixed, it's just a call to getHigh

      // fill in split parts of illegals
      CallInst *CI = dyn_cast<CallInst>(L);
      CallInst *OCI = dyn_cast<CallInst>(I);
      int Num = OCI->getNumArgOperands();
      for (int i = 0, j = 0; i < Num; i++, j++) {
        if (isIllegal(OCI->getArgOperand(i)->getType())) {
          ChunksVec Chunks = getChunks(OCI->getArgOperand(i));
          CI->setArgOperand(j, Chunks[0]);
          CI->setArgOperand(j + 1, Chunks[1]);
          j++;
        }
      }
      if (!isIllegal(I->getType())) {
        // legal return value, so just replace the old call with the new call
        I->replaceAllUsesWith(L);
      }
      break;
    }
    case Instruction::Switch: {
      SwitchInst *SI = dyn_cast<SwitchInst>(I);
      ChunksVec Chunks = getChunks(SI->getCondition());
      SwitchInst *NewSI = dyn_cast<SwitchInst>(Split.ToFix[0]);
      NewSI->setCondition(Chunks[0]);
      unsigned Num = Split.ToFix.size();
      for (unsigned i = 1; i < Num; i++) {
        Instruction *Curr = Split.ToFix[i];
        if (SwitchInst *SI = dyn_cast<SwitchInst>(Curr)) {
          SI->setCondition(Chunks[1]);
        } else {
          assert(0);
          Split.ToFix[i]->setOperand(0, Chunks[1]);
        }
      }
      break;
    }
    default: dumpIR(I); assert(0);
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
  Shl = Function::Create(FourFunc, GlobalValue::ExternalLinkage,
                          "bitshift64Shl", TheModule);

  if (!(GetHigh = TheModule->getFunction("getHigh32"))) {
    SmallVector<Type*, 0> GetHighArgTypes;
    FunctionType *GetHighFunc = FunctionType::get(i32, GetHighArgTypes, false);
    GetHigh = Function::Create(GetHighFunc, GlobalValue::ExternalLinkage,
                               "getHigh32", TheModule);
  }

  Type *V = Type::getVoidTy(TheModule->getContext());

  SmallVector<Type*, 1> SetHighArgTypes;
  SetHighArgTypes.push_back(i32);
  FunctionType *SetHighFunc = FunctionType::get(V, SetHighArgTypes, false);
  SetHigh = Function::Create(SetHighFunc, GlobalValue::ExternalLinkage,
                             "setHigh32", TheModule);

  Type *Double = Type::getDoubleTy(TheModule->getContext());
  Type *Float  = Type::getFloatTy(TheModule->getContext());

  SmallVector<Type*, 1> FtoITypes;
  FtoITypes.push_back(Float);
  FunctionType *FtoIFunc = FunctionType::get(i32, FtoITypes, false);

  SmallVector<Type*, 1> DtoITypes;
  DtoITypes.push_back(Double);
  FunctionType *DtoIFunc = FunctionType::get(i32, DtoITypes, false);

  FtoILow = Function::Create(FtoIFunc, GlobalValue::ExternalLinkage,
                             "FtoILow", TheModule);
  FtoIHigh = Function::Create(FtoIFunc, GlobalValue::ExternalLinkage,
                              "FtoIHigh", TheModule);
  DtoILow = Function::Create(DtoIFunc, GlobalValue::ExternalLinkage,
                             "DtoILow", TheModule);
  DtoIHigh = Function::Create(DtoIFunc, GlobalValue::ExternalLinkage,
                              "DtoIHigh", TheModule);
  BDtoILow = Function::Create(DtoIFunc, GlobalValue::ExternalLinkage,
                              "BDtoILow", TheModule);
  BDtoIHigh = Function::Create(DtoIFunc, GlobalValue::ExternalLinkage,
                               "BDtoIHigh", TheModule);

  SmallVector<Type*, 2> ItoTypes;
  ItoTypes.push_back(i32);
  ItoTypes.push_back(i32);

  FunctionType *ItoFFunc = FunctionType::get(Float, ItoTypes, false);
  SItoF = Function::Create(ItoFFunc, GlobalValue::ExternalLinkage,
                           "SItoF", TheModule);
  UItoF = Function::Create(ItoFFunc, GlobalValue::ExternalLinkage,
                           "UItoF", TheModule);

  FunctionType *ItoDFunc = FunctionType::get(Double, ItoTypes, false);
  SItoD = Function::Create(ItoDFunc, GlobalValue::ExternalLinkage,
                           "SItoD", TheModule);
  UItoD = Function::Create(ItoDFunc, GlobalValue::ExternalLinkage,
                           "UItoD", TheModule);

  BItoD = Function::Create(ItoDFunc, GlobalValue::ExternalLinkage,
                           "BItoD", TheModule);
}

bool ExpandI64::runOnModule(Module &M) {
  TheModule = &M;

  bool Changed = false;
  DataLayout DL(&M);
  // pre pass - legalize functions
  for (Module::iterator Iter = M.begin(), E = M.end(); Iter != E; ) {
    Function *Func = Iter++;
    ensureLegalFunc(Func);
  }

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
          if (isIllegal(T)) {
            Changed = true;
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
      // First, unlink them
      Type *i64 = Type::getInt64Ty(TheModule->getContext());
      Value *Zero  = Constant::getNullValue(i64);
      for (SplitsMap::iterator I = Splits.begin(); I != Splits.end(); I++) {
        //dump("unlink"); dumpIR(I->first);
        int Num = I->first->getNumOperands();
        for (int i = 0; i < Num; i++) { // -1 is the type of I itself
          Value *V = I->first->getOperand(i);
          Type *T = V->getType();
          if (isIllegal(T)) {
            I->first->setOperand(i, Zero);
          }
        }
      }

      // Now actually remove them
      for (SplitsMap::iterator I = Splits.begin(); I != Splits.end(); I++) {
        //dump("delete"); dumpIR(I->first);
        I->first->eraseFromParent();
      }
    }
  }

  // post pass - clean up illegal functions that were legalized
  for (Module::iterator Iter = M.begin(), E = M.end(); Iter != E; ) {
    Function *Func = Iter++;
    removeIllegalFunc(Func);
  }

  // remove bitcasts that were introduced while legalizing functions
  for (Module::iterator Iter = M.begin(), E = M.end(); Iter != E; ) {
    Function *Func = Iter++;
    for (Function::iterator BB = Func->begin(), E = Func->end();
         BB != E;
         ++BB) {
      for (BasicBlock::iterator Iter = BB->begin(), E = BB->end();
           Iter != E; ) {
        Instruction *I = Iter++;
        unsigned Opcode = I->getOpcode();
        if (Opcode == Instruction::BitCast || Opcode == Instruction::PtrToInt) {
          if (ConstantExpr *CE = dyn_cast<ConstantExpr>(I->getOperand(0))) {
            assert(CE->getOpcode() == Instruction::BitCast);
            assert(isa<FunctionType>(cast<PointerType>(CE->getType())->getElementType()));
            I->setOperand(0, CE->getOperand(0));
          }
        }
      }
    }
  }

  return Changed;
}

ModulePass *llvm::createExpandI64Pass() {
  return new ExpandI64();
}

