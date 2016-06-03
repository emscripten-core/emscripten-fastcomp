//===- ExpandI64.cpp - Expand i64 and wider integer types -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===------------------------------------------------------------------===//
//
// This pass expands and lowers all operations on integers i64 and wider
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

#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Analysis/ConstantFolding.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Transforms/NaCl.h"
#include "llvm/Transforms/Utils/Local.h"
#include <map>
#include <vector>

#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

  struct PhiBlockChange {
    BasicBlock *DD, *SwitchBB, *NewBB;
  };

  typedef SmallVector<Value*, 2> ChunksVec;
  typedef std::map<Value*, ChunksVec> SplitsMap;

  typedef SmallVector<PHINode *, 8> PHIVec;
  typedef SmallVector<Instruction *, 8> DeadVec;

  // This is a ModulePass because the pass recreates functions in
  // order to expand i64 arguments to pairs of i32s.
  class ExpandI64 : public ModulePass {
    bool Changed;
    const DataLayout *DL;
    Module *TheModule;

    SplitsMap Splits; // old illegal value to new insts
    PHIVec Phis;
    std::vector<PhiBlockChange> PhiBlockChanges;

    // If the function has an illegal return or argument, create a legal version
    void ensureLegalFunc(Function *F);

    // If a function is illegal, remove it
    void removeIllegalFunc(Function *F);

    // splits an illegal instruction into 32-bit chunks. We do
    // not yet have the values yet, as they depend on other
    // splits, so store the parts in Splits, for FinalizeInst.
    bool splitInst(Instruction *I);

    // For an illegal value, returns the split out chunks
    // representing the low and high parts, that splitInst
    // generated.
    // The value can also be a constant, in which case we just
    // split it, or a function argument, in which case we
    // map to the proper legalized new arguments
    //
    // @param AllowUnreachable  It is possible for phi nodes
    //                          to refer to unreachable blocks,
    //                          which our traversal never
    //                          reaches; this flag lets us
    //                          ignore those - otherwise,
    //                          not finding chunks is fatal
    ChunksVec getChunks(Value *V, bool AllowUnreachable=false);

    Function *Add, *Sub, *Mul, *SDiv, *UDiv, *SRem, *URem, *LShr, *AShr, *Shl, *GetHigh, *SetHigh, *FtoILow, *FtoIHigh, *DtoILow, *DtoIHigh, *SItoF, *UItoF, *SItoD, *UItoD, *BItoD, *BDtoILow, *BDtoIHigh;

    Function *AtomicAdd, *AtomicSub, *AtomicAnd, *AtomicOr, *AtomicXor;

    void ensureFuncs();
    unsigned getNumChunks(Type *T);

  public:
    static char ID;
    ExpandI64() : ModulePass(ID) {
      initializeExpandI64Pass(*PassRegistry::getPassRegistry());

      Add = Sub = Mul = SDiv = UDiv = SRem = URem = LShr = AShr = Shl = GetHigh = SetHigh = AtomicAdd = AtomicSub = AtomicAnd = AtomicOr = AtomicXor = NULL;
    }

    virtual bool runOnModule(Module &M);
  };
}

char ExpandI64::ID = 0;
INITIALIZE_PASS(ExpandI64, "expand-illegal-ints",
                "Expand and lower illegal >i32 operations into 32-bit chunks",
                false, false)

// Utilities

static Instruction *CopyDebug(Instruction *NewInst, Instruction *Original) {
  NewInst->setDebugLoc(Original->getDebugLoc());
  return NewInst;
}

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

static bool okToRemainIllegal(Function *F) {
  StringRef Name = F->getName();
  if (Name == "llvm.dbg.value") return true;

  // XXX EMSCRIPTEN: These take an i64 immediate argument; since they're not
  // real instructions, we don't need to legalize them.
  if (Name == "llvm.lifetime.start") return true;
  if (Name == "llvm.lifetime.end") return true;
  if (Name == "llvm.invariant.start") return true;
  if (Name == "llvm.invariant.end") return true;

  return false;
}

unsigned ExpandI64::getNumChunks(Type *T) {
  unsigned Num = DL->getTypeSizeInBits(T);
  return (Num + 31) / 32;
}

static bool isLegalFunctionType(FunctionType *FT) {
  if (isIllegal(FT->getReturnType())) {
    return false;
  }

  int Num = FT->getNumParams();
  for (int i = 0; i < Num; i++) {
    if (isIllegal(FT->getParamType(i))) {
      return false;
    }
  }

  return true;
}

static bool isLegalInstruction(const Instruction *I) {
  if (isIllegal(I->getType())) {
    return false;
  }

  for (unsigned i = 0, e = I->getNumOperands(); i != e; ++i) {
    if (isIllegal(I->getOperand(i)->getType())) {
      return false;
    }
  }

  return true;
}

