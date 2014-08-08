//===- SandboxMemoryAccesses.cpp - Apply SFI sandboxing to used pointers --===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass applies SFI sandboxing to all memory access instructions in the
// IR. Pointers are truncated to 32-bit integers and shifted to the 32-bit
// address subspace defined by the base address stored in the global variable
// '__sfi_memory_base' initialized at runtime.
//
// It is meant to be the next to last pass of MinSFI, followed only by a CFI
// pass. Because there is no runtime verifier, it must be trusted to correctly
// sandbox all dereferenced pointers.
//
// This pass currently assumes that the host system uses 64-bit pointers.
//
// Sandboxed instructions:
//  - load, store
//  - memcpy, memmove, memset
//  - @llvm.nacl.atomic.load.*
//  - @llvm.nacl.atomic.store.*
//  - @llvm.nacl.atomic.rmw.*
//  - @llvm.nacl.atomic.cmpxchg.*
//
// This pass fails if code contains an instruction with pointer-type operands
// not listed above, with the exception of ptrtoint needed for function
// pointers. Assumes those will be sandboxed by a CFI pass applied afterwards.
//
// The pass recognizes pointer arithmetic produced by ExpandGetElementPtr and
// reuses its final integer value to save target instructions. This optimization
// is safe only if the runtime creates a 4GB guard region after the dedicated
// memory region.
//
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/NaClAtomicIntrinsics.h"
#include "llvm/Transforms/NaCl.h"

static const char GlobalMemBaseVariableName[] = "__sfi_memory_base";

using namespace llvm;

namespace {
// This pass needs to be a ModulePass because it adds a GlobalVariable.
class SandboxMemoryAccesses : public ModulePass {
  Value *MemBaseVar;
  Type *I32;
  Type *I64;

  void sandboxPtrOperand(Instruction *Inst, unsigned int OpNum, Function &Func,
                         Value **MemBase);
  void checkDoesNotHavePointerOperands(Instruction *Inst);
  void runOnFunction(Function &Func);

 public:
  static char ID;
  SandboxMemoryAccesses() : ModulePass(ID) {
    initializeSandboxMemoryAccessesPass(*PassRegistry::getPassRegistry());
    MemBaseVar = NULL;
    I32 = I64 = NULL;
  }

  virtual bool runOnModule(Module &M);
};
}  // namespace

bool SandboxMemoryAccesses::runOnModule(Module &M) {
  I32 = Type::getInt32Ty(M.getContext());
  I64 = Type::getInt64Ty(M.getContext());

  // Create a global variable with external linkage that will hold the base
  // address of the sandbox. This variable is defined and initialized by
  // the runtime. We assume that all original global variables have been 
  // removed during the AllocateDataSegment pass.
  MemBaseVar = M.getOrInsertGlobal(GlobalMemBaseVariableName, I64);

  for (Module::iterator Func = M.begin(), E = M.end(); Func != E; ++Func)
    runOnFunction(*Func);

  return true;
}

