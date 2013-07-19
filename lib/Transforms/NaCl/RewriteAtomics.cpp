//===- RewriteAtomics.cpp - Stabilize instructions used for concurrency ---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass encodes atomics, volatiles and fences using NaCl intrinsics
// instead of LLVM's regular IR instructions.
//
// All of the above are transformed into one of the
// @llvm.nacl.atomic.* intrinsics.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Twine.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/NaClAtomicIntrinsics.h"
#include "llvm/InstVisitor.h"
#include "llvm/Pass.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/NaCl.h"
#include <climits>
#include <string>

using namespace llvm;

namespace {
class RewriteAtomics : public ModulePass {
public:
  static char ID; // Pass identification, replacement for typeid
  RewriteAtomics() : ModulePass(ID) {
    // This is a module pass because it may have to introduce
    // intrinsic declarations into the module and modify a global function.
    initializeRewriteAtomicsPass(*PassRegistry::getPassRegistry());
  }

  virtual bool runOnModule(Module &M);
  virtual void getAnalysisUsage(AnalysisUsage &Info) const {
    Info.addRequired<DataLayout>();
  }
};

template <class T> std::string ToStr(const T &V) {
  std::string S;
  raw_string_ostream OS(S);
  OS << const_cast<T &>(V);
  return OS.str();
}

class AtomicVisitor : public InstVisitor<AtomicVisitor> {
public:
  AtomicVisitor(Module &M, Pass &P)
      : M(M), C(M.getContext()), TD(P.getAnalysis<DataLayout>()), AI(C),
        ModifiedModule(false) {}
  ~AtomicVisitor() {}
  bool modifiedModule() const { return ModifiedModule; }

  void visitLoadInst(LoadInst &I);
  void visitStoreInst(StoreInst &I);
  void visitAtomicCmpXchgInst(AtomicCmpXchgInst &I);
  void visitAtomicRMWInst(AtomicRMWInst &I);
  void visitFenceInst(FenceInst &I);

private:
  Module &M;
  LLVMContext &C;
  const DataLayout TD;
  NaCl::AtomicIntrinsics AI;
  bool ModifiedModule;

  AtomicVisitor() LLVM_DELETED_FUNCTION;
  AtomicVisitor(const AtomicVisitor &) LLVM_DELETED_FUNCTION;
  AtomicVisitor &operator=(const AtomicVisitor &) LLVM_DELETED_FUNCTION;

  /// Create an integer constant holding a NaCl::MemoryOrder that can be
  /// passed as an argument to one of the @llvm.nacl.atomic.*
  /// intrinsics. This function may strengthen the ordering initially
  /// specified by the instruction \p I for stability purpose.
  template <class Instruction>
  ConstantInt *freezeMemoryOrder(const Instruction &I) const;

  /// Sanity-check that instruction \p I which has pointer and value
  /// parameters have matching sizes \p BitSize for the type-pointed-to
  /// and the value's type \p T.
  void checkSizeMatchesType(const Instruction &I, unsigned BitSize,
                            const Type *T) const;

  /// Verify that loads and stores are at least naturally aligned. Use
  /// byte alignment because converting to bits could truncate the
  /// value.
  void checkAlignment(const Instruction &I, unsigned ByteAlignment,
                      unsigned ByteSize) const;

  /// Create a cast before Instruction \p I from \p Src to \p Dst with \p Name.
  CastInst *createCast(Instruction &I, Value *Src, Type *Dst, Twine Name) const;

  /// Helper function which rewrites a single instruction \p I to a
  /// particular intrinsic \p ID with overloaded type \p OverloadedType,
  /// and argument list \p Args. Will perform a bitcast to the proper \p
  /// DstType, if different from \p OverloadedType.
  void replaceInstructionWithIntrinsicCall(Instruction &I, Intrinsic::ID ID,
                                           Type *DstType, Type *OverloadedType,
                                           ArrayRef<Value *> Args);

  /// Most atomics instructions deal with at least one pointer, this
  /// struct automates some of this and has generic sanity checks.
  template <class Instruction> struct PointerHelper {
    Value *P;
    Type *OriginalPET;
    Type *PET;
    unsigned BitSize;
    PointerHelper(const AtomicVisitor &AV, Instruction &I)
        : P(I.getPointerOperand()) {
      if (I.getPointerAddressSpace() != 0)
        report_fatal_error("unhandled pointer address space " +
                           Twine(I.getPointerAddressSpace()) + " for atomic: " +
                           ToStr(I));
      assert(P->getType()->isPointerTy() && "expected a pointer");
      PET = OriginalPET = P->getType()->getPointerElementType();
      BitSize = AV.TD.getTypeSizeInBits(OriginalPET);
      if (!OriginalPET->isIntegerTy()) {
        // The pointer wasn't to an integer type. We define atomics in
        // terms of integers, so bitcast the pointer to an integer of
        // the proper width.
        Type *IntNPtr = Type::getIntNPtrTy(AV.C, BitSize);
        P = AV.createCast(I, P, IntNPtr, P->getName() + ".cast");
        PET = P->getType()->getPointerElementType();
      }
      AV.checkSizeMatchesType(I, BitSize, PET);
    }
  };
};
}

