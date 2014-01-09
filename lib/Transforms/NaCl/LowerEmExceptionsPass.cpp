//===- LowerEmExceptions - Lower exceptions for Emscripten/JS   -----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This is based off the 'cheap' version of LowerInvoke. It does two things:
//
//  1) Lower
//         invoke() to l1 unwind l2
//     into
//         preinvoke(); // (will clear __THREW__)
//         call();
//         threw = postinvoke(); (check __THREW__)
//         br threw, l1, l2
//
//  2) Lower landingpads to return a single i8*, avoid the structural type
//     which is unneeded anyhow.
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Scalar.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Target/TargetLowering.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

namespace {
  class LowerEmExceptions : public ModulePass {
    Function *PreInvoke, *PostInvoke;
    Module *TheModule;

  public:
    static char ID; // Pass identification, replacement for typeid
    explicit LowerEmExceptions() : ModulePass(ID), PreInvoke(NULL), PostInvoke(NULL), TheModule(NULL) {
      initializeLowerEmExceptionsPass(*PassRegistry::getPassRegistry());
    }
    bool runOnModule(Module &M);
  };
}

char LowerEmExceptions::ID = 0;
INITIALIZE_PASS(LowerEmExceptions, "loweremexceptions",
                "Lower invoke and unwind for js/emscripten",
                false, false)

Instruction *getSingleUse(Instruction *I) {
  Instruction *Ret = NULL;
  for (Instruction::use_iterator UI = I->use_begin(), UE = I->use_end(); UI != UE; ++UI) {
    assert(Ret == NULL);
    Ret = cast<ExtractElementInst>(*UI);
  }
  assert(Ret != NULL);
  return Ret;
}

bool LowerEmExceptions::runOnModule(Module &M) {
  TheModule = &M;

  Type *Void = Type::getVoidTy(M.getContext());
  Type *i32 = Type::getInt32Ty(M.getContext());

  SmallVector<Type*, 0> ArgTypes;
  FunctionType *VoidFunc = FunctionType::get(Void, ArgTypes, false);
  FunctionType *IntFunc = FunctionType::get(i32, ArgTypes, false);

  PreInvoke = Function::Create(VoidFunc, GlobalValue::ExternalLinkage, "preInvoke", TheModule);
  PostInvoke = Function::Create(IntFunc, GlobalValue::ExternalLinkage, "postInvoke", TheModule);

  bool Changed = false;

  for (Module::iterator Iter = M.begin(), E = M.end(); Iter != E; ) {
    Function *F = Iter++;
    for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB) {
      if (InvokeInst *II = dyn_cast<InvokeInst>(BB->getTerminator())) {
        // Fix up the landingpad. First, make a copy returning just an integer
        LandingPadInst *LP = II->getLandingPadInst();
        unsigned Num = LP->getNumClauses();
        LandingPadInst *NewLP = LandingPadInst::Create(i32, LP->getPersonalityFn(), Num, "", LP);
        NewLP->setCleanup(LP->isCleanup());
        for (unsigned i = 0; i < Num; i++) NewLP->addClause(LP->getClause(i));

        // Next, replace the old LP's single use, which is an extractelement, to eliminate the ee's and use the value directly
        ExtractElementInst *EE = cast<ExtractElementInst>(getSingleUse(LP));
        EE->replaceAllUsesWith(NewLP);
        EE->eraseFromParent();

        // Finish the LP by replacing it
        LP->replaceAllUsesWith(NewLP);
        LP->eraseFromParent();

        Changed = true;
      }
    }
  }

  return Changed;
}

ModulePass *llvm::createLowerEmExceptionsPass() {
  return new LowerEmExceptions();
}

