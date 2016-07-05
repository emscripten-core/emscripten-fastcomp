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
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/NaCl.h"
#include "llvm/Transforms/Utils/Local.h"
#if defined(PNACL_BROWSER_TRANSLATOR)
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
  bool runOnFunction(Function &F) override;

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
    CallResolver(const CallResolver &) = delete;
    CallResolver &operator=(const CallResolver &) = delete;
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
  ~IntrinsicCallToFunctionCall() override {}

private:
  Function *TargetFunction;

  Function *doGetDeclaration() const override {
    return Intrinsic::getDeclaration(M, IntrinsicID);
  }

  bool doResolve(IntrinsicInst *Call) override {
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

  IntrinsicCallToFunctionCall(const IntrinsicCallToFunctionCall &) = delete;
  IntrinsicCallToFunctionCall &
  operator=(const IntrinsicCallToFunctionCall &) = delete;
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
  ~ConstantCallResolver() override {}

private:
  Callable Functor;

  Function *doGetDeclaration() const override {
    return Intrinsic::getDeclaration(M, IntrinsicID);
  }

  bool doResolve(IntrinsicInst *Call) override {
    Constant *C = Functor(Call);
    Call->replaceAllUsesWith(C);
    Call->eraseFromParent();
    return true;
  }

  ConstantCallResolver(const ConstantCallResolver &) = delete;
  ConstantCallResolver &operator=(const ConstantCallResolver &) = delete;
};

/// Resolve __nacl_atomic_is_lock_free to true/false at translation
/// time. PNaCl's currently supported platforms all support lock-free atomics at
/// byte sizes {1,2,4,8} except for MIPS and asmjs architectures that supports
/// lock-free atomics at byte sizes {1,2,4}, and the alignment of the pointer is
/// always expected to be natural (as guaranteed by C11 and C++11). PNaCl's
/// Module-level ABI verification checks that the byte size is constant and in
/// {1,2,4,8}.
struct IsLockFreeToConstant {
  Constant *operator()(CallInst *Call) {
    uint64_t MaxLockFreeByteSize = 8;
    const APInt &ByteSize =
        cast<Constant>(Call->getOperand(0))->getUniqueInteger();

#   if defined(PNACL_BROWSER_TRANSLATOR)
    switch (__builtin_nacl_target_arch()) {
    case PnaclTargetArchitectureX86_32:
    case PnaclTargetArchitectureX86_64:
    case PnaclTargetArchitectureARM_32:
      break;
    case PnaclTargetArchitectureMips_32:
      MaxLockFreeByteSize = 4;
      break;
    default:
      errs() << "Architecture: " << Triple::getArchTypeName(Arch) << "\n";
      report_fatal_error("is_lock_free: unhandled architecture");
    }
#   else
    switch (Arch) {
    case Triple::x86:
    case Triple::x86_64:
    case Triple::arm:
      break;
    case Triple::mipsel:
    case Triple::asmjs:
      MaxLockFreeByteSize = 4;
      break;
    default:
      errs() << "Architecture: " << Triple::getArchTypeName(Arch) << "\n";
      report_fatal_error("is_lock_free: unhandled architecture");
    }
#   endif

    bool IsLockFree = ByteSize.ule(MaxLockFreeByteSize);
    auto *C = ConstantInt::get(Call->getType(), IsLockFree);
    return C;
  }

  Triple::ArchType Arch;
  IsLockFreeToConstant(Module *M)
      : Arch(Triple(M->getTargetTriple()).getArch()) {}
  IsLockFreeToConstant() = delete;
};

/// Rewrite atomic intrinsics to LLVM IR instructions.
class AtomicCallResolver : public ResolvePNaClIntrinsics::CallResolver {
public:
  AtomicCallResolver(Function &F,
                     const NaCl::AtomicIntrinsics::AtomicIntrinsic *I)
      : CallResolver(F, I->ID), I(I) {}
  ~AtomicCallResolver() override {}

private:
  const NaCl::AtomicIntrinsics::AtomicIntrinsic *I;

  Function *doGetDeclaration() const override { return I->getDeclaration(M); }