// We can't use RecreateFunction because we need to handle
// function and argument attributes specially.
static Function *RecreateFunctionLegalized(Function *F, FunctionType *NewType) {
  Function *NewFunc = Function::Create(NewType, F->getLinkage());

  AttributeSet Attrs = F->getAttributes();
  AttributeSet FnAttrs = Attrs.getFnAttributes();

  // Legalizing the return value is done by storing part of the value into
  // static storage. Subsequent analysis will see this as a memory access,
  // so we can no longer claim to be readonly or readnone.
  if (isIllegal(F->getReturnType())) {
    FnAttrs = FnAttrs.removeAttribute(F->getContext(),
                                      AttributeSet::FunctionIndex,
                                      Attribute::ReadOnly);
    FnAttrs = FnAttrs.removeAttribute(F->getContext(),
                                      AttributeSet::FunctionIndex,
                                      Attribute::ReadNone);
  }

  NewFunc->addAttributes(AttributeSet::FunctionIndex, FnAttrs);
  NewFunc->addAttributes(AttributeSet::ReturnIndex, Attrs.getRetAttributes());
  Function::arg_iterator AI = F->arg_begin();

  // We need to recreate the attribute set, with the right indexes
  AttributeSet NewAttrs;
  unsigned NumArgs = F->arg_size();
  for (unsigned i = 1, j = 1; i < NumArgs+1; i++, j++, AI++) {
    if (isIllegal(AI->getType())) {
      j++;
      continue;
    }
    if (!Attrs.hasAttributes(i)) continue;
    AttributeSet ParamAttrs = Attrs.getParamAttributes(i);
    AttrBuilder AB;
    unsigned NumSlots = ParamAttrs.getNumSlots();
    for (unsigned k = 0; k < NumSlots; k++) {
      for (AttributeSet::iterator I = ParamAttrs.begin(k), E = ParamAttrs.end(k); I != E; I++) {
        AB.addAttribute(*I);
      }
    }
    NewFunc->addAttributes(j, AttributeSet::get(F->getContext(), j, AB));
  }

  F->getParent()->getFunctionList().insert(F->getIterator(), NewFunc);
  NewFunc->takeName(F);
  NewFunc->getBasicBlockList().splice(NewFunc->begin(),
                                      F->getBasicBlockList());
  F->replaceAllUsesWith(
      ConstantExpr::getBitCast(NewFunc,
                               F->getFunctionType()->getPointerTo()));
  return NewFunc;
}

void ExpandI64::ensureLegalFunc(Function *F) {
  if (okToRemainIllegal(F)) return;

  FunctionType *FT = F->getFunctionType();
  if (isLegalFunctionType(FT)) return;

  Changed = true;
  Function *NF = RecreateFunctionLegalized(F, getLegalizedFunctionType(FT));
  std::string Name = NF->getName();
  if (strncmp(Name.c_str(), "llvm.", 5) == 0) {
    // this is an intrinsic, and we are changing its signature, which will annoy LLVM, so rename
    const size_t len = Name.size();
    SmallString<256> NewName;
    NewName.resize(len);
    for (unsigned i = 0; i < len; i++) {
      NewName[i] = Name[i] != '.' ? Name[i] : '_';
    }
    NF->setName(Twine(NewName));
  }

  // Move and update arguments
  for (Function::arg_iterator Arg = F->arg_begin(), E = F->arg_end(), NewArg = NF->arg_begin();
       Arg != E; ++Arg) {
    if (Arg->getType() == NewArg->getType()) {
      NewArg->takeName(&*Arg);
      Arg->replaceAllUsesWith(&*NewArg);
      NewArg++;
    } else {
      // This was legalized
      ChunksVec &Chunks = Splits[&*Arg];
      int Num = getNumChunks(Arg->getType());
      assert(Num == 2);
      for (int i = 0; i < Num; i++) {
        Chunks.push_back(&*NewArg);
        if (NewArg->hasName()) Chunks[i]->setName(NewArg->getName() + "$" + utostr(i));
        NewArg++;
      }
    }
  }
}

void ExpandI64::removeIllegalFunc(Function *F) {
  if (okToRemainIllegal(F)) return;

  FunctionType *FT = F->getFunctionType();
  if (!isLegalFunctionType(FT)) {
    F->eraseFromParent();
  }
}

