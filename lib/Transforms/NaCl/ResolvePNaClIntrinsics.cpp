//===- ResolvePNaClIntrinsics.cpp - Resolve calls to PNaCl intrinsics ----====//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass resolves calls to PNaCl stable bitcode intrinsics. It is
// normally run in the PNaCl translator.
//
// Running AddPNaClExternalDeclsPass is a precondition for running this
// pass. They are separate because one is a ModulePass and the other is
// a FunctionPass.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Triple.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/NaClAtomicIntrinsics.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Transforms/NaCl.h"
#include "llvm/Transforms/Utils/Local.h"
#if defined(__pnacl__)
#include "native_client/src/untrusted/nacl/pnacl.h"
#endif

using namespace llvm;

namespace {
class ResolvePNaClIntrinsics : public FunctionPass {
public:
  ResolvePNaClIntrinsics() : FunctionPass(ID) {
    initializeResolvePNaClIntrinsicsPass(*PassRegistry::getPassRegistry());
  }

  static char ID;
  virtual bool runOnFunction(Function &F);

  /// Interface specifying how intrinsic calls should be resolved. Each
  /// intrinsic call handled by the implementor will be visited by the
  /// doResolve method.
  class CallResolver {
  public:
    /// Called once per \p Call to the intrinsic in the module.
    /// Returns true if the Function was changed.
    bool resolve(IntrinsicInst *Call) {
      // To be a well-behaving FunctionPass, don't touch uses in other
      // functions. These will be handled when the pass manager gets to
      // those functions.
      if (Call->getParent()->getParent() == &F)
        return doResolve(Call);
      return false;
    }
    Function *getDeclaration() const { return doGetDeclaration(); }
    std::string getName() { return Intrinsic::getName(IntrinsicID); }

  protected:
    Function &F;
    Module *M;
    Intrinsic::ID IntrinsicID;

    CallResolver(Function &F, Intrinsic::ID IntrinsicID)
        : F(F), M(F.getParent()), IntrinsicID(IntrinsicID) {}
    virtual ~CallResolver() {}

    /// The following pure virtual methods must be defined by
    /// implementors, and will be called once per intrinsic call.
    /// NOTE: doGetDeclaration() should only "get" the intrinsic declaration
    /// and not *add* decls to the module. Declarations should be added
    /// up front by the AddPNaClExternalDecls module pass.
    virtual Function *doGetDeclaration() const = 0;
    /// Returns true if the Function was changed.
    virtual bool doResolve(IntrinsicInst *Call) = 0;

  private:
    CallResolver(const CallResolver &) LLVM_DELETED_FUNCTION;
    CallResolver &operator=(const CallResolver &) LLVM_DELETED_FUNCTION;
  };

private:
  /// Visit all calls matching the \p Resolver's declaration, and invoke
  /// the CallResolver methods on each of them.
  bool visitCalls(CallResolver &Resolver);
};

/// Rewrite intrinsic calls to another function.
class IntrinsicCallToFunctionCall :
    public ResolvePNaClIntrinsics::CallResolver {
public:
  IntrinsicCallToFunctionCall(Function &F, Intrinsic::ID IntrinsicID,
                              const char *TargetFunctionName)
      : CallResolver(F, IntrinsicID),
        TargetFunction(M->getFunction(TargetFunctionName)) {
    // Expect to find the target function for this intrinsic already
    // declared, even if it is never used.
    if (!TargetFunction)
      report_fatal_error(std::string(
          "Expected to find external declaration of ") + TargetFunctionName);
  }
  virtual ~IntrinsicCallToFunctionCall() {}

private:
  Function *TargetFunction;

  virtual Function *doGetDeclaration() const {
    return Intrinsic::getDeclaration(M, IntrinsicID);
  }

  virtual bool doResolve(IntrinsicInst *Call) {
    Call->setCalledFunction(TargetFunction);
    if (IntrinsicID == Intrinsic::nacl_setjmp) {
      // The "returns_twice" attribute is required for correctness,
      // otherwise the backend will reuse stack slots in a way that is
      // incorrect for setjmp().  See:
      // https://code.google.com/p/nativeclient/issues/detail?id=3733
      Call->setCanReturnTwice();
    }
    return true;
  }

  IntrinsicCallToFunctionCall(const IntrinsicCallToFunctionCall &)
      LLVM_DELETED_FUNCTION;
  IntrinsicCallToFunctionCall &operator=(const IntrinsicCallToFunctionCall &)
      LLVM_DELETED_FUNCTION;
};

/// Rewrite intrinsic calls to a constant whose value is determined by a
/// functor. This functor is called once per Call, and returns a
/// Constant that should replace the Call.
template <class Callable>
class ConstantCallResolver : public ResolvePNaClIntrinsics::CallResolver {
public:
  ConstantCallResolver(Function &F, Intrinsic::ID IntrinsicID,
                       Callable Functor)
      : CallResolver(F, IntrinsicID), Functor(Functor) {}
  virtual ~ConstantCallResolver() {}

private:
  Callable Functor;