char RewriteAtomics::ID = 0;
INITIALIZE_PASS(RewriteAtomics, "nacl-rewrite-atomics",
                "rewrite atomics, volatiles and fences into stable "
                "@llvm.nacl.atomics.* intrinsics",
                false, false)

bool RewriteAtomics::runOnModule(Module &M) {
  AtomicVisitor AV(M, *this);
  AV.visit(M);
  return AV.modifiedModule();
}

template <class Instruction>
ConstantInt *AtomicVisitor::freezeMemoryOrder(const Instruction &I) const {
  NaCl::MemoryOrder AO = NaCl::MemoryOrderInvalid;

  // TODO Volatile load/store are promoted to sequentially consistent
  //      for now. We could do something weaker.
  if (const LoadInst *L = dyn_cast<LoadInst>(&I)) {
    if (L->isVolatile())
      AO = NaCl::MemoryOrderSequentiallyConsistent;
  } else if (const StoreInst *S = dyn_cast<StoreInst>(&I)) {
    if (S->isVolatile())
      AO = NaCl::MemoryOrderSequentiallyConsistent;
  }

  if (AO == NaCl::MemoryOrderInvalid) {
    switch (I.getOrdering()) {
    default:
    case NotAtomic: llvm_unreachable("unexpected memory order");
    // Monotonic is a strict superset of Unordered. Both can therefore
    // map to Relaxed ordering, which is in the C11/C++11 standard.
    case Unordered: AO = NaCl::MemoryOrderRelaxed; break;
    case Monotonic: AO = NaCl::MemoryOrderRelaxed; break;
    // TODO Consume is currently unspecified by LLVM's internal IR.
    case Acquire: AO = NaCl::MemoryOrderAcquire; break;
    case Release: AO = NaCl::MemoryOrderRelease; break;
    case AcquireRelease: AO = NaCl::MemoryOrderAcquireRelease; break;
    case SequentiallyConsistent:
      AO = NaCl::MemoryOrderSequentiallyConsistent; break;
    }
  }

  // TODO For now only sequential consistency is allowed.
  AO = NaCl::MemoryOrderSequentiallyConsistent;

  return ConstantInt::get(Type::getInt32Ty(C), AO);
}

void AtomicVisitor::checkSizeMatchesType(const Instruction &I, unsigned BitSize,
                                         const Type *T) const {
  Type *IntType = Type::getIntNTy(C, BitSize);
  if (IntType && T == IntType)
    return;
  report_fatal_error("unsupported atomic type " + ToStr(*T) + " of size " +
                     Twine(BitSize) + " bits in: " + ToStr(I));
}

void AtomicVisitor::checkAlignment(const Instruction &I, unsigned ByteAlignment,
                                   unsigned ByteSize) const {
  if (ByteAlignment < ByteSize)
    report_fatal_error("atomic load/store must be at least naturally aligned, "
                       "got " +
                       Twine(ByteAlignment) + ", bytes expected at least " +
                       Twine(ByteSize) + " bytes, in: " + ToStr(I));
}

CastInst *AtomicVisitor::createCast(Instruction &I, Value *Src, Type *Dst,
                                    Twine Name) const {
  Type *SrcT = Src->getType();
  Instruction::CastOps Op = SrcT->isIntegerTy() && Dst->isPointerTy()
                                ? Instruction::IntToPtr
                                : SrcT->isPointerTy() && Dst->isIntegerTy()
                                      ? Instruction::PtrToInt
                                      : Instruction::BitCast;
  if (!CastInst::castIsValid(Op, Src, Dst))
    report_fatal_error("cannot emit atomic instruction while converting type " +
                       ToStr(*SrcT) + " to " + ToStr(*Dst) + " for " + Name +
                       " in " + ToStr(I));
  return CastInst::Create(Op, Src, Dst, Name, &I);
}

void AtomicVisitor::replaceInstructionWithIntrinsicCall(
    Instruction &I, Intrinsic::ID ID, Type *DstType, Type *OverloadedType,
    ArrayRef<Value *> Args) {
  std::string Name(I.getName());
  Function *F = AI.find(ID, OverloadedType)->getDeclaration(&M);
  CallInst *Call = CallInst::Create(F, Args, "", &I);
  Instruction *Res = Call;
  if (!Call->getType()->isVoidTy() && DstType != OverloadedType) {
    // The call returns a value which needs to be cast to a non-integer.
    Res = createCast(I, Call, DstType, Name + ".cast");
    Res->setDebugLoc(I.getDebugLoc());
  }
  Call->setDebugLoc(I.getDebugLoc());
  I.replaceAllUsesWith(Res);
  I.eraseFromParent();
  Call->setName(Name);
  ModifiedModule = true;
}

