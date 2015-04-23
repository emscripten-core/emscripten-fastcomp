//===- RewriteLLVMIntrinsics.cpp - Rewrite LLVM intrinsics to other values ===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass replaces calls to LLVM intrinsics that are *not* part of the
// PNaCl stable bitcode ABI into simpler values.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Twine.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/NaCl.h"
#include <string>

using namespace llvm;

namespace {
class RewriteLLVMIntrinsics : public ModulePass {
public:
  static char ID;
  RewriteLLVMIntrinsics() : ModulePass(ID) {
    // This is a module pass because this makes it easier to access uses
    // of global intrinsic functions.
    initializeRewriteLLVMIntrinsicsPass(*PassRegistry::getPassRegistry());
  }

  virtual bool runOnModule(Module &M);

  /// Rewrite an intrinsic to something different.
  class IntrinsicRewriter {
  public:
    Function *function() const { return F; }
    /// Called once per \p Call of the Intrinsic Function.
    void rewriteCall(CallInst *Call) { doRewriteCall(Call); }

  protected:
    IntrinsicRewriter(Module &M, Intrinsic::ID IntrinsicID)
        : F(Intrinsic::getDeclaration(&M, IntrinsicID)) {}
    virtual ~IntrinsicRewriter() {}
    /// This pure virtual method must be defined by implementors, and
    /// will be called by rewriteCall.
    virtual void doRewriteCall(CallInst *Call) = 0;

    Function *F;

  private:
    IntrinsicRewriter() = delete;
    IntrinsicRewriter(const IntrinsicRewriter &) = delete;
    IntrinsicRewriter &operator=(const IntrinsicRewriter &) = delete;
  };

private:
  /// Visit all uses of a Function, rewrite it using the \p Rewriter,
  /// and then delete the Call. Later delete the Function from the
  /// Module. Returns true if the Module was changed.
  bool visitUses(IntrinsicRewriter &Rewriter);
};

/// Rewrite a Call to nothing.
class ToNothing : public RewriteLLVMIntrinsics::IntrinsicRewriter {
public:
  ToNothing(Module &M, Intrinsic::ID IntrinsicID)
      : IntrinsicRewriter(M, IntrinsicID) {}
  virtual ~ToNothing() {}

protected:
  virtual void doRewriteCall(CallInst *Call) {
    // Nothing to do: the visit does the deletion.
  }
};

/// Rewrite a Call to a ConstantInt of the same type.
class ToConstantInt : public RewriteLLVMIntrinsics::IntrinsicRewriter {
public:
  ToConstantInt(Module &M, Intrinsic::ID IntrinsicID, uint64_t Value)
      : IntrinsicRewriter(M, IntrinsicID), Value(Value),
        RetType(function()->getFunctionType()->getReturnType()) {}
  virtual ~ToConstantInt() {}

protected:
  virtual void doRewriteCall(CallInst *Call) {
    Constant *C = ConstantInt::get(RetType, Value);
    Call->replaceAllUsesWith(C);
  }

private:
  uint64_t Value;
  Type *RetType;
};
}

char RewriteLLVMIntrinsics::ID = 0;
INITIALIZE_PASS(RewriteLLVMIntrinsics, "rewrite-llvm-intrinsic-calls",
                "Rewrite LLVM intrinsic calls to simpler expressions", false,
                false)

bool RewriteLLVMIntrinsics::runOnModule(Module &M) {
  // Replace all uses of the @llvm.flt.rounds intrinsic with the constant
  // "1" (round-to-nearest). Until we add a second intrinsic like
  // @llvm.set.flt.round it is impossible to have a rounding mode that is
  // not the initial rounding mode (round-to-nearest). We can remove
  // this rewrite after adding a set() intrinsic.
  ToConstantInt FltRoundsRewriter(M, Intrinsic::flt_rounds, 1);

  // Remove all @llvm.prefetch intrinsics.
  ToNothing PrefetchRewriter(M, Intrinsic::prefetch);
  ToNothing AssumeRewriter(M, Intrinsic::assume);

  return visitUses(FltRoundsRewriter) | visitUses(PrefetchRewriter)
    | visitUses(AssumeRewriter);
}

bool RewriteLLVMIntrinsics::visitUses(IntrinsicRewriter &Rewriter) {
  Function *F = Rewriter.function();
  SmallVector<CallInst *, 64> Calls;
  for (User *U : F->users()) {
    if (CallInst *Call = dyn_cast<CallInst>(U)) {
      Calls.push_back(Call);
    } else {
      // Intrinsics we care about currently don't need to handle this case.
      std::string S;
      raw_string_ostream OS(S);
      OS << "Taking the address of this intrinsic is invalid: " << *U;
      report_fatal_error(OS.str());
    }
  }

  for (auto Call : Calls) {
      Rewriter.rewriteCall(Call);
      Call->eraseFromParent();
  }

  F->eraseFromParent();
  return !Calls.empty();
}

ModulePass *llvm::createRewriteLLVMIntrinsicsPass() {
  return new RewriteLLVMIntrinsics();
}