  virtual Function *doGetDeclaration() const {
    return Intrinsic::getDeclaration(M, IntrinsicID);
  }

  virtual bool doResolve(IntrinsicInst *Call) {
    Constant *C = Functor(Call);
    Call->replaceAllUsesWith(C);
    Call->eraseFromParent();
    return true;
  }

  ConstantCallResolver(const ConstantCallResolver &) LLVM_DELETED_FUNCTION;
  ConstantCallResolver &operator=(const ConstantCallResolver &)
      LLVM_DELETED_FUNCTION;
};

/// Resolve __nacl_atomic_is_lock_free to true/false at translation
/// time. PNaCl's currently supported platforms all support lock-free
/// atomics at byte sizes {1,2,4,8} except for MIPS arch that supports
/// lock-free atomics at byte sizes {1,2,4}, and the alignment of the
/// pointer is always expected to be natural (as guaranteed by C11 and
/// C++11). PNaCl's Module-level ABI verification checks that the byte
/// size is constant and in {1,2,4,8}.
struct IsLockFreeToConstant {
  Constant *operator()(CallInst *Call) {
    uint64_t MaxLockFreeByteSize = 8;
    const APInt &ByteSize =
        cast<Constant>(Call->getOperand(0))->getUniqueInteger();

#   if defined(__pnacl__)
    switch (__builtin_nacl_target_arch()) {
    case PnaclTargetArchitectureX86_32:
    case PnaclTargetArchitectureX86_64:
    case PnaclTargetArchitectureARM_32:
      break;
    case PnaclTargetArchitectureMips_32:
      MaxLockFreeByteSize = 4;
      break;
    default:
      report_fatal_error("Unhandled arch from __builtin_nacl_target_arch()");
    }
#   elif defined(__i386__) || defined(__x86_64__) || defined(__arm__)
    // Continue.
#   elif defined(__mips__)
    MaxLockFreeByteSize = 4;
#   else
#     error "Unknown architecture"
#   endif

    bool IsLockFree = ByteSize.ule(MaxLockFreeByteSize);
    Constant *C = ConstantInt::get(Call->getType(), IsLockFree);
    return C;
  }
};

/// Rewrite atomic intrinsics to LLVM IR instructions.
class AtomicCallResolver : public ResolvePNaClIntrinsics::CallResolver {
public:
  AtomicCallResolver(Function &F,
                     const NaCl::AtomicIntrinsics::AtomicIntrinsic *I)
      : CallResolver(F, I->ID), I(I) {}
  virtual ~AtomicCallResolver() {}

private:
  const NaCl::AtomicIntrinsics::AtomicIntrinsic *I;

  virtual Function *doGetDeclaration() const { return I->getDeclaration(M); }

