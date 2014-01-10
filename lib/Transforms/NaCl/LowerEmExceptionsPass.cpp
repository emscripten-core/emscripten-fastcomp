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
#include <vector>

#include "llvm/Support/raw_ostream.h"
#include <stdio.h>
#define dump(x) fprintf(stderr, x "\n")
#define dumpv(x, ...) fprintf(stderr, x "\n", __VA_ARGS__)
#define dumpfail(x)       { fprintf(stderr, x "\n");              fprintf(stderr, "%s : %d\n", __FILE__, __LINE__); report_fatal_error("fail"); }
#define dumpfailv(x, ...) { fprintf(stderr, x "\n", __VA_ARGS__); fprintf(stderr, "%s : %d\n", __FILE__, __LINE__); report_fatal_error("fail"); }
#define dumpIR(value) { \
  std::string temp; \
  raw_string_ostream stream(temp); \
  stream << *(value); \
  fprintf(stderr, "%s\n", temp.c_str()); \
}
#undef assert
#define assert(x) { if (!(x)) dumpfail(#x); }

using namespace llvm;

namespace {
  class LowerEmExceptions : public ModulePass {
    Function *GetHigh, *PreInvoke, *PostInvoke, *LandingPad;
    Module *TheModule;

  public:
    static char ID; // Pass identification, replacement for typeid
    explicit LowerEmExceptions() : ModulePass(ID), GetHigh(NULL), PreInvoke(NULL), PostInvoke(NULL), LandingPad(NULL), TheModule(NULL) {
      initializeLowerEmExceptionsPass(*PassRegistry::getPassRegistry());
    }
    bool runOnModule(Module &M);
  };
}

char LowerEmExceptions::ID = 0;
INITIALIZE_PASS(LowerEmExceptions, "loweremexceptions",
                "Lower invoke and unwind for js/emscripten",
                false, false)

bool LowerEmExceptions::runOnModule(Module &M) {
  TheModule = &M;

  // Add functions

  Type *i32 = Type::getInt32Ty(M.getContext());
  Type *i8 = Type::getInt8Ty(M.getContext());
  Type *i1 = Type::getInt1Ty(M.getContext());
  Type *i8P = i8->getPointerTo();
  Type *Void = Type::getVoidTy(M.getContext());

  if (!TheModule->getFunction("getHigh32")) {
    FunctionType *GetHighFunc = FunctionType::get(i32, false);
    GetHigh = Function::Create(GetHighFunc, GlobalValue::ExternalLinkage,
                               "getHigh32", TheModule);
  }

  FunctionType *VoidFunc = FunctionType::get(Void, false);
  PreInvoke = Function::Create(VoidFunc, GlobalValue::ExternalLinkage, "emscripten_preinvoke", TheModule);

  FunctionType *Int1Func = FunctionType::get(i1, false);
  PostInvoke = Function::Create(Int1Func, GlobalValue::ExternalLinkage, "emscripten_postinvoke", TheModule);

  FunctionType *LandingPadFunc = FunctionType::get(i32, true);
  LandingPad = Function::Create(LandingPadFunc, GlobalValue::ExternalLinkage, "emscripten_landingpad", TheModule);

  // Process

  bool Changed = false;

  std::vector<Instruction*> ToErase;

  for (Module::iterator Iter = M.begin(), E = M.end(); Iter != E; ) {
    Function *F = Iter++;
    for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB) {
      if (InvokeInst *II = dyn_cast<InvokeInst>(BB->getTerminator())) {
        // Insert a normal call instruction folded in between pre- and post-invoke
        CallInst *Pre = CallInst::Create(PreInvoke, "", II);

        SmallVector<Value*,16> CallArgs(II->op_begin(), II->op_end() - 3);
        CallInst *NewCall = CallInst::Create(II->getCalledValue(),
                                             CallArgs, "", II);
        NewCall->takeName(II);
        NewCall->setCallingConv(II->getCallingConv());
        NewCall->setAttributes(II->getAttributes());
        NewCall->setDebugLoc(II->getDebugLoc());
        II->replaceAllUsesWith(NewCall);
        ToErase.push_back(II);

        CallInst *Post = CallInst::Create(PostInvoke, "", II);

        // Insert a branch based on the postInvoke
        BranchInst::Create(II->getNormalDest(), II->getUnwindDest(), Post, II);

        // Replace the landingpad with a landingpad call to get the low part, and a getHigh for the high
        LandingPadInst *LP = II->getLandingPadInst();
        unsigned Num = LP->getNumClauses();
        SmallVector<Value*,16> NewLPArgs;
        NewLPArgs.push_back(LP->getPersonalityFn());
        for (unsigned i = 0; i < Num; i++) NewLPArgs.push_back(LP->getClause(i));
        NewLPArgs.push_back(LP->isCleanup() ? ConstantInt::getTrue(i1) : ConstantInt::getFalse(i1));
        CallInst *NewLP = CallInst::Create(LandingPad, NewLPArgs, "", LP);

        Instruction *High = CallInst::Create(GetHigh, "", LP);

        // New recreate an aggregate for them, which will be all simplified later (simplification cannot handle landingpad, hence all this)
        SmallVector<unsigned, 1> IVArgsA;
        IVArgsA.push_back(0);
        InsertValueInst *IVA = InsertValueInst::Create(UndefValue::get(LP->getType()), NewLP, IVArgsA, "", LP);
        SmallVector<unsigned, 1> IVArgsB;
        IVArgsB.push_back(1);
        InsertValueInst *IVB = InsertValueInst::Create(IVA, High, IVArgsB, "", LP);

        LP->replaceAllUsesWith(IVB);
        ToErase.push_back(LP);

        Changed = true;
      }
    }
  }

  for (unsigned i = 0; i < ToErase.size(); i++) ToErase[i]->eraseFromParent();

  return Changed;
}

ModulePass *llvm::createLowerEmExceptionsPass() {
  return new LowerEmExceptions();
}