bool ExpandI64::splitInst(Instruction *I) {
  Type *i32 = Type::getInt32Ty(I->getContext());
  Type *i32P = i32->getPointerTo();
  Type *i64 = Type::getInt64Ty(I->getContext());
  Value *Zero  = Constant::getNullValue(i32);

  ChunksVec &Chunks = Splits[I];

  switch (I->getOpcode()) {
    case Instruction::GetElementPtr: {
      GetElementPtrInst *GEP = cast<GetElementPtrInst>(I);
      SmallVector<Value*, 2> NewOps;
      for (unsigned i = 1, e = I->getNumOperands(); i != e; ++i) {
        Value *Op = I->getOperand(i);
        if (isIllegal(Op->getType())) {
          // Truncate the operand down to one chunk.
          NewOps.push_back(getChunks(Op)[0]);
        } else {
          NewOps.push_back(Op);
        }
      }
      Value *NewGEP = CopyDebug(GetElementPtrInst::Create(GEP->getSourceElementType(), GEP->getPointerOperand(), NewOps, "", GEP), GEP);
      Chunks.push_back(NewGEP);
      I->replaceAllUsesWith(NewGEP);
      break;
    }
    case Instruction::SExt: {
      ChunksVec InputChunks;
      Value *Op = I->getOperand(0);
      if (isIllegal(Op->getType())) {
        InputChunks = getChunks(Op);
      } else {
        InputChunks.push_back(Op);
      }

      for (unsigned i = 0, e = InputChunks.size(); i != e; ++i) {
        Value *Input = InputChunks[i];

        Type *T = Input->getType();
        Value *Chunk;
        if (T->getIntegerBitWidth() < 32) {
          Chunk = CopyDebug(new SExtInst(Input, i32, "", I), I);
        } else {
          assert(T->getIntegerBitWidth() == 32);
          Chunk = Input;
        }
        Chunks.push_back(Chunk);
      }

      Instruction *Check = CopyDebug(new ICmpInst(I, ICmpInst::ICMP_SLT, Chunks.back(), Zero), I);
      int Num = getNumChunks(I->getType());
      for (int i = Chunks.size(); i < Num; i++) {
        Instruction *High = CopyDebug(new SExtInst(Check, i32, "", I), I);
        Chunks.push_back(High);
      }
      break;
    }
    case Instruction::PtrToInt:
    case Instruction::ZExt: {
      Value *Op = I->getOperand(0);
      ChunksVec InputChunks;
      if (I->getOpcode() == Instruction::PtrToInt) {
        InputChunks.push_back(CopyDebug(new PtrToIntInst(Op, i32, "", I), I));
      } else if (isIllegal(Op->getType())) {
        InputChunks = getChunks(Op);
      } else {
        InputChunks.push_back(Op);
      }

      for (unsigned i = 0, e = InputChunks.size(); i != e; ++i) {
        Value *Input = InputChunks[i];
        Type *T = Input->getType();

        Value *Chunk;
        if (T->getIntegerBitWidth() < 32) {
          Chunk = CopyDebug(new ZExtInst(Input, i32, "", I), I);
        } else {
          assert(T->getIntegerBitWidth() == 32);
          Chunk = Input;
        }
        Chunks.push_back(Chunk);
      }

      int Num = getNumChunks(I->getType());
      for (int i = Chunks.size(); i < Num; i++) {
        Chunks.push_back(Zero);
      }
      break;
    }
    case Instruction::IntToPtr:
    case Instruction::Trunc: {
      unsigned Num = getNumChunks(I->getType());
      unsigned NumBits = DL->getTypeSizeInBits(I->getType());
      ChunksVec InputChunks = getChunks(I->getOperand(0));
      for (unsigned i = 0; i < Num; i++) {
        Value *Input = InputChunks[i];

        Value *Chunk;
        if (NumBits < 32) {
          Chunk = CopyDebug(new TruncInst(Input, IntegerType::get(I->getContext(), NumBits), "", I), I);
          NumBits = 0;
        } else {
          Chunk = Input;
          NumBits -= 32;
        }
        if (I->getOpcode() == Instruction::IntToPtr) {
          assert(i == 0);
          Chunk = CopyDebug(new IntToPtrInst(Chunk, I->getType(), "", I), I);
        }
        Chunks.push_back(Chunk);
      }
      if (!isIllegal(I->getType())) {
        assert(Chunks.size() == 1);
        I->replaceAllUsesWith(Chunks[0]);
      }
      break;
    }
    case Instruction::Load: {
      LoadInst *LI = cast<LoadInst>(I);
      Instruction *AI = CopyDebug(new PtrToIntInst(LI->getPointerOperand(), i32, "", I), I);
      int Num = getNumChunks(I->getType());
      for (int i = 0; i < Num; i++) {
        Instruction *Add = i == 0 ? AI : CopyDebug(BinaryOperator::Create(Instruction::Add, AI, ConstantInt::get(i32, 4*i), "", I), I);
        Instruction *Ptr = CopyDebug(new IntToPtrInst(Add, i32P, "", I), I);
        LoadInst *Chunk = new LoadInst(Ptr, "", I); CopyDebug(Chunk, I);
        Chunk->setAlignment(MinAlign(LI->getAlignment() == 0 ?
                                         DL->getABITypeAlignment(LI->getType()) :
                                         LI->getAlignment(),
                                     4*i));
        Chunk->setVolatile(LI->isVolatile());
        Chunk->setOrdering(LI->getOrdering());
        Chunk->setSynchScope(LI->getSynchScope());
        Chunks.push_back(Chunk);
      }
      break;
    }
    case Instruction::Store: {
      StoreInst *SI = cast<StoreInst>(I);
      Instruction *AI = CopyDebug(new PtrToIntInst(SI->getPointerOperand(), i32, "", I), I);
      ChunksVec InputChunks = getChunks(SI->getValueOperand());
      int Num = InputChunks.size();
      for (int i = 0; i < Num; i++) {
        Instruction *Add = i == 0 ? AI : CopyDebug(BinaryOperator::Create(Instruction::Add, AI, ConstantInt::get(i32, 4*i), "", I), I);
        Instruction *Ptr = CopyDebug(new IntToPtrInst(Add, i32P, "", I), I);
        StoreInst *Chunk = new StoreInst(InputChunks[i], Ptr, I);
        Chunk->setAlignment(MinAlign(SI->getAlignment() == 0 ?
                                         DL->getABITypeAlignment(SI->getValueOperand()->getType()) :
                                         SI->getAlignment(),
                                     4*i));
        Chunk->setVolatile(SI->isVolatile());
        Chunk->setOrdering(SI->getOrdering());
        Chunk->setSynchScope(SI->getSynchScope());
        CopyDebug(Chunk, I);
      }
      break;
    }
    case Instruction::Ret: {
      assert(I->getOperand(0)->getType() == i64);
      ChunksVec InputChunks = getChunks(I->getOperand(0));
      ensureFuncs();
      SmallVector<Value *, 1> Args;
      Args.push_back(InputChunks[1]);
      CopyDebug(CallInst::Create(SetHigh, Args, "", I), I);
      CopyDebug(ReturnInst::Create(I->getContext(), InputChunks[0], I), I);
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
      ChunksVec LeftChunks = getChunks(I->getOperand(0));
      ChunksVec RightChunks = getChunks(I->getOperand(1));
      unsigned Num = getNumChunks(I->getType());
      if (Num == 2) {
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
          case Instruction::AShr: F = AShr; break;
          case Instruction::LShr: {
            if (ConstantInt *CI = dyn_cast<ConstantInt>(I->getOperand(1))) {
              unsigned Shifts = CI->getZExtValue();
              if (Shifts == 32) {
                Low = LeftChunks[1];
                High = Zero;
                break;
              }
            }
            F = LShr;
            break;
          }
          case Instruction::Shl: {
            if (ConstantInt *CI = dyn_cast<ConstantInt>(I->getOperand(1))) {
              const APInt &Shifts = CI->getValue();
              if (Shifts == 32) {
                Low = Zero;
                High = LeftChunks[0];
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
          Args.push_back(LeftChunks[0]);
          Args.push_back(LeftChunks[1]);
          Args.push_back(RightChunks[0]);
          Args.push_back(RightChunks[1]);
          Low = CopyDebug(CallInst::Create(F, Args, "", I), I);
          High = CopyDebug(CallInst::Create(GetHigh, "", I), I);
        }
        Chunks.push_back(Low);
        Chunks.push_back(High);
      } else {
        // more than 64 bits. handle simple shifts for lshr and shl
        assert(I->getOpcode() == Instruction::LShr || I->getOpcode() == Instruction::AShr || I->getOpcode() == Instruction::Shl);
        ConstantInt *CI = cast<ConstantInt>(I->getOperand(1));
        unsigned Shifts = CI->getZExtValue();
        unsigned Fraction = Shifts % 32;
        Constant *Frac = ConstantInt::get(i32, Fraction);
        Constant *Comp = ConstantInt::get(i32, 32 - Fraction);
        Instruction::BinaryOps Opcode, Reverse;
        unsigned ShiftChunks, Dir;
        Value *TopFiller = Zero;
        if (I->getOpcode() == Instruction::Shl) {
          Opcode = Instruction::Shl;
          Reverse = Instruction::LShr;
          ShiftChunks = -(Shifts/32);
          Dir = -1;
        } else {
          Opcode = Instruction::LShr;
          Reverse = Instruction::Shl;
          ShiftChunks = Shifts/32;
          Dir = 1;
          if (I->getOpcode() == Instruction::AShr) {
            Value *Cond = CopyDebug(new ICmpInst(I, ICmpInst::ICMP_SLT, LeftChunks[LeftChunks.size()-1], Zero), I);
            TopFiller = CopyDebug(SelectInst::Create(Cond, ConstantInt::get(i32, -1), Zero, "", I), I);
          }
        }
        for (unsigned i = 0; i < Num; i++) {
          Value *L;
          if (i + ShiftChunks < LeftChunks.size()) {
            L = LeftChunks[i + ShiftChunks];
          } else {
            L = Zero;
          }

          Value *H;
          if (i + ShiftChunks + Dir < LeftChunks.size()) {
            H = LeftChunks[i + ShiftChunks + Dir];
          } else {
            H = TopFiller;
          }

          // shifted the fractional amount
          if (Frac != Zero && L != Zero) {
            if (Fraction == 32) {
              L = Zero;
            } else {
              L = CopyDebug(BinaryOperator::Create(Opcode, L, Frac, "", I), I);
            }
          }
          // shifted the complement-fractional amount to the other side
          if (Comp != Zero && H != Zero) {
            if (Fraction == 0) {
              H = TopFiller;
            } else {
              H = CopyDebug(BinaryOperator::Create(Reverse, H, Comp, "", I), I);
            }
          }

          // Or the parts together. Since we may have zero, try to fold it away.
          if (Value *V = SimplifyBinOp(Instruction::Or, L, H, *DL)) {
            Chunks.push_back(V);
          } else {
            Chunks.push_back(CopyDebug(BinaryOperator::Create(Instruction::Or, L, H, "", I), I));
          }
        }
      }
      break;
    }
    case Instruction::ICmp: {
      ICmpInst *CE = cast<ICmpInst>(I);
      ICmpInst::Predicate Pred = CE->getPredicate();
      ChunksVec LeftChunks = getChunks(I->getOperand(0));
      ChunksVec RightChunks = getChunks(I->getOperand(1));
      switch (Pred) {
        case ICmpInst::ICMP_EQ:
        case ICmpInst::ICMP_NE: {
          ICmpInst::Predicate PartPred; // the predicate to use on each of the parts
          llvm::Instruction::BinaryOps CombineOp; // the predicate to use to combine the subcomparisons
          int Num = LeftChunks.size();
          if (Pred == ICmpInst::ICMP_EQ) {
            PartPred = ICmpInst::ICMP_EQ;
            CombineOp = Instruction::And;
          } else {
            PartPred = ICmpInst::ICMP_NE;
            CombineOp = Instruction::Or;
          }
          // first combine 0 and 1. then combine that with 2, etc.
          Value *Combined = NULL;
          for (int i = 0; i < Num; i++) {
            Value *Cmp = CopyDebug(new ICmpInst(I, PartPred, LeftChunks[i], RightChunks[i]), I);
            Combined = !Combined ? Cmp : CopyDebug(BinaryOperator::Create(CombineOp, Combined, Cmp, "", I), I);
          }
          I->replaceAllUsesWith(Combined);
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
          if (ConstantInt *CI = dyn_cast<ConstantInt>(I->getOperand(1))) {
            if (CI->getZExtValue() == 0 && Pred == ICmpInst::ICMP_SLT) {
              // strict < 0 is easy to do, even on non-i64, just the sign bit matters
              Instruction *NewInst = new ICmpInst(I, ICmpInst::ICMP_SLT, LeftChunks[LeftChunks.size()-1], Zero);
              CopyDebug(NewInst, I);
              I->replaceAllUsesWith(NewInst);
              return true;
            }
          }
          Type *T = I->getOperand(0)->getType();
          assert(T->isIntegerTy() && T->getIntegerBitWidth() % 32 == 0);
          int NumChunks = getNumChunks(T);
          assert(NumChunks >= 2);
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
          // general pattern is
          // a,b,c < A,B,C    =>    c < C || (c == C && b < B) || (c == C && b == B && a < A)
          Instruction *Final = CopyDebug(new ICmpInst(I, StrictPred, LeftChunks[NumChunks-1], RightChunks[NumChunks-1]), I);
          for (int i = NumChunks-2; i >= 0; i--) {
            Instruction *Curr = CopyDebug(new ICmpInst(I, UnsignedPred, LeftChunks[i], RightChunks[i]), I);
            for (int j = NumChunks-1; j > i; j--) {
              Instruction *Temp = CopyDebug(new ICmpInst(I, ICmpInst::ICMP_EQ, LeftChunks[j], RightChunks[j]), I);
              Curr = CopyDebug(BinaryOperator::Create(Instruction::And, Temp, Curr, "", I), I);
            }
            Final = CopyDebug(BinaryOperator::Create(Instruction::Or, Final, Curr, "", I), I);
          }
          I->replaceAllUsesWith(Final);
          break;
        }
        default: assert(0);
      }
      break;
    }
    case Instruction::Select: {
      SelectInst *SI = cast<SelectInst>(I);
      Value *Cond = SI->getCondition();
      ChunksVec TrueChunks = getChunks(SI->getTrueValue());
      ChunksVec FalseChunks = getChunks(SI->getFalseValue());
      unsigned Num = getNumChunks(I->getType());
      for (unsigned i = 0; i < Num; i++) {
        Instruction *Part = CopyDebug(SelectInst::Create(Cond, TrueChunks[i], FalseChunks[i], "", I), I);
        Chunks.push_back(Part);
      }
      break;
    }
    case Instruction::PHI: {
      PHINode *Parent = cast<PHINode>(I);
      int Num = getNumChunks(I->getType());
      int PhiNum = Parent->getNumIncomingValues();
      for (int i = 0; i < Num; i++) {
        Instruction *P = CopyDebug(PHINode::Create(i32, PhiNum, "", I), I);
        Chunks.push_back(P);
      }
      // PHI node operands may not be translated yet; we'll handle them at the end.
      Phis.push_back(Parent);
      break;
    }
    case Instruction::And:
    case Instruction::Or:
    case Instruction::Xor: {
      BinaryOperator *BO = cast<BinaryOperator>(I);
      ChunksVec LeftChunks = getChunks(BO->getOperand(0));
      ChunksVec RightChunks = getChunks(BO->getOperand(1));
      int Num = getNumChunks(BO->getType());
      for (int i = 0; i < Num; i++) {
        // If there's a constant operand, it's likely enough that one of the
        // chunks will be a trivial operation, so it's worth calling
        // SimplifyBinOp here.
        if (Value *V = SimplifyBinOp(BO->getOpcode(), LeftChunks[i], RightChunks[i], *DL)) {
          Chunks.push_back(V);
        } else {
          Chunks.push_back(CopyDebug(BinaryOperator::Create(BO->getOpcode(), LeftChunks[i], RightChunks[i], "", BO), BO));
        }
      }
      break;
    }
    case Instruction::Call: {
      CallInst *CI = cast<CallInst>(I);
      Function *F = CI->getCalledFunction();
      if (F) {
        assert(okToRemainIllegal(F));
        return false;
      }
      Value *CV = CI->getCalledValue();
      FunctionType *OFT = NULL;
      if (ConstantExpr *CE = dyn_cast<ConstantExpr>(CV)) {
        assert(CE);
        OFT = cast<FunctionType>(cast<PointerType>(CE->getType())->getElementType());
        Constant *C = CE->getOperand(0);
        if (CE->getOpcode() == Instruction::BitCast) {
          CV = ConstantExpr::getBitCast(C, getLegalizedFunctionType(OFT)->getPointerTo());
        } else if (CE->getOpcode() == Instruction::IntToPtr) {
          CV = ConstantExpr::getIntToPtr(C, getLegalizedFunctionType(OFT)->getPointerTo());
        } else {
          llvm_unreachable("Bad CE in i64 Call");
        }
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
          ChunksVec ArgChunks = getChunks(CI->getArgOperand(i));
          Args.push_back(ArgChunks[0]);
          Args.push_back(ArgChunks[1]);
        }
      }
      Instruction *L = CopyDebug(CallInst::Create(CV, Args, "", I), I);
      Instruction *H = NULL;
      // legalize return value as well, if necessary
      if (isIllegal(I->getType())) {
        assert(I->getType() == i64);
        ensureFuncs();
        H = CopyDebug(CallInst::Create(GetHigh, "", I), I);
        Chunks.push_back(L);
        Chunks.push_back(H);
      } else {
        I->replaceAllUsesWith(L);
      }
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
      Chunks.push_back(L);
      Chunks.push_back(H);
      break;
    }
    case Instruction::BitCast: {
      if (I->getType() == Type::getDoubleTy(TheModule->getContext())) {
        // fall through to itofp
      } else if (I->getOperand(0)->getType() == Type::getDoubleTy(TheModule->getContext())) {
        // double to i64
        assert(I->getType() == i64);
        ensureFuncs();
        SmallVector<Value *, 1> Args;
        Args.push_back(I->getOperand(0));
        Instruction *L = CopyDebug(CallInst::Create(BDtoILow, Args, "", I), I);
        Instruction *H = CopyDebug(CallInst::Create(BDtoIHigh, Args, "", I), I);
        Chunks.push_back(L);
        Chunks.push_back(H);
        break;
      } else if (isa<VectorType>(I->getOperand(0)->getType()) && !isa<VectorType>(I->getType())) {
        unsigned NumElts = getNumChunks(I->getType());
        VectorType *IVTy = VectorType::get(i32, NumElts);
        Instruction *B = CopyDebug(new BitCastInst(I->getOperand(0), IVTy, "", I), I);
        for (unsigned i = 0; i < NumElts; ++i) {
          Constant *Idx = ConstantInt::get(i32, i);
          Instruction *Ext = CopyDebug(ExtractElementInst::Create(B, Idx, "", I), I);
          Chunks.push_back(Ext);
        }
        break;
      } else {
        // no-op bitcast
        assert(I->getType() == I->getOperand(0)->getType() && "possible hint: optimize with -O0 or -O2+, and not -O1");
        Chunks = getChunks(I->getOperand(0));
        break;
      }
    }
    case Instruction::SIToFP:
    case Instruction::UIToFP: {
      assert(I->getOperand(0)->getType() == i64);
      ensureFuncs();
      ChunksVec InputChunks = getChunks(I->getOperand(0));
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
      Instruction *D = CopyDebug(CallInst::Create(F, InputChunks, "", I), I);
      I->replaceAllUsesWith(D);
      break;
    }
    case Instruction::Switch: {
      assert(I->getOperand(0)->getType() == i64);
      ChunksVec InputChunks = getChunks(I->getOperand(0));

      // do a switch on the lower 32 bits, into a different basic block for each target, then do a branch in each of those on the high 32 bits
      SwitchInst* SI = cast<SwitchInst>(I);
      BasicBlock *DD = SI->getDefaultDest();
      BasicBlock *SwitchBB = I->getParent();
      Function *F = SwitchBB->getParent();

      unsigned NumItems = SI->getNumCases();
      SwitchInst *LowSI = SwitchInst::Create(InputChunks[0], DD, NumItems, I); // same default destination: if lower bits do not match, go straight to default
      CopyDebug(LowSI, I);

      typedef std::pair<uint32_t, BasicBlock*> Pair;
      typedef std::vector<Pair> Vec; // vector of pairs of high 32 bits, basic block
      typedef std::map<uint32_t, Vec> Map; // maps low 32 bits to their Vec info
      Map Groups;                          // (as two 64-bit values in the switch may share their lower bits)

      for (SwitchInst::CaseIt i = SI->case_begin(), e = SI->case_end(); i != e; ++i) {
        BasicBlock *BB = i.getCaseSuccessor();
        uint64_t Bits = i.getCaseValue()->getZExtValue();
        uint32_t LowBits = (uint32_t)Bits;
        uint32_t HighBits = (uint32_t)(Bits >> 32);
        Vec& V = Groups[LowBits];
        V.push_back(Pair(HighBits, BB));
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
          Instruction *CheckHigh = CopyDebug(new ICmpInst(*NewBB, ICmpInst::ICMP_EQ, InputChunks[1], ConstantInt::get(i32, V[0]->first)), I);
          Split.ToFix.push_back(CheckHigh);
          CopyDebug(BranchInst::Create(V[0]->second, DD, CheckHigh, NewBB), I);
        } else {*/

        // multiple options, create a switch - we could also optimize and make an icmp/branch if just one, as in commented code above
        SwitchInst *HighSI = SwitchInst::Create(InputChunks[1], DD, V.size(), NewBB); // same default destination: if lower bits do not match, go straight to default
        for (unsigned i = 0; i < V.size(); i++) {
          BasicBlock *BB = V[i].second;
          HighSI->addCase(cast<ConstantInt>(ConstantInt::get(i32, V[i].first)), BB);
          // fix phis, we used to go SwitchBB->BB, but now go SwitchBB->NewBB->BB, so we look like we arrived from NewBB. Replace the phi from the
          // now unneeded SwitchBB to the new BB
          // We cannot do this here right now, as phis we encounter may be in the middle of processing (empty), so we queue these.
          for (BasicBlock::iterator I = BB->begin(); I != BB->end(); ++I) {
            PHINode *Phi = dyn_cast<PHINode>(I);
            if (!Phi) break;
            PhiBlockChange Change;
            Change.DD = BB;
            Change.SwitchBB = SwitchBB;
            Change.NewBB = NewBB;
            PhiBlockChanges.push_back(Change);
            break; // we saw a phi on this BB, and pushed a Change
          }
        }

        // We used to go SwitchBB->DD, but now go SwitchBB->NewBB->DD, fix that like with BB above. However here we do not replace,
        // as the switch BB is still possible to arrive from - we can arrive at the default if either the lower bits were wrong (we
        // arrive from the switchBB) or from the NewBB if the high bits were wrong.
        PhiBlockChange Change;
        Change.DD = DD;
        Change.SwitchBB = SwitchBB;
        Change.NewBB = NewBB;
        PhiBlockChanges.push_back(Change);
      }
      break;
    }
    case Instruction::AtomicRMW: {
      const AtomicRMWInst *rmwi = cast<AtomicRMWInst>(I);
      ChunksVec Chunks32Bit = getChunks(I->getOperand(1));
      unsigned Num = getNumChunks(I->getType());
      assert(Num == 2 && "Only know how to handle 32-bit and 64-bit AtomicRMW instructions!");
      ensureFuncs();
      Value *Low = NULL, *High = NULL;
      Function *F = NULL;
      switch (rmwi->getOperation()) {
        case AtomicRMWInst::Add: F = AtomicAdd; break;
        case AtomicRMWInst::Sub: F = AtomicSub; break;
        case AtomicRMWInst::And: F = AtomicAnd; break;
        case AtomicRMWInst::Or: F = AtomicOr; break;
        case AtomicRMWInst::Xor: F = AtomicXor; break;
        case AtomicRMWInst::Xchg:
        case AtomicRMWInst::Nand:
        case AtomicRMWInst::Max:
        case AtomicRMWInst::Min:
        case AtomicRMWInst::UMax:
        case AtomicRMWInst::UMin:
        default: llvm_unreachable("Bad atomic operation");
      }
      SmallVector<Value *, 3> Args;
      Args.push_back(new BitCastInst(I->getOperand(0), Type::getInt8PtrTy(TheModule->getContext()), "", I));
      Args.push_back(Chunks32Bit[0]);
      Args.push_back(Chunks32Bit[1]);
      Low = CopyDebug(CallInst::Create(F, Args, "", I), I);
      High = CopyDebug(CallInst::Create(GetHigh, "", I), I);
      Chunks.push_back(Low);
      Chunks.push_back(High);
      break;
    }
    case Instruction::AtomicCmpXchg: {
      assert(0 && "64-bit compare-and-exchange (__sync_bool_compare_and_swap & __sync_val_compare_and_swap) are not supported! Please directly call emscripten_atomic_cas_u64() instead in order to emulate!");
      break;
    }
    default: {
      I->dump();
      assert(0 && "some i64 thing we can't legalize yet. possible hint: optimize with -O0 or -O2+, and not -O1");
    }
  }

  return true;
}

ChunksVec ExpandI64::getChunks(Value *V, bool AllowUnreachable) {
  assert(isIllegal(V->getType()));

  unsigned Num = getNumChunks(V->getType());
  Type *i32 = Type::getInt32Ty(V->getContext());

  if (isa<UndefValue>(V))
    return ChunksVec(Num, UndefValue::get(i32));

  if (Constant *C = dyn_cast<Constant>(V)) {
    ChunksVec Chunks;
    for (unsigned i = 0; i < Num; i++) {
      Constant *Count = ConstantInt::get(C->getType(), i * 32);
      Constant *NewC = ConstantExpr::getTrunc(ConstantExpr::getLShr(C, Count), i32);
      TargetLibraryInfo *TLI = 0; // TODO
      if (ConstantExpr *NewCE = dyn_cast<ConstantExpr>(NewC)) {
        if (Constant *FoldedC = ConstantFoldConstantExpression(NewCE, *DL, TLI)) {
          NewC = FoldedC;
        }
      }

      Chunks.push_back(NewC);
    }
    return Chunks;
  }

  if (Splits.find(V) == Splits.end()) {
    if (AllowUnreachable)
      return ChunksVec(Num, UndefValue::get(i32));
    errs() << *V << "\n";
    report_fatal_error("could not find chunks for illegal value");
  }
  assert(Splits[V].size() == Num);
  return Splits[V];
}

void ExpandI64::ensureFuncs() {
  if (Add != NULL) return;

  Type *i32 = Type::getInt32Ty(TheModule->getContext());

  SmallVector<Type*, 3> ThreeArgTypes;
  ThreeArgTypes.push_back(Type::getInt8PtrTy(TheModule->getContext()));
  ThreeArgTypes.push_back(i32);
  ThreeArgTypes.push_back(i32);
  FunctionType *ThreeFunc = FunctionType::get(i32, ThreeArgTypes, false);

  AtomicAdd = TheModule->getFunction("_emscripten_atomic_fetch_and_add_u64");
  if (!AtomicAdd) {
    AtomicAdd = Function::Create(ThreeFunc, GlobalValue::ExternalLinkage,
                                 "_emscripten_atomic_fetch_and_add_u64", TheModule);
  }
  AtomicSub = TheModule->getFunction("_emscripten_atomic_fetch_and_sub_u64");
  if (!AtomicSub) {
    AtomicSub = Function::Create(ThreeFunc, GlobalValue::ExternalLinkage,
                                 "_emscripten_atomic_fetch_and_sub_u64", TheModule);
  }
  AtomicAnd = TheModule->getFunction("_emscripten_atomic_fetch_and_and_u64");
  if (!AtomicAnd) {
    AtomicAnd = Function::Create(ThreeFunc, GlobalValue::ExternalLinkage,
                                 "_emscripten_atomic_fetch_and_and_u64", TheModule);
  }
  AtomicOr = TheModule->getFunction("_emscripten_atomic_fetch_and_or_u64");
  if (!AtomicOr) {
    AtomicOr = Function::Create(ThreeFunc, GlobalValue::ExternalLinkage,
                                 "_emscripten_atomic_fetch_and_or_u64", TheModule);
  }
  AtomicXor = TheModule->getFunction("_emscripten_atomic_fetch_and_xor_u64");
  if (!AtomicXor) {
    AtomicXor = Function::Create(ThreeFunc, GlobalValue::ExternalLinkage,
                                 "_emscripten_atomic_fetch_and_xor_u64", TheModule);
  }

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
  DL = &M.getDataLayout();
  Splits.clear();
  Changed = false;

  // pre pass - legalize functions
  for (Module::iterator Iter = M.begin(), E = M.end(); Iter != E; ) {
    Function *Func = &*Iter++;
    ensureLegalFunc(Func);
  }

  // first pass - split
  DeadVec Dead;
  for (Module::iterator Iter = M.begin(), E = M.end(); Iter != E; ++Iter) {
    Function *Func = &*Iter;
    if (Func->isDeclaration()) {
      continue;
    }

    // Walk the body of the function. We use reverse postorder so that we visit
    // all operands of an instruction before the instruction itself. The
    // exception to this is PHI nodes, which we put on a list and handle below.
    ReversePostOrderTraversal<Function*> RPOT(Func);
    for (ReversePostOrderTraversal<Function*>::rpo_iterator RI = RPOT.begin(),
         RE = RPOT.end(); RI != RE; ++RI) {
      BasicBlock *BB = *RI;
      for (BasicBlock::iterator Iter = BB->begin(), E = BB->end();
           Iter != E; ) {
        Instruction *I = &*Iter++;
        if (!isLegalInstruction(I)) {
          if (splitInst(I)) {
            Changed = true;
            Dead.push_back(I);
          }
        }
      }
    }

    // Fix up PHI node operands.
    while (!Phis.empty()) {
      PHINode *PN = Phis.pop_back_val();
      ChunksVec OutputChunks = getChunks(PN);
      for (unsigned j = 0, je = PN->getNumIncomingValues(); j != je; ++j) {
        Value *Op = PN->getIncomingValue(j);
        ChunksVec InputChunks = getChunks(Op, true);
        for (unsigned k = 0, ke = OutputChunks.size(); k != ke; ++k) {
          PHINode *NewPN = cast<PHINode>(OutputChunks[k]);
          NewPN->addIncoming(InputChunks[k], PN->getIncomingBlock(j));
        }
      }
      PN->dropAllReferences();
    }

    // Delete instructions which were replaced. We do this after the full walk
    // of the instructions so that all uses are replaced first.
    while (!Dead.empty()) {
      Instruction *D = Dead.pop_back_val();
      D->eraseFromParent();
    }

    // Apply basic block changes to phis, now that phis are all processed (and illegal phis erased)
    for (unsigned i = 0; i < PhiBlockChanges.size(); i++) {
      PhiBlockChange &Change = PhiBlockChanges[i];
      for (BasicBlock::iterator I = Change.DD->begin(); I != Change.DD->end(); ++I) {
        PHINode *Phi = dyn_cast<PHINode>(I);
        if (!Phi) break;
        int Index = Phi->getBasicBlockIndex(Change.SwitchBB);
        assert(Index >= 0);
        Phi->addIncoming(Phi->getIncomingValue(Index), Change.NewBB);
      }
    }
    PhiBlockChanges.clear();

    // We only visited blocks found by a DFS walk from the entry, so we haven't
    // visited any unreachable blocks, and they may still contain illegal
    // instructions at this point. Being unreachable, they can simply be deleted.
    removeUnreachableBlocks(*Func);
  }

  // post pass - clean up illegal functions that were legalized. We do this
  // after the full walk of the functions so that all uses are replaced first.
  for (Module::iterator Iter = M.begin(), E = M.end(); Iter != E; ) {
    Function *Func = &*Iter++;
    removeIllegalFunc(Func);
  }

  return Changed;
}

ModulePass *llvm::createExpandI64Pass() {
  return new ExpandI64();
}
