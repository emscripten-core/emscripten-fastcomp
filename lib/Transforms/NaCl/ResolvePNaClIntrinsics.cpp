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
#include "llvm/IR/Constant.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/NaClAtomicIntrinsics.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Transforms/NaCl.h"

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
class SimpleCallResolver : public ResolvePNaClIntrinsics::CallResolver {
public:
  SimpleCallResolver(Function &F, Intrinsic::ID IntrinsicID,
                     const char *TargetFunctionName)
      : CallResolver(F, IntrinsicID),
        TargetFunction(M->getFunction(TargetFunctionName)) {
    // Expect to find the target function for this intrinsic already
    // declared, even if it is never used.
    if (!TargetFunction)
      report_fatal_error(
          std::string("Expected to find external declaration of ") +
          TargetFunctionName);
  }
  virtual ~SimpleCallResolver() {}

private:
  Function *TargetFunction;

  virtual Function *doGetDeclaration() const {
    return Intrinsic::getDeclaration(M, IntrinsicID);
  }

  virtual bool doResolve(IntrinsicInst *Call) {
    Call->setCalledFunction(TargetFunction);
    return true;
  }

  SimpleCallResolver(const SimpleCallResolver &) LLVM_DELETED_FUNCTION;
  SimpleCallResolver &operator=(
      const SimpleCallResolver &) LLVM_DELETED_FUNCTION;
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
      // TODO LLVM currently doesn't support specifying separate memory
      //      orders for compare exchange's success and failure cases:
      //      LLVM IR implicitly drops the Release part of the specified
      //      memory order on failure. It is therefore correct to map
      //      the success memory order onto the LLVM IR and ignore the
      //      failure one.
      I = new AtomicCmpXchgInst(Call->getArgOperand(0), Call->getArgOperand(1),
                                Call->getArgOperand(2),
                                thawMemoryOrder(Call->getArgOperand(3)), SS,
                                Call);
      break;
    case Intrinsic::nacl_atomic_fence:
      I = new FenceInst(M->getContext(),
                        thawMemoryOrder(Call->getArgOperand(0)), SS, Call);
      break;
    }
    I->setName(Call->getName());
    I->setDebugLoc(Call->getDebugLoc());
    Call->replaceAllUsesWith(I);
    Call->eraseFromParent();

    return true;
  }

  unsigned alignmentFromPointer(const Value *Ptr) const {
    const PointerType *PtrType = cast<PointerType>(Ptr->getType());
    unsigned BitWidth = PtrType->getElementType()->getIntegerBitWidth();
    return BitWidth / 8;
  }

  AtomicOrdering thawMemoryOrder(const Value *MemoryOrder) const {
    NaCl::MemoryOrder MO = (NaCl::MemoryOrder)cast<Constant>(MemoryOrder)
        ->getUniqueInteger().getLimitedValue();
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
    NaCl::AtomicRMWOperation Op =
        (NaCl::AtomicRMWOperation)cast<Constant>(Operation)->getUniqueInteger()
            .getLimitedValue();
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

  for (Value::use_iterator UI = IntrinsicFunction->use_begin(),
                           UE = IntrinsicFunction->use_end();
       UI != UE;) {
    // At this point, the only uses of the intrinsic can be calls, since
    // we assume this pass runs on bitcode that passed ABI verification.
    IntrinsicInst *Call = dyn_cast<IntrinsicInst>(*UI++);
    if (!Call)
      report_fatal_error("Expected use of intrinsic to be a call: " +
                         Resolver.getName());

    Changed |= Resolver.resolve(Call);
  }

  return Changed;
}

bool ResolvePNaClIntrinsics::runOnFunction(Function &F) {
  bool Changed = false;

  SimpleCallResolver SetJmpResolver(F, Intrinsic::nacl_setjmp, "setjmp");
  SimpleCallResolver LongJmpResolver(F, Intrinsic::nacl_longjmp, "longjmp");
  Changed |= visitCalls(SetJmpResolver);
  Changed |= visitCalls(LongJmpResolver);

  NaCl::AtomicIntrinsics AI(F.getParent()->getContext());
  NaCl::AtomicIntrinsics::View V = AI.allIntrinsicsAndOverloads();
  for (NaCl::AtomicIntrinsics::View::iterator I = V.begin(), E = V.end();
       I != E; ++I) {
    AtomicCallResolver AtomicResolver(F, I);
    Changed |= visitCalls(AtomicResolver);
  }

  return Changed;
}

char ResolvePNaClIntrinsics::ID = 0;
INITIALIZE_PASS(ResolvePNaClIntrinsics, "resolve-pnacl-intrinsics",
                "Resolve PNaCl intrinsic calls", false, false)

FunctionPass *llvm::createResolvePNaClIntrinsicsPass() {
  return new ResolvePNaClIntrinsics();
}
