//===----- ExpandAllocas.cpp - Allocate memory on the untrusted stack -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Code sandboxed with MinSFI cannot access the execution stack directly
// because the stack lies outside of its address subspace, which prevents it
// from using memory allocated with the alloca instruction. This pass therefore
// replaces allocas with memory allocation on a separate stack at a fixed
// location inside the designated memory region.
//
// The new stack does not have to be trusted as it is only used for memory
// allocation inside the sandbox. The call and ret instructions still operate
// on the native stack, preventing manipulation with the return address or
// callee-saved registers.
//
// This pass also replaces the @llvm.stacksave and @llvm.stackrestore
// intrinsics which would otherwise allow access to the native stack pointer.
// Instead, they are expanded out and save/restore the current untrusted stack
// pointer.
//
// When a function is invoked, the current untrusted stack pointer is obtained
// from the "__sfi_stack_ptr" global variable (internal to the module). The
// function then keeps track of the current value of the stack pointer, but
// must update the global variable prior to any function calls and restore the
// initial value before it returns.
//
// If an alloca requests alignment greater than 1, the untrusted stack pointer
// is aligned accordingly. However, the alignment is applied before the address
// is sandboxed and therefore the runtime must guarantee that the base address
// of the sandbox is aligned to at least 2^29 bytes (=512MB), which is the
// maximum alignment supported by LLVM.
//
// Possible optimizations:
//  - accumulate constant-sized allocas to reduce the number of stores
//    into the global stack pointer variable
//  - remove stores into the global pointer if the respective values never
//    reach a function call
//  - align frame to 16 bytes
//
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/MinSFI.h"
#include "llvm/Transforms/NaCl.h"

static const char StackPtrVariableName[] = "__sfi_stack_ptr";

using namespace llvm;

namespace {
// ExpandAllocas needs to be a ModulePass because it adds a GlobalVariable.
class ExpandAllocas : public ModulePass {
  GlobalVariable *StackPtrVar;
  Type *IntPtrType, *I8Ptr;

  bool runOnFunction(Function &Func);

public:
  static char ID;
  ExpandAllocas() : ModulePass(ID), StackPtrVar(NULL), IntPtrType(NULL),
                    I8Ptr(NULL) {
    initializeExpandAllocasPass(*PassRegistry::getPassRegistry());
  }

  virtual bool runOnModule(Module &M);
};
}  // namespace

bool ExpandAllocas::runOnModule(Module &M) {
  DataLayout DL(&M);
  IntPtrType = DL.getIntPtrType(M.getContext());
  I8Ptr = Type::getInt8PtrTy(M.getContext());

  // The stack bottom is positioned arbitrarily towards the end of the address
  // subspace. This has no effect on the alignment of memory allocated on the
  // untrusted stack.
  uint64_t StackPtrInitialValue = minsfi::GetAddressSubspaceSize() - 16;
  StackPtrVar = new GlobalVariable(M, IntPtrType, /*isConstant=*/false,
                                   GlobalVariable::InternalLinkage,
                                   ConstantInt::get(IntPtrType,
                                                    StackPtrInitialValue),
                                   StackPtrVariableName);

  bool Changed = false;
  for (Module::iterator Func = M.begin(), E = M.end(); Func != E; ++Func)
    Changed |= runOnFunction(*Func);

  return Changed;
}

static inline void replaceWithPointer(Instruction *OrigInst, Value *IntPtr,
                                      SmallVectorImpl<Instruction*> &Dead) {
  Instruction *NewInst =
      new IntToPtrInst(IntPtr, OrigInst->getType(), "", OrigInst);
  NewInst->takeName(OrigInst);
  OrigInst->replaceAllUsesWith(NewInst);
  CopyDebug(NewInst, OrigInst);
  Dead.push_back(OrigInst);
}

static inline Instruction *getBBStackPtr(BasicBlock *BB) {
  return BB->getInstList().begin();
}