  virtual bool doResolve(IntrinsicInst *Call) {
    // Assume the @llvm.nacl.atomic.* intrinsics follow the PNaCl ABI:
    // this should have been checked by the verifier.
    bool isVolatile = false;
    SynchronizationScope SS = CrossThread;
    Instruction *I;
    SmallVector<Instruction *, 16> MaybeDead;

    switch (Call->getIntrinsicID()) {
    default:
      llvm_unreachable("unknown atomic intrinsic");
    case Intrinsic::nacl_atomic_load:
      I = new LoadInst(Call->getArgOperand(0), "", isVolatile,
                       alignmentFromPointer(Call->getArgOperand(0)),
                       thawMemoryOrder(Call->getArgOperand(1)), SS, Call);
      break;
    case Intrinsic::nacl_atomic_store:
      I = new StoreInst(Call->getArgOperand(0), Call->getArgOperand(1),
                        isVolatile,
                        alignmentFromPointer(Call->getArgOperand(1)),
                        thawMemoryOrder(Call->getArgOperand(2)), SS, Call);
      break;
    case Intrinsic::nacl_atomic_rmw:
      if (needsX8632HackFor16BitAtomics(cast<PointerType>(
              Call->getArgOperand(1)->getType())->getElementType())) {
        // TODO(jfb) Remove this hack. See below.
        atomic16BitX8632Hack(Call, false, Call->getArgOperand(1),
                             Call->getArgOperand(2), Call->getArgOperand(0),
                             NULL);
        return true;
      }
      I = new AtomicRMWInst(thawRMWOperation(Call->getArgOperand(0)),
                            Call->getArgOperand(1), Call->getArgOperand(2),
                            thawMemoryOrder(Call->getArgOperand(3)), SS, Call);
      break;
    case Intrinsic::nacl_atomic_cmpxchg:
      if (needsX8632HackFor16BitAtomics(cast<PointerType>(
              Call->getArgOperand(0)->getType())->getElementType())) {
        // TODO(jfb) Remove this hack. See below.
        atomic16BitX8632Hack(Call, true, Call->getArgOperand(0),
                             Call->getArgOperand(2), NULL,
                             Call->getArgOperand(1));
        return true;
      }
      I = new AtomicCmpXchgInst(
          Call->getArgOperand(0), Call->getArgOperand(1),
          Call->getArgOperand(2), thawMemoryOrder(Call->getArgOperand(3)),
          thawMemoryOrder(Call->getArgOperand(4)), SS, Call);

      // cmpxchg returns struct { T loaded, i1 success } whereas the PNaCl
      // intrinsic only returns the loaded value. The Call can't simply be
      // replaced. Identify loaded+success structs that can be replaced by the
      // cmxpchg's returned struct.
      {
        Instruction *Loaded = nullptr;
        Instruction *Success = nullptr;
        for (User *CallUser : Call->users()) {
          if (auto ICmp = dyn_cast<ICmpInst>(CallUser)) {
            // Identify comparisons for cmpxchg's success.
            if (ICmp->getPredicate() != CmpInst::ICMP_EQ)
              continue;
            Value *LHS = ICmp->getOperand(0);
            Value *RHS = ICmp->getOperand(1);
            Value *Old = I->getOperand(1);
            if (RHS != Old && LHS != Old) // Call is either RHS or LHS.
              continue; // The comparison isn't checking for cmpxchg's success.

            // Recognize the pattern creating struct { T loaded, i1 success }:
            // it can be replaced by cmpxchg's result.
            for (User *InsUser : ICmp->users()) {
              if (!isa<Instruction>(InsUser) ||
                  cast<Instruction>(InsUser)->getParent() != Call->getParent())
                continue; // Different basic blocks, don't be clever.
              auto Ins = dyn_cast<InsertValueInst>(InsUser);
              if (!Ins)
                continue;
              auto InsTy = dyn_cast<StructType>(Ins->getType());
              if (!InsTy)
                continue;
              if (!InsTy->isLayoutIdentical(cast<StructType>(I->getType())))
                continue; // Not a struct { T loaded, i1 success }.
              if (Ins->getNumIndices() != 1 || Ins->getIndices()[0] != 1)
                continue; // Not an insert { T, i1 } %something, %success, 1.
              auto TIns = dyn_cast<InsertValueInst>(Ins->getAggregateOperand());
              if (!TIns)
                continue; // T wasn't inserted into the struct, don't be clever.
              if (!isa<UndefValue>(TIns->getAggregateOperand()))
                continue; // Not an insert into an undef value, don't be clever.
              if (TIns->getInsertedValueOperand() != Call)
                continue; // Not inserting the loaded value.
              if (TIns->getNumIndices() != 1 || TIns->getIndices()[0] != 0)
                continue; // Not an insert { T, i1 } undef, %loaded, 0.
              // Hooray! This is the struct you're looking for.

              // Keep track of values extracted from the struct, instead of
              // recreating them.
              for (User *StructUser : Ins->users()) {
                if (auto Extract = dyn_cast<ExtractValueInst>(StructUser)) {
                  MaybeDead.push_back(Extract);
                  if (!Loaded && Extract->getIndices()[0] == 0) {
                    Loaded = cast<Instruction>(StructUser);
                    Loaded->moveBefore(Call);
                  } else if (!Success && Extract->getIndices()[0] == 1) {
                    Success = cast<Instruction>(StructUser);
                    Success->moveBefore(Call);
                  }
                }
              }

              MaybeDead.push_back(Ins);
              MaybeDead.push_back(TIns);
              Ins->replaceAllUsesWith(I);
            }

            MaybeDead.push_back(ICmp);
            if (!Success)
              Success = ExtractValueInst::Create(I, 1, "success", Call);
            ICmp->replaceAllUsesWith(Success);
          }
        }

        // Clean up remaining uses of the loaded value, if any. Later code will
        // try to replace Call with I, make sure the types match.
        if (Call->hasNUsesOrMore(1)) {
          if (!Loaded)
            Loaded = ExtractValueInst::Create(I, 0, "loaded", Call);
          I = Loaded;
        } else {
          I = nullptr;
        }

        if (Loaded)
          MaybeDead.push_back(Loaded);
        if (Success)
          MaybeDead.push_back(Success);
      }
      break;
    case Intrinsic::nacl_atomic_fence:
      I = new FenceInst(M->getContext(),
                        thawMemoryOrder(Call->getArgOperand(0)), SS, Call);
      break;
    case Intrinsic::nacl_atomic_fence_all: {
      FunctionType *FTy =
          FunctionType::get(Type::getVoidTy(M->getContext()), false);
      std::string AsmString; // Empty.
      std::string Constraints("~{memory}");
      bool HasSideEffect = true;
      CallInst *Asm = CallInst::Create(
          InlineAsm::get(FTy, AsmString, Constraints, HasSideEffect), "", Call);
      Asm->setDebugLoc(Call->getDebugLoc());
      I = new FenceInst(M->getContext(), SequentiallyConsistent, SS, Asm);
      Asm = CallInst::Create(
          InlineAsm::get(FTy, AsmString, Constraints, HasSideEffect), "", I);
      Asm->setDebugLoc(Call->getDebugLoc());
    } break;
    }

    if (I) {
      I->setName(Call->getName());
      I->setDebugLoc(Call->getDebugLoc());
      Call->replaceAllUsesWith(I);
    }
    Call->eraseFromParent();

    // Remove dead code.
    for (auto Kill : MaybeDead)
      if (isInstructionTriviallyDead(Kill))
        Kill->eraseFromParent();

    return true;
  }

