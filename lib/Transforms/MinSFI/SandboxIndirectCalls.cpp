//===- SandboxIndirectCalls.cpp - Apply CFI to indirect function calls ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This is a pass which applies basic control-flow integrity enforcement to
// indirect function calls as a mitigation technique against attempts to
// subvert code execution.
//
// Pointers to address-taken functions are placed into global function tables
// (one function table is created per signature) and pointers to functions are
// replaced with the respective table indices. Indirect function calls are
// rewritten to treat the target pointer as an index and to load the actual
// pointer from the corresponding table.
//
// The zero-index entry of each table is set to null to provide consistent
// behaviour for null pointers. Tables are also padded with null entries to
// round their size to the nearest power of two and indices passed to calls
// are bit-masked accordingly in order to prevent buffer overflow during the
// load from the table.
//
// Even if placed into different tables, two functions are never assigned the
// same index. Interpreting a function pointer as a function of an incorrect
// signature will therefore result in jumping to null.
//
// Pointer arithmetic is not allowed on function pointers and will result in
// undefined behaviour.
//
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Transforms/MinSFI.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

static const char InternalSymName_FunctionTable[] = "__sfi_function_table";

namespace {
// This pass needs to be a ModulePass because it adds a GlobalVariable.
class SandboxIndirectCalls : public ModulePass {
 public:
  static char ID;
  SandboxIndirectCalls() : ModulePass(ID) {
    initializeSandboxIndirectCallsPass(*PassRegistry::getPassRegistry());
  }

  virtual bool runOnModule(Module &M);
};
}  // namespace

static inline size_t RoundToPowerOf2(size_t n) {
  if (isPowerOf2_64(n))
    return n;
  else
    return NextPowerOf2(n);
}

static inline bool IsPtrToIntUse(const Function::user_iterator &FuncUser) {
  if (isa<PtrToIntInst>(*FuncUser))
    return true;
  else if (ConstantExpr *Expr = dyn_cast<ConstantExpr>(*FuncUser))
    return Expr->getOpcode() == Instruction::PtrToInt;
  else
    return false;
}

// Function use is a direct call if the user is a call instruction and
// the function is its last operand.
static inline bool IsDirectCallUse(const Function::user_iterator &FuncUser) {
  if (CallInst *Call = dyn_cast<CallInst>(*FuncUser))
    return FuncUser.getOperandNo() == Call->getNumArgOperands();
  else
    return false;
}

