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
// The stack pointer is initialized in the entry function of the module, the
// _start_minsfi function. The runtime is expected to copy the arguments
// (a NULL-terminated integer array) at the end of the allocated memory region,
// i.e. at the bottom of the untrusted stack, and pass the pointer to the array
// to the entry function. The sandboxed code is then expected to use the
// pointer not only to access its arguments but also as the initial value of
// its stack pointer and to grow the stack backwards.
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
#include "llvm/Support/MathExtras.h"
#include "llvm/Transforms/MinSFI.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

static const char InternalSymName_StackPointer[] = "__sfi_stack_ptr";

namespace {
// ExpandAllocas needs to be a ModulePass because it adds a GlobalVariable.
class ExpandAllocas : public ModulePass {
  GlobalVariable *StackPtrVar;
  Type *IntPtrType, *I8Ptr;

  void runOnFunction(Function &Func);
  void insertStackPtrInit(Module &M);

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

  // Create the stack pointer global variable. We are forced to give it some
  // initial value, but it will be initialized at runtime.
  StackPtrVar = new GlobalVariable(M, IntPtrType, /*isConstant=*/false,
                                   GlobalVariable::InternalLinkage,
                                   ConstantInt::get(IntPtrType, 0),
                                   InternalSymName_StackPointer);

  for (Module::iterator Func = M.begin(), E = M.end(); Func != E; ++Func)
    runOnFunction(*Func);

  insertStackPtrInit(M);

  return true;
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

void ExpandAllocas::runOnFunction(Function &Func) {
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
    return;

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
}

void ExpandAllocas::insertStackPtrInit(Module &M) {
  Function *EntryFunction = M.getFunction(minsfi::EntryFunctionName);
  if (!EntryFunction)
    report_fatal_error("ExpandAllocas: Module does not have an entry function");

  // Check the signature of the entry function.
  Function::ArgumentListType &Args = EntryFunction->getArgumentList();
  if (Args.size() != 1 || Args.front().getType() != IntPtrType) {
    report_fatal_error(std::string("ExpandAllocas: Invalid signature of ") +
                       minsfi::EntryFunctionName);
  }

  // Insert a store instruction which saves the value of the first argument
  // to the stack pointer global variable.
  new StoreInst(&Args.front(), StackPtrVar,
                EntryFunction->getEntryBlock().getFirstInsertionPt());
}

char ExpandAllocas::ID = 0;
INITIALIZE_PASS(ExpandAllocas, "minsfi-expand-allocas",
                "Expand allocas to allocate memory on an untrusted stack",
                false, false)

ModulePass *llvm::createExpandAllocasPass() {
  return new ExpandAllocas();
}