  unsigned alignmentFromPointer(const Value *Ptr) const {
    const PointerType *PtrType = cast<PointerType>(Ptr->getType());
    unsigned BitWidth = PtrType->getElementType()->getIntegerBitWidth();
    return BitWidth / 8;
  }

  AtomicOrdering thawMemoryOrder(const Value *MemoryOrder) const {
    NaCl::MemoryOrder MO = (NaCl::MemoryOrder)
        cast<Constant>(MemoryOrder)->getUniqueInteger().getLimitedValue();
    switch (MO) {
    // Only valid values should pass validation.
    default: llvm_unreachable("unknown memory order");
    case NaCl::MemoryOrderRelaxed: return Monotonic;
    // TODO Consume is unspecified by LLVM's internal IR.
    case NaCl::MemoryOrderConsume: return SequentiallyConsistent;
    case NaCl::MemoryOrderAcquire: return Acquire;
    case NaCl::MemoryOrderRelease: return Release;
    case NaCl::MemoryOrderAcquireRelease: return AcquireRelease;
    case NaCl::MemoryOrderSequentiallyConsistent: return SequentiallyConsistent;
    }
  }

  AtomicRMWInst::BinOp thawRMWOperation(const Value *Operation) const {
    NaCl::AtomicRMWOperation Op = (NaCl::AtomicRMWOperation)
        cast<Constant>(Operation)->getUniqueInteger().getLimitedValue();
    switch (Op) {
    // Only valid values should pass validation.
    default: llvm_unreachable("unknown atomic RMW operation");
    case NaCl::AtomicAdd: return AtomicRMWInst::Add;
    case NaCl::AtomicSub: return AtomicRMWInst::Sub;
    case NaCl::AtomicOr:  return AtomicRMWInst::Or;
    case NaCl::AtomicAnd: return AtomicRMWInst::And;
    case NaCl::AtomicXor: return AtomicRMWInst::Xor;
    case NaCl::AtomicExchange: return AtomicRMWInst::Xchg;
    }
  }