bool SandboxIndirectCalls::runOnModule(Module &M) {
  typedef SmallVector<Constant*, 16> FunctionVector;
  DataLayout DL(&M);
  Type *I32 = Type::getInt32Ty(M.getContext());
  Type *IntPtrType = DL.getIntPtrType(M.getContext());

  // First, we find all address-taken functions and assign each an index.
  // Pointers in code are then immediately replaced with these indices, even
  // though the tables have not been created yet.
  FunctionVector AddrTakenFuncs;
  for (Module::iterator Func = M.begin(), E = M.end(); Func != E; ++Func) {
    bool HasIndirectUse = false;
    Constant *Index = ConstantInt::get(IntPtrType, AddrTakenFuncs.size() + 1);
    for (Function::user_iterator User = Func->user_begin(),
                                 E = Func->user_end();
         User != E; ++User) {
      if (IsPtrToIntUse(User)) {
        HasIndirectUse = true;
        (*User)->replaceAllUsesWith(Index);
        if (Instruction *UserInst = dyn_cast<Instruction>(*User))
          UserInst->eraseFromParent();
      } else if (!IsDirectCallUse(User)) {
        report_fatal_error("SandboxIndirectCalls: Invalid reference to "
                           "function @" + Func->getName());
      }
    }

    if (HasIndirectUse)
      AddrTakenFuncs.push_back(Func);
  }

  // Return if no address-taken functions have been found.
  if (AddrTakenFuncs.empty())
    return false;

  // Generate and fill out the function tables. Their size is rounded up to the
  // nearest power of two, index zero is reserved for null and functions are
  // stored under the indices that were assigned to them earlier.
  size_t FuncIndex = 1;
  size_t TableSize = RoundToPowerOf2(AddrTakenFuncs.size() + 1);
  DenseMap<PointerType*, FunctionVector> TableEntries;
  for (FunctionVector::iterator It = AddrTakenFuncs.begin(),
       E = AddrTakenFuncs.end(); It != E; ++It) {
    Constant *Func = (*It);
    PointerType *FuncType = cast<PointerType>(Func->getType());
    FunctionVector &Table = TableEntries[FuncType];

    // If this table has not been initialized yet, fill it with nulls.
    if (Table.empty())
      Table.assign(TableSize, ConstantPointerNull::get(FuncType));

    Table[FuncIndex++] = Func;
  }

  // Create a global variable for each of the function tables.
  DenseMap<PointerType*, GlobalVariable*> TableGlobals;
  for (DenseMap<PointerType*, FunctionVector>::iterator
       Iter = TableEntries.begin(), E = TableEntries.end(); Iter != E; ++Iter) {
    PointerType *FuncType = Iter->first;
    FunctionVector &Table = Iter->second;

    Constant *TableArray =
        ConstantArray::get(ArrayType::get(FuncType, TableSize), Table);
    TableGlobals[FuncType] =
        new GlobalVariable(M, TableArray->getType(), /*isConstant=*/true,
                           GlobalVariable::InternalLinkage, TableArray,
                           InternalSymName_FunctionTable);
  }

  // Iterate over all call instructions and replace integers casted to function
  // pointers with a load from the corresponding function table (because now
  // the integers are not pointers but indices).
  Constant *IndexMask = ConstantInt::get(IntPtrType, TableSize - 1);
  for (Module::iterator Func = M.begin(), EFunc = M.end();
       Func != EFunc; ++Func) {
    for (Function::iterator BB = Func->begin(), EBB = Func->end();
         BB != EBB; ++BB) {
      for (BasicBlock::iterator Inst = BB->begin(), EInst = BB->end();
           Inst != EInst; ++Inst) {
        if (CallInst *Call = dyn_cast<CallInst>(Inst)) {
          Value *Callee = Call->getCalledValue();
          if (IntToPtrInst *Cast = dyn_cast<IntToPtrInst>(Callee)) {
            Value *FuncIndex = Cast->getOperand(0);
            PointerType *FuncType = cast<PointerType>(Cast->getType());
            Constant *GlobalVar = TableGlobals[FuncType];

            Value *FuncPtr;
            if (GlobalVar) {
              Instruction *MaskedIndex =
                  BinaryOperator::CreateAnd(FuncIndex, IndexMask, "", Call);
              Value *Indexes[] = { ConstantInt::get(I32, 0), MaskedIndex };
              Instruction *TableElemPtr =
                  GetElementPtrInst::Create(GlobalVar, Indexes, "", Call);
              FuncPtr = CopyDebug(new LoadInst(TableElemPtr, "", Call), Cast);
            } else {
              // There is no function table for this signature, i.e. the module
              // does not contain a function which could be called at this site.
              // We replace the pointer with a null and put a trap in front of
              // the call because it should never be called.
              CallInst::Create(Intrinsic::getDeclaration(&M, Intrinsic::trap),
                               "", Call);
              FuncPtr = ConstantPointerNull::get(FuncType);
            }

            Call->setCalledFunction(FuncPtr);
            if (Cast->use_empty())
              Cast->eraseFromParent();
          }
        }
      }
    }
  }

  return true;
}

char SandboxIndirectCalls::ID = 0;
INITIALIZE_PASS(SandboxIndirectCalls, "minsfi-sandbox-indirect-calls",
                "Add CFI to indirect calls", false, false)

ModulePass *llvm::createSandboxIndirectCallsPass() {
  return new SandboxIndirectCalls();
}