/// %res = load {atomic|volatile} T* %ptr memory_order, align sizeof(T)
/// becomes:
/// %res = call T @llvm.nacl.atomic.load.i<size>(%ptr, memory_order)
void AtomicVisitor::visitLoadInst(LoadInst &I) {
  if (I.isSimple())
    return;
  PointerHelper<LoadInst> PH(*this, I);
  checkAlignment(I, I.getAlignment(), PH.BitSize / CHAR_BIT);
  Value *Args[] = { PH.P, freezeMemoryOrder(I) };
  replaceInstructionWithIntrinsicCall(I, Intrinsic::nacl_atomic_load,
                                      PH.OriginalPET, PH.PET, Args);
}

/// store {atomic|volatile} T %val, T* %ptr memory_order, align sizeof(T)
/// becomes:
/// call void @llvm.nacl.atomic.store.i<size>(%val, %ptr, memory_order)
void AtomicVisitor::visitStoreInst(StoreInst &I) {
  if (I.isSimple())
    return;
  PointerHelper<StoreInst> PH(*this, I);
  checkAlignment(I, I.getAlignment(), PH.BitSize / CHAR_BIT);
  Value *V = I.getValueOperand();
  if (!V->getType()->isIntegerTy()) {
    // The store isn't of an integer type. We define atomics in terms of
    // integers, so bitcast the value to store to an integer of the
    // proper width.
    CastInst *Cast = createCast(I, V, Type::getIntNTy(C, PH.BitSize),
                                V->getName() + ".cast");
    Cast->setDebugLoc(I.getDebugLoc());
    V = Cast;
  }
  checkSizeMatchesType(I, PH.BitSize, V->getType());
  Value *Args[] = { V, PH.P, freezeMemoryOrder(I) };
  replaceInstructionWithIntrinsicCall(I, Intrinsic::nacl_atomic_store,
                                      PH.OriginalPET, PH.PET, Args);
}

/// %res = atomicrmw OP T* %ptr, T %val memory_order
/// becomes:
/// %res = call T @llvm.nacl.atomic.rmw.i<size>(OP, %ptr, %val, memory_order)
void AtomicVisitor::visitAtomicRMWInst(AtomicRMWInst &I) {
  NaCl::AtomicRMWOperation Op;
  switch (I.getOperation()) {
  default: report_fatal_error("unsupported atomicrmw operation: " + ToStr(I));
  case AtomicRMWInst::Add: Op = NaCl::AtomicAdd; break;
  case AtomicRMWInst::Sub: Op = NaCl::AtomicSub; break;
  case AtomicRMWInst::And: Op = NaCl::AtomicAnd; break;
  case AtomicRMWInst::Or:  Op = NaCl::AtomicOr;  break;
  case AtomicRMWInst::Xor: Op = NaCl::AtomicXor; break;
  case AtomicRMWInst::Xchg: Op = NaCl::AtomicExchange; break;
  }
  PointerHelper<AtomicRMWInst> PH(*this, I);
  checkSizeMatchesType(I, PH.BitSize, I.getValOperand()->getType());
  Value *Args[] = { ConstantInt::get(Type::getInt32Ty(C), Op), PH.P,
                    I.getValOperand(), freezeMemoryOrder(I) };
  replaceInstructionWithIntrinsicCall(I, Intrinsic::nacl_atomic_rmw,
                                      PH.OriginalPET, PH.PET, Args);
}

/// %res = cmpxchg T* %ptr, T %old, T %new memory_order
/// becomes:
/// %res = call T @llvm.nacl.atomic.cmpxchg.i<size>(
///     %object, %expected, %desired, memory_order_success,
///     memory_order_failure)
void AtomicVisitor::visitAtomicCmpXchgInst(AtomicCmpXchgInst &I) {
  PointerHelper<AtomicCmpXchgInst> PH(*this, I);
  checkSizeMatchesType(I, PH.BitSize, I.getCompareOperand()->getType());
  checkSizeMatchesType(I, PH.BitSize, I.getNewValOperand()->getType());
  // TODO LLVM currently doesn't support specifying separate memory
  //      orders for compare exchange's success and failure cases: LLVM
  //      IR implicitly drops the Release part of the specified memory
  //      order on failure.
  Value *Args[] = { PH.P, I.getCompareOperand(), I.getNewValOperand(),
                    freezeMemoryOrder(I), freezeMemoryOrder(I) };
  replaceInstructionWithIntrinsicCall(I, Intrinsic::nacl_atomic_cmpxchg,
                                      PH.OriginalPET, PH.PET, Args);
}

/// fence memory_order
/// becomes:
/// call void @llvm.nacl.atomic.fence(memory_order)
void AtomicVisitor::visitFenceInst(FenceInst &I) {
  Type *T = Type::getInt32Ty(C); // Fences aren't overloaded on type.
  Value *Args[] = { freezeMemoryOrder(I) };
  replaceInstructionWithIntrinsicCall(I, Intrinsic::nacl_atomic_fence, T, T,
                                      Args);
}

ModulePass *llvm::createRewriteAtomicsPass() { return new RewriteAtomics(); }