  bool doResolve(IntrinsicInst *Call) override {
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
      I = new AtomicRMWInst(thawRMWOperation(Call->getArgOperand(0)),
                            Call->getArgOperand(1), Call->getArgOperand(2),
                            thawMemoryOrder(Call->getArgOperand(3)), SS, Call);
      break;
    case Intrinsic::nacl_atomic_cmpxchg:
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
      I = new FenceInst(M->getContext(), AtomicOrdering::SequentiallyConsistent, SS, Asm);
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
    for (Instruction *Kill : MaybeDead)
      if (isInstructionTriviallyDead(Kill))
        Kill->eraseFromParent();

    return true;
  }

  unsigned alignmentFromPointer(const Value *Ptr) const {
    auto *PtrType = cast<PointerType>(Ptr->getType());
    unsigned BitWidth = PtrType->getElementType()->getIntegerBitWidth();
    return BitWidth / 8;
  }

  AtomicOrdering thawMemoryOrder(const Value *MemoryOrder) const {
    auto MO = static_cast<NaCl::MemoryOrder>(
        cast<Constant>(MemoryOrder)->getUniqueInteger().getLimitedValue());
    switch (MO) {
    // Only valid values should pass validation.
    default: llvm_unreachable("unknown memory order");
    case NaCl::MemoryOrderRelaxed: return AtomicOrdering::Monotonic;
    // TODO Consume is unspecified by LLVM's internal IR.
    case NaCl::MemoryOrderConsume: return AtomicOrdering::SequentiallyConsistent;
    case NaCl::MemoryOrderAcquire: return AtomicOrdering::Acquire;
    case NaCl::MemoryOrderRelease: return AtomicOrdering::Release;
    case NaCl::MemoryOrderAcquireRelease: return AtomicOrdering::AcquireRelease;
    case NaCl::MemoryOrderSequentiallyConsistent: return AtomicOrdering::SequentiallyConsistent;
    }
  }

  AtomicRMWInst::BinOp thawRMWOperation(const Value *Operation) const {
    auto Op = static_cast<NaCl::AtomicRMWOperation>(
        cast<Constant>(Operation)->getUniqueInteger().getLimitedValue());
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
    auto *Call = dyn_cast<IntrinsicInst>(U);
    if (!Call)
      report_fatal_error("Expected use of intrinsic to be a call: " +
                         Resolver.getName());
    Calls.push_back(Call);
  }

  for (IntrinsicInst *Call : Calls)
    Changed |= Resolver.resolve(Call);

  return Changed;
}

bool ResolvePNaClIntrinsics::runOnFunction(Function &F) {
  Module *M = F.getParent();
  LLVMContext &C = M->getContext();
  bool Changed = false;

  IntrinsicCallToFunctionCall SetJmpResolver(F, Intrinsic::nacl_setjmp,
                                             "setjmp");
  IntrinsicCallToFunctionCall LongJmpResolver(F, Intrinsic::nacl_longjmp,
                                              "longjmp");
  Changed |= visitCalls(SetJmpResolver);
  Changed |= visitCalls(LongJmpResolver);

  NaCl::AtomicIntrinsics AI(C);
  NaCl::AtomicIntrinsics::View V = AI.allIntrinsicsAndOverloads();
  for (auto I = V.begin(), E = V.end(); I != E; ++I) {
    AtomicCallResolver AtomicResolver(F, I);
    Changed |= visitCalls(AtomicResolver);
  }

  ConstantCallResolver<IsLockFreeToConstant> IsLockFreeResolver(
      F, Intrinsic::nacl_atomic_is_lock_free, IsLockFreeToConstant(M));
  Changed |= visitCalls(IsLockFreeResolver);

  return Changed;
}

char ResolvePNaClIntrinsics::ID = 0;
INITIALIZE_PASS(ResolvePNaClIntrinsics, "resolve-pnacl-intrinsics",
                "Resolve PNaCl intrinsic calls", false, false)

FunctionPass *llvm::createResolvePNaClIntrinsicsPass() {
  return new ResolvePNaClIntrinsics();
}