bool ExpandAllocas::runOnFunction(Function &Func) {
  // Do an initial scan of the entire function body. Check whether it contains
  // instructions which we want to operate on the untrusted stack and return
  // if there aren't any. Also check whether it contains any function calls.
  // If not, we will not have to update the global stack pointer variable.
  bool NoUntrustedStackOps = true;
  bool MustUpdateStackPtrGlobal = false;
  for (Function::iterator BB = Func.begin(), E = Func.end(); BB != E; ++BB) {
    for (BasicBlock::iterator Inst = BB->begin(), E = BB->end(); Inst != E;
         ++Inst) {
      NoUntrustedStackOps &= !isa<AllocaInst>(Inst);
      if (CallInst *Call = dyn_cast<CallInst>(Inst)) {
        if (isa<IntrinsicInst>(Call)) {
          unsigned IntrinsicID = Call->getCalledFunction()->getIntrinsicID();
          bool IsNotStackIntr = IntrinsicID != Intrinsic::stacksave &&
                                IntrinsicID != Intrinsic::stackrestore;
          NoUntrustedStackOps &= IsNotStackIntr;
          MustUpdateStackPtrGlobal |= IsNotStackIntr;
        } else {
          MustUpdateStackPtrGlobal = true;
        }
      }
    }
  }

  if (NoUntrustedStackOps)
    return false;

  SmallVector<Instruction *, 10> DeadInsts;
  Instruction *InitialStackPtr = new LoadInst(StackPtrVar, "frame_top");

  // First, we insert a new instruction at the beginning of each basic block,
  // which will represent the value of the stack pointer at that point. For
  // the entry block, this is the value of the global stack pointer variable.
  // Other blocks are initialized with empty phi nodes which we will later
  // fill with the values carried over from the respective predecessors.
  BasicBlock *EntryBB = &Func.getEntryBlock();
  for (Function::iterator BB = Func.begin(), E = Func.end(); BB != E; ++BB)
    BB->getInstList().push_front(
        ((BasicBlock*)BB == EntryBB) ? InitialStackPtr
                                     : PHINode::Create(IntPtrType, 2, ""));

  // Now iterate over the instructions and expand out the untrusted stack
  // operations. Allocas are replaced with pointer arithmetic that pushes
  // the untrusted stack pointer and updates the global stack pointer variable
  // if the initial scan identified function calls in the code.
  // The @llvm.stacksave intrinsic returns the latest value of the stack
  // pointer, and the @llvm.stackrestore overwrites it and potentially updates
  // the global variable. If needed, return instructions are prepended with
  // a store which restores the initial value of the global variable.
  // At the end of each basic block, the last value of the untrusted stack
  // pointer is inserted into the phi node at the beginning of each successor
  // block.
  for (Function::iterator BB = Func.begin(), EBB = Func.end(); BB != EBB;
       ++BB) {
    Instruction *LastTop = getBBStackPtr(BB);
    for (BasicBlock::iterator Inst = BB->begin(), EInst = BB->end(); 
         Inst != EInst; ++Inst) {
      if (AllocaInst *Alloca = dyn_cast<AllocaInst>(Inst)) {
        Value *SizeOp = Alloca->getArraySize();
        unsigned Alignment = Alloca->getAlignment();
        assert(Alloca->getType() == I8Ptr);
        assert(SizeOp->getType()->isIntegerTy(32));
        assert(Alignment <= (1 << 29));  // 512MB

        LastTop = BinaryOperator::CreateSub(LastTop, SizeOp, "", Alloca);
        if (Alignment > 1)
          LastTop = BinaryOperator::CreateAnd(LastTop,
                                              ConstantInt::get(IntPtrType,
                                                               -Alignment),
                                              "", Alloca);
        if (MustUpdateStackPtrGlobal)
          new StoreInst(LastTop, StackPtrVar, Alloca);
        replaceWithPointer(Alloca, LastTop, DeadInsts);
      } else if (IntrinsicInst *Intr = dyn_cast<IntrinsicInst>(Inst)) {
        if (Intr->getIntrinsicID() == Intrinsic::stacksave) {
          replaceWithPointer(Intr, LastTop, DeadInsts);
        } else if (Intr->getIntrinsicID() == Intrinsic::stackrestore) {
          Value *NewStackPtr = Intr->getArgOperand(0);
          LastTop = new PtrToIntInst(NewStackPtr, IntPtrType, "", Intr);
          if (MustUpdateStackPtrGlobal)
            new StoreInst(LastTop, StackPtrVar, Intr);
          CopyDebug(LastTop, Intr);
          DeadInsts.push_back(Intr);
        }
      } else if (ReturnInst *Return = dyn_cast<ReturnInst>(Inst)) {
        if (MustUpdateStackPtrGlobal)
          new StoreInst(InitialStackPtr, StackPtrVar, Return);
      }
    }

    // Insert the final frame top value into all successor phi nodes.
    TerminatorInst *Term = BB->getTerminator();
    for (unsigned int I = 0; I < Term->getNumSuccessors(); ++I) {
      PHINode *Succ = cast<PHINode>(getBBStackPtr(Term->getSuccessor(I)));
      Succ->addIncoming(LastTop, BB);
    }
  }

  // Delete the replaced instructions.
  for (SmallVectorImpl<Instruction *>::const_iterator Inst = DeadInsts.begin(),
       E = DeadInsts.end(); Inst != E; ++Inst)
    (*Inst)->eraseFromParent();

  return true;
}

char ExpandAllocas::ID = 0;
INITIALIZE_PASS(ExpandAllocas, "minsfi-expand-allocas",
                "Expand allocas to allocate memory on an untrusted stack",
                false, false)