  // TODO(jfb) Remove the following hacks once NaCl's x86-32 validator
  // supports 16-bit atomic intrisics. See:
  //   https://code.google.com/p/nativeclient/issues/detail?id=3579
  //   https://code.google.com/p/nativeclient/issues/detail?id=2981
  // ===========================================================================
  bool needsX8632HackFor16BitAtomics(Type *OverloadedType) const {
    return Triple(M->getTargetTriple()).getArch() == Triple::x86 &&
        OverloadedType == Type::getInt16Ty(M->getContext());
  }

  /// Expand the 16-bit Intrinsic into an equivalent 32-bit
  /// compare-exchange loop.
  void atomic16BitX8632Hack(IntrinsicInst *Call, bool IsCmpXChg,
                            Value *Ptr16, Value *RHS, Value *RMWOp,
                            Value *CmpXChgOldVal) const {
    assert((IsCmpXChg ? CmpXChgOldVal : RMWOp) &&
           "cmpxchg expects an old value, whereas RMW expects an operation");
    Type *I16 = Type::getInt16Ty(M->getContext());
    Type *I32 = Type::getInt32Ty(M->getContext());
    Type *I32Ptr = Type::getInt32PtrTy(M->getContext());

    // Precede this with a compiler fence.
    FunctionType *FTy =
        FunctionType::get(Type::getVoidTy(M->getContext()), false);
    std::string AsmString; // Empty.
    std::string Constraints("~{memory}");
    bool HasSideEffect = true;
    CallInst::Create(InlineAsm::get(
        FTy, AsmString, Constraints, HasSideEffect), "", Call);

    BasicBlock *CurrentBB = Call->getParent();
    IRBuilder<> IRB(CurrentBB, Call);
    BasicBlock *Aligned32BB =
        BasicBlock::Create(IRB.getContext(), "atomic16aligned32",
                           CurrentBB->getParent());
    BasicBlock *Aligned16BB =
        BasicBlock::Create(IRB.getContext(), "atomic16aligned16",
                           CurrentBB->getParent());

    // Setup.
    // Align the 16-bit pointer to 32-bits, and figure out if the 16-bit
    // operation is to be carried on the top or bottom half of the
    // 32-bit aligned value.
    Value *IPtr = IRB.CreatePtrToInt(Ptr16, I32, "uintptr");
    Value *IPtrAlign = IRB.CreateAnd(IPtr, IRB.getInt32(~3u), "aligneduintptr");
    Value *Aligned32 = IRB.CreateAnd(IPtr, IRB.getInt32(3u), "aligned32");
    Value *Ptr32 = IRB.CreateIntToPtr(IPtrAlign, I32Ptr, "ptr32");
    Value *IsAligned32 = IRB.CreateICmpEQ(Aligned32, IRB.getInt32(0),
                                          "isaligned32");
    IRB.CreateCondBr(IsAligned32, Aligned32BB, Aligned16BB);

    // Create a diamond after the setup. The rest of the basic block
    // that the Call was in is separated into the successor block.
    BasicBlock *Successor =
        CurrentBB->splitBasicBlock(IRB.GetInsertPoint(), "atomic16successor");
    // Remove the extra unconditional branch that the split added.
    CurrentBB->getTerminator()->eraseFromParent();

    // Aligned 32 block.
    // The 16-bit value was aligned to 32-bits:
    //  - Atomically load the full 32-bit value.
    //  - Get the 16-bit value from its bottom.
    //  - Perform the 16-bit operation.
    //  - Truncate and merge the result back with the top half of the
    //    loaded value.
    //  - Try to compare-exchange this new 32-bit result. This will
    //    succeed if the value at the 32-bit location is still what was
    //    just loaded. If not, try the entire thing again.
    //  - Return the 16-bit value before the operation was performed.
    Value *Ret32;
    {
      IRB.SetInsertPoint(Aligned32BB);
      LoadInst *Loaded = IRB.CreateAlignedLoad(Ptr32, 4, "loaded");
      Loaded->setAtomic(SequentiallyConsistent);
      Value *TruncVal = IRB.CreateTrunc(Loaded, I16, "truncval");
      Ret32 = TruncVal;
      Value *Res;
      if (IsCmpXChg) {
        Res = RHS;
      } else {
        switch (thawRMWOperation(RMWOp)) {
        default: llvm_unreachable("unknown atomic RMW operation");
        case AtomicRMWInst::Add:
          Res = IRB.CreateAdd(TruncVal, RHS, "res"); break;
        case AtomicRMWInst::Sub:
          Res = IRB.CreateSub(TruncVal, RHS, "res"); break;
        case AtomicRMWInst::Or:
          Res = IRB.CreateOr(TruncVal, RHS, "res"); break;
        case AtomicRMWInst::And:
          Res = IRB.CreateAnd(TruncVal, RHS, "res"); break;
        case AtomicRMWInst::Xor:
          Res = IRB.CreateXor(TruncVal, RHS, "res"); break;
        case AtomicRMWInst::Xchg:
          Res = RHS; break;
        }
      }
      Value *MergeRes = IRB.CreateZExt(Res, I32, "mergeres");
      Value *MaskedLoaded = IRB.CreateAnd(Loaded, IRB.getInt32(0xFFFF0000u),
                                          "maskedloaded");
      Value *FinalRes = IRB.CreateOr(MergeRes, MaskedLoaded, "finalres");
      Value *Expected = IsCmpXChg ?
          IRB.CreateOr(MaskedLoaded, IRB.CreateZExt(CmpXChgOldVal, I32, "zext"),
                       "expected") :
          Loaded;
      Value *ValSuc = IRB.CreateAtomicCmpXchg(Ptr32, Expected, FinalRes,
                                              SequentiallyConsistent,
                                              SequentiallyConsistent);
      ValSuc->setName("cmpxchg.results");
      // Test that the entire 32-bit value didn't change during the operation.
      // The cmpxchg returned struct { i32 loaded, i1 success }.
      Value *Success = IRB.CreateExtractValue(ValSuc, 1, "success");
      IRB.CreateCondBr(Success, Successor, Aligned32BB);
    }

    // Aligned 16 block.
    // Similar to the above aligned 32 block, but the 16-bit value is in
    // the top half of the 32-bit value. It needs to be shifted down,
    // and shifted back up before being merged in.
    Value *Ret16;
    {
      IRB.SetInsertPoint(Aligned16BB);
      LoadInst *Loaded = IRB.CreateAlignedLoad(Ptr32, 4, "loaded");
      Loaded->setAtomic(SequentiallyConsistent);
      Value *ShVal = IRB.CreateTrunc(IRB.CreateLShr(Loaded, 16, "lshr"), I16,
                                     "shval");
      Ret16 = ShVal;
      Value *Res;
      if (IsCmpXChg) {
        Res = RHS;
      } else {
        switch (thawRMWOperation(RMWOp)) {
        default: llvm_unreachable("unknown atomic RMW operation");
        case AtomicRMWInst::Add:
          Res = IRB.CreateAdd(ShVal, RHS, "res"); break;
        case AtomicRMWInst::Sub:
          Res = IRB.CreateSub(ShVal, RHS, "res"); break;
        case AtomicRMWInst::Or:
          Res = IRB.CreateOr(ShVal, RHS, "res"); break;
        case AtomicRMWInst::And:
          Res = IRB.CreateAnd(ShVal, RHS, "res"); break;
        case AtomicRMWInst::Xor:
          Res = IRB.CreateXor(ShVal, RHS, "res"); break;
        case AtomicRMWInst::Xchg:
          Res = RHS; break;
        }
      }
      Value *MergeRes = IRB.CreateShl(IRB.CreateZExt(Res, I32, "zext"), 16,
                                      "mergeres");
      Value *MaskedLoaded = IRB.CreateAnd(Loaded, IRB.getInt32(0xFFFF),
                                          "maskedloaded");
      Value *FinalRes = IRB.CreateOr(MergeRes, MaskedLoaded, "finalres");
      Value *Expected = IsCmpXChg ?
          IRB.CreateOr(MaskedLoaded, IRB.CreateShl(
              IRB.CreateZExt(CmpXChgOldVal, I32, "zext"), 16, "shl"),
                       "expected") :
          Loaded;
      Value *ValSuc = IRB.CreateAtomicCmpXchg(Ptr32, Expected, FinalRes,
                                              SequentiallyConsistent,
                                              SequentiallyConsistent);
      ValSuc->setName("cmpxchg.results");
      // Test that the entire 32-bit value didn't change during the operation.
      // The cmpxchg returned struct { i32 loaded, i1 success }.
      Value *Success = IRB.CreateExtractValue(ValSuc, 1, "success");
      IRB.CreateCondBr(Success, Successor, Aligned16BB);
    }

    // Merge the value, and remove the original intrinsic Call.
    IRB.SetInsertPoint(Successor->getFirstInsertionPt());
    PHINode *PHI = IRB.CreatePHI(I16, 2);
    PHI->addIncoming(Ret32, Aligned32BB);
    PHI->addIncoming(Ret16, Aligned16BB);
    Call->replaceAllUsesWith(PHI);
    Call->eraseFromParent();

    // Finish everything with another compiler fence.
    CallInst::Create(InlineAsm::get(
        FTy, AsmString, Constraints, HasSideEffect), "",
                     Successor->getFirstInsertionPt());
  }
  // ===========================================================================
  // End hacks.

