//===- CanonicalizeMemIntrinsics.cpp - Make memcpy's "len" arg consistent--===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass canonicalizes uses of the llvm.memset, llvm.memcpy and
// llvm.memmove intrinsics so that the variants with 64-bit "len"
// arguments aren't used, and the 32-bit variants are used instead.
//
// This means the PNaCl translator won't need to handle two versions
// of each of these intrinsics, and it won't need to do any implicit
// truncations from 64-bit to 32-bit.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

namespace {
  // This is a ModulePass because that makes it easier to find all
  // uses of intrinsics efficiently.
  class CanonicalizeMemIntrinsics : public ModulePass {
  public:
    static char ID; // Pass identification, replacement for typeid
    CanonicalizeMemIntrinsics() : ModulePass(ID) {
      initializeCanonicalizeMemIntrinsicsPass(*PassRegistry::getPassRegistry());
    }

    virtual bool runOnModule(Module &M);
  };
}

char CanonicalizeMemIntrinsics::ID = 0;
INITIALIZE_PASS(CanonicalizeMemIntrinsics, "canonicalize-mem-intrinsics",
                "Make memcpy() et al's \"len\" argument consistent",
                false, false)

static bool expandIntrinsic(Module *M, Intrinsic::ID ID) {
  SmallVector<Type *, 3> Types;
  Types.push_back(Type::getInt8PtrTy(M->getContext()));
  if (ID != Intrinsic::memset)
    Types.push_back(Type::getInt8PtrTy(M->getContext()));
  unsigned LengthTypePos = Types.size();
  Types.push_back(Type::getInt64Ty(M->getContext()));

  std::string OldName = Intrinsic::getName(ID, Types);
  Function *OldIntrinsic = M->getFunction(OldName);
  if (!OldIntrinsic)
    return false;

  Types[LengthTypePos] = Type::getInt32Ty(M->getContext());
  Function *NewIntrinsic = Intrinsic::getDeclaration(M, ID, Types);

  SmallVector<CallInst *, 64> Calls;
  for (User *U : OldIntrinsic->users()) {
    if (CallInst *Call = dyn_cast<CallInst>(U))
      Calls.push_back(Call);
    else
      report_fatal_error("CanonicalizeMemIntrinsics: Taking the address of an "
                         "intrinsic is not allowed: " +
                         OldName);
  }

  for (CallInst *Call : Calls) {
    // This temporarily leaves Call non-well-typed.
    Call->setCalledFunction(NewIntrinsic);
    // Truncate the "len" argument.  No overflow check.
    IRBuilder<> Builder(Call);
    Value *Length = Builder.CreateTrunc(Call->getArgOperand(2),
                                        Type::getInt32Ty(M->getContext()),
                                        "mem_len_truncate");
    Call->setArgOperand(2, Length);
  }

  OldIntrinsic->eraseFromParent();
  return true;
}

bool CanonicalizeMemIntrinsics::runOnModule(Module &M) {
  bool Changed = false;
  Changed |= expandIntrinsic(&M, Intrinsic::memset);
  Changed |= expandIntrinsic(&M, Intrinsic::memcpy);
  Changed |= expandIntrinsic(&M, Intrinsic::memmove);
  return Changed;
}

ModulePass *llvm::createCanonicalizeMemIntrinsicsPass() {
  return new CanonicalizeMemIntrinsics();
}