void SandboxMemoryAccesses::sandboxPtrOperand(Instruction *Inst,
                                              unsigned int OpNum,
                                              Function &Func, Value **MemBase) {
  // Function must first acquire the sandbox memory region base from
  // the global variable. If this is the first sandboxed pointer, insert
  // the corresponding load instruction at the beginning of the function.
  if (!*MemBase) {
    Instruction *MemBaseInst = new LoadInst(MemBaseVar, "mem_base");
    Func.getEntryBlock().getInstList().push_front(MemBaseInst);
    *MemBase = MemBaseInst;
  }

  Value *Ptr = Inst->getOperand(OpNum);
  Value *Truncated = NULL, *OffsetConst = NULL;

  // The ExpandGetElementPtr pass replaces the getelementptr instruction
  // with pointer arithmetic. If we recognize that pointer arithmetic pattern
  // here, we can sandbox the pointer more efficiently than in the general
  // case below.
  //
  // The recognized pattern is:
  //   %0 = add i32 %x, <const>               ; must be positive
  //   %ptr = inttoptr i32 %0 to <type>*
  // and can be replaced with:
  //   %0 = zext i32 %x to i64
  //   %1 = add i64 %0, %mem_base
  //   %2 = add i64 %1, <const>               ; zero-extended to i64
  //   %ptr = inttoptr i64 %2 to <type>*
  //
  // Since this enables the code to access memory outside the dedicated
  // region, this is safe only if the 4GB sandbox region is followed by
  // a 4GB guard region.

  bool OptimizeGEP = false;
  Instruction *RedundantCast = NULL, *RedundantAdd = NULL;
  if (IntToPtrInst *Cast = dyn_cast<IntToPtrInst>(Ptr)) {
    if (BinaryOperator *Op = dyn_cast<BinaryOperator>(Cast->getOperand(0))) {
      if (Op->getOpcode() == Instruction::Add) {
        if (Op->getType()->isIntegerTy(32)) {
          if (ConstantInt *CI = dyn_cast<ConstantInt>(Op->getOperand(1))) {
            if (CI->getSExtValue() > 0) {
              Truncated = Op->getOperand(0);
              OffsetConst = ConstantInt::get(I64, CI->getZExtValue());
              RedundantCast = Cast;
              RedundantAdd = Op;
              OptimizeGEP = true;
            }
          }
        }
      }
    }
  }

  // If the pattern above has not been recognized, start by truncating
  // the pointer to i32.
  if (!OptimizeGEP)
    Truncated = new PtrToIntInst(Ptr, I32, "", Inst);

  // Sandbox the pointer by zero-extending it back to 64 bits, and adding
  // the memory region base.
  Instruction *Extend = new ZExtInst(Truncated, I64, "", Inst);
  Instruction *AddBase = BinaryOperator::CreateAdd(*MemBase, Extend, "", Inst);
  Instruction *AddOffset =
      OptimizeGEP ? BinaryOperator::CreateAdd(AddBase, OffsetConst, "", Inst)
                  : AddBase;
  Instruction *SandboxedPtr =
      new IntToPtrInst(AddOffset, Ptr->getType(), "", Inst);

  // Replace the pointer in the sandboxed operand
  Inst->setOperand(OpNum, SandboxedPtr);

  if (OptimizeGEP) {
    // Copy debug information
    CopyDebug(AddOffset, RedundantAdd);
    CopyDebug(SandboxedPtr, RedundantCast);

    // Remove instructions if now dead (order matters)
    if (RedundantCast->getNumUses() == 0)
      RedundantCast->eraseFromParent();
    if (RedundantAdd->getNumUses() == 0)
      RedundantAdd->eraseFromParent();
  }
}

void SandboxMemoryAccesses::checkDoesNotHavePointerOperands(Instruction *Inst) {
  bool hasPointerOperand = false;

  // Handle Call instructions separately because they always contain
  // a pointer to the target function. Integrity of calls is guaranteed by CFI.
  // This pass therefore only checks the function's arguments.
  if (CallInst *Call = dyn_cast<CallInst>(Inst)) {
    for (unsigned int I = 0, E = Call->getNumArgOperands(); I < E; ++I)
      hasPointerOperand |= Call->getArgOperand(I)->getType()->isPointerTy();
  } else {
    for (unsigned int I = 0, E = Inst->getNumOperands(); I < E; ++I)
      hasPointerOperand |= Inst->getOperand(I)->getType()->isPointerTy();
  }

  if (hasPointerOperand)
    report_fatal_error("SandboxMemoryAccesses: unexpected instruction with "
                       "pointer-type operands");
}

void SandboxMemoryAccesses::runOnFunction(Function &Func) {
  Value *MemBase = NULL;

  for (Function::iterator BB = Func.begin(), E = Func.end(); BB != E; ++BB) {
    for (BasicBlock::iterator Inst = BB->begin(), E = BB->end(); Inst != E; 
         ++Inst) {
      if (isa<LoadInst>(Inst)) {
        sandboxPtrOperand(Inst, 0, Func, &MemBase);
      } else if (isa<StoreInst>(Inst)) {
        sandboxPtrOperand(Inst, 1, Func, &MemBase);
      } else if (isa<MemCpyInst>(Inst) || isa<MemMoveInst>(Inst)) {
        sandboxPtrOperand(Inst, 0, Func, &MemBase);
        sandboxPtrOperand(Inst, 1, Func, &MemBase);
      } else if (isa<MemSetInst>(Inst)) {
        sandboxPtrOperand(Inst, 0, Func, &MemBase);
      } else if (IntrinsicInst *IntrCall = dyn_cast<IntrinsicInst>(Inst)) {
        switch (IntrCall->getIntrinsicID()) {
        case Intrinsic::nacl_atomic_load:
        case Intrinsic::nacl_atomic_cmpxchg:
          sandboxPtrOperand(IntrCall, 0, Func, &MemBase);
          break;
        case Intrinsic::nacl_atomic_store:
        case Intrinsic::nacl_atomic_rmw:
        case Intrinsic::nacl_atomic_is_lock_free:
          sandboxPtrOperand(IntrCall, 1, Func, &MemBase);
          break;
        default:
          checkDoesNotHavePointerOperands(IntrCall);
        }
      } else if (!isa<PtrToIntInst>(Inst)) {
        checkDoesNotHavePointerOperands(Inst);
      }
    }
  }
}

char SandboxMemoryAccesses::ID = 0;
INITIALIZE_PASS(SandboxMemoryAccesses, "minsfi-sandbox-memory-accesses",
                "Add SFI sandboxing to memory accesses", false, false)