  AtomicCallResolver(const AtomicCallResolver &);
  AtomicCallResolver &operator=(const AtomicCallResolver &);
};
}

bool ResolvePNaClIntrinsics::visitCalls(
    ResolvePNaClIntrinsics::CallResolver &Resolver) {
  bool Changed = false;
  Function *IntrinsicFunction = Resolver.getDeclaration();
  if (!IntrinsicFunction)
    return false;

  SmallVector<IntrinsicInst *, 64> Calls;
  for (User *U : IntrinsicFunction->users()) {
    // At this point, the only uses of the intrinsic can be calls, since we
    // assume this pass runs on bitcode that passed ABI verification.
    IntrinsicInst *Call = dyn_cast<IntrinsicInst>(U);
    if (!Call)
      report_fatal_error("Expected use of intrinsic to be a call: " +
                         Resolver.getName());
    Calls.push_back(Call);
  }

  for (auto Call : Calls)
    Changed |= Resolver.resolve(Call);

  return Changed;
}

bool ResolvePNaClIntrinsics::runOnFunction(Function &F) {
  LLVMContext &C = F.getParent()->getContext();
  bool Changed = false;

  IntrinsicCallToFunctionCall SetJmpResolver(F, Intrinsic::nacl_setjmp,
                                             "setjmp");
  IntrinsicCallToFunctionCall LongJmpResolver(F, Intrinsic::nacl_longjmp,
                                              "longjmp");
  Changed |= visitCalls(SetJmpResolver);
  Changed |= visitCalls(LongJmpResolver);

  NaCl::AtomicIntrinsics AI(C);
  NaCl::AtomicIntrinsics::View V = AI.allIntrinsicsAndOverloads();
  for (NaCl::AtomicIntrinsics::View::iterator I = V.begin(), E = V.end();
       I != E; ++I) {
    AtomicCallResolver AtomicResolver(F, I);
    Changed |= visitCalls(AtomicResolver);
  }

  ConstantCallResolver<IsLockFreeToConstant> IsLockFreeResolver(
      F, Intrinsic::nacl_atomic_is_lock_free, IsLockFreeToConstant());
  Changed |= visitCalls(IsLockFreeResolver);

  return Changed;
}

char ResolvePNaClIntrinsics::ID = 0;
INITIALIZE_PASS(ResolvePNaClIntrinsics, "resolve-pnacl-intrinsics",
                "Resolve PNaCl intrinsic calls", false, false)

FunctionPass *llvm::createResolvePNaClIntrinsicsPass() {
  return new ResolvePNaClIntrinsics();
}
