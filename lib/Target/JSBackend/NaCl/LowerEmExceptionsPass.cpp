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
//         preinvoke(id); // (will clear __THREW__)
//         call();
//         threw = postinvoke(id); (check __THREW__)
//         br threw, l1, l2
//
//     We do this to avoid introducing a new LLVM IR type, or to try to reuse
//     invoke-landingpad for our special purposes (as they are checked very
//     carefully by llvm)
//
//  2) Lower landingpads to a call to emscripten_landingpad
//
//  3) Lower resume to emscripten_resume which receives non-aggregate inputs
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
#include "llvm/Support/raw_ostream.h"

#include <vector>
#include <set>

using namespace llvm;

static cl::list<std::string>
Whitelist("emscripten-cxx-exceptions-whitelist",
          cl::desc("Enables C++ exceptions in emscripten (see emscripten EXCEPTION_CATCHING_WHITELIST option)"),
          cl::CommaSeparated);

namespace {
  class LowerEmExceptions : public ModulePass {
    Function *GetHigh, *PreInvoke, *PostInvoke, *LandingPad, *Resume;
    Module *TheModule;

  public:
    static char ID; // Pass identification, replacement for typeid
    explicit LowerEmExceptions() : ModulePass(ID), GetHigh(NULL), PreInvoke(NULL), PostInvoke(NULL), LandingPad(NULL), Resume(NULL), TheModule(NULL) {
      initializeLowerEmExceptionsPass(*PassRegistry::getPassRegistry());
    }
    bool runOnModule(Module &M);
  };
}

char LowerEmExceptions::ID = 0;
INITIALIZE_PASS(LowerEmExceptions, "loweremexceptions",
                "Lower invoke and unwind for js/emscripten",
                false, false)

bool canThrow(Value *V) {
  if (Function *F = dyn_cast<Function>(V)) {
    // intrinsics and some emscripten builtins cannot throw
    if (F->isIntrinsic()) return false;
    StringRef Name = F->getName();
    if (Name.startswith("emscripten_asm_")) return false;
    if (Name == "setjmp" || Name == "longjmp") return false; // leave setjmp and longjmp (mostly) alone, we process them properly later
    return true;
  }
  return true; // not a function, so an indirect call - can throw, we can't tell
}

bool LowerEmExceptions::runOnModule(Module &M) {
  TheModule = &M;

  // Add functions

  Type *i32 = Type::getInt32Ty(M.getContext());
  Type *i8 = Type::getInt8Ty(M.getContext());
  Type *i1 = Type::getInt1Ty(M.getContext());
  Type *i8P = i8->getPointerTo();
  Type *Void = Type::getVoidTy(M.getContext());

  if (!(GetHigh = TheModule->getFunction("getHigh32"))) {
    FunctionType *GetHighFunc = FunctionType::get(i32, false);
    GetHigh = Function::Create(GetHighFunc, GlobalValue::ExternalLinkage,
                               "getHigh32", TheModule);
  }

  if (!(PreInvoke = TheModule->getFunction("emscripten_preinvoke"))) {
    SmallVector<Type*, 1> IntArgTypes;
    IntArgTypes.push_back(i32);
    FunctionType *VoidIntFunc = FunctionType::get(Void, IntArgTypes, false);
    PreInvoke = Function::Create(VoidIntFunc, GlobalValue::ExternalLinkage, "emscripten_preinvoke", TheModule);
  }

  if (!(PostInvoke = TheModule->getFunction("emscripten_postinvoke"))) {
    SmallVector<Type*, 1> IntArgTypes;
    IntArgTypes.push_back(i32);
    FunctionType *IntIntFunc = FunctionType::get(i32, IntArgTypes, false);
    PostInvoke = Function::Create(IntIntFunc, GlobalValue::ExternalLinkage, "emscripten_postinvoke", TheModule);
  }

  FunctionType *LandingPadFunc = FunctionType::get(i8P, true);
  LandingPad = Function::Create(LandingPadFunc, GlobalValue::ExternalLinkage, "emscripten_landingpad", TheModule);

  FunctionType *ResumeFunc = FunctionType::get(Void, true);
  Resume = Function::Create(ResumeFunc, GlobalValue::ExternalLinkage, "emscripten_resume", TheModule);
  
  // Process

  std::set<std::string> WhitelistSet(Whitelist.begin(), Whitelist.end());

  bool Changed = false;

  unsigned InvokeId = 0;

  for (Module::iterator Iter = M.begin(), E = M.end(); Iter != E; ) {
    Function *F = &*Iter++;

    std::vector<Instruction*> ToErase;
    std::set<LandingPadInst*> LandingPads;

    bool AllowExceptionsInFunc = WhitelistSet.empty() || (WhitelistSet.count("_" + F->getName().str()) != 0);

    for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB) {
      // check terminator for invokes
      if (InvokeInst *II = dyn_cast<InvokeInst>(BB->getTerminator())) {
        LandingPads.insert(II->getLandingPadInst());

        bool NeedInvoke = AllowExceptionsInFunc && canThrow(II->getCalledValue());

        if (NeedInvoke) {
          // If we are calling a function that is noreturn, we must remove that attribute. The code we
          // insert here does expect it to return, after we catch the exception.
          if (II->doesNotReturn()) {
            if (Function *F = dyn_cast<Function>(II->getCalledValue())) {
              F->removeFnAttr(Attribute::NoReturn);
            }
            II->setAttributes(II->getAttributes().removeAttribute(TheModule->getContext(), AttributeSet::FunctionIndex, Attribute::NoReturn));
            assert(!II->doesNotReturn());
          }

          // Insert a normal call instruction folded in between pre- and post-invoke
          SmallVector<Value*,1> HelperArgs;
          HelperArgs.push_back(ConstantInt::get(i32, InvokeId++));
          CallInst::Create(PreInvoke, HelperArgs, "", II);

          SmallVector<Value*,16> CallArgs(II->op_begin(), II->op_end() - 3);
          CallInst *NewCall = CallInst::Create(II->getCalledValue(),
                                               CallArgs, "", II);
          NewCall->takeName(II);
          NewCall->setCallingConv(II->getCallingConv());
          NewCall->setAttributes(II->getAttributes());
          NewCall->setDebugLoc(II->getDebugLoc());
          II->replaceAllUsesWith(NewCall);
          ToErase.push_back(II);

          CallInst *Post = CallInst::Create(PostInvoke, HelperArgs, "", II);
          Instruction *Post1 = new TruncInst(Post, i1, "", II);

          // Insert a branch based on the postInvoke
          BranchInst::Create(II->getUnwindDest(), II->getNormalDest(), Post1, II);
        } else {
          // This can't throw, and we don't need this invoke, just replace it with a call+branch
          SmallVector<Value*,16> CallArgs(II->op_begin(), II->op_end() - 3);
          CallInst *NewCall = CallInst::Create(II->getCalledValue(),
                                               CallArgs, "", II);
          NewCall->takeName(II);
          NewCall->setCallingConv(II->getCallingConv());
          NewCall->setAttributes(II->getAttributes());
          NewCall->setDebugLoc(II->getDebugLoc());
          II->replaceAllUsesWith(NewCall);
          ToErase.push_back(II);

          BranchInst::Create(II->getNormalDest(), II);

          // Remove any PHI node entries from the exception destination.
          II->getUnwindDest()->removePredecessor(&*BB);
        }

        Changed = true;
      }
      // scan the body of the basic block for resumes
      for (BasicBlock::iterator Iter = BB->begin(), E = BB->end();
           Iter != E; ) {
        Instruction *I = &*Iter++;
        if (ResumeInst *R = dyn_cast<ResumeInst>(I)) {
          // split the input into legal values
          Value *Input = R->getValue();
          ExtractValueInst *Low = ExtractValueInst::Create(Input, 0, "", R);
          ExtractValueInst *High = ExtractValueInst::Create(Input, 1, "", R);

          // create a resume call
          SmallVector<Value*,2> CallArgs;
          CallArgs.push_back(Low);
          CallArgs.push_back(High);
          CallInst::Create(Resume, CallArgs, "", R);

          new UnreachableInst(TheModule->getContext(), R); // add a terminator to the block

          ToErase.push_back(R);
        }
      }
    }

    // Look for orphan landingpads, can occur in blocks with no predecesors
    for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB) {
      Instruction *I = BB->getFirstNonPHI();
      if (LandingPadInst *LP = dyn_cast<LandingPadInst>(I)) {
        LandingPads.insert(LP);
      }
    }

    // Handle all the landingpad for this function together, as multiple invokes may share a single lp
    for (std::set<LandingPadInst*>::iterator I = LandingPads.begin(); I != LandingPads.end(); I++) {
      // Replace the landingpad with a landingpad call to get the low part, and a getHigh for the high
      LandingPadInst *LP = *I;
      unsigned Num = LP->getNumClauses();
      SmallVector<Value*,16> NewLPArgs;
      NewLPArgs.push_back(F->getPersonalityFn());
      for (unsigned i = 0; i < Num; i++) {
        Value *Arg = LP->getClause(i);
        // As a temporary workaround for the lack of aggregate varargs support
        // in the varargs lowering code, break out filter operands into their
        // component elements.
        if (LP->isFilter(i)) {
          ArrayType *ATy = cast<ArrayType>(Arg->getType());
          for (unsigned elem = 0, elemEnd = ATy->getNumElements(); elem != elemEnd; ++elem) {
            Instruction *EE = ExtractValueInst::Create(Arg, makeArrayRef(elem), "", LP);
            NewLPArgs.push_back(EE);
          }
        } else {
          NewLPArgs.push_back(Arg);
        }
      }
      NewLPArgs.push_back(LP->isCleanup() ? ConstantInt::getTrue(i1) : ConstantInt::getFalse(i1));
      CallInst *NewLP = CallInst::Create(LandingPad, NewLPArgs, "", LP);

      Instruction *High = CallInst::Create(GetHigh, "", LP);

      // New recreate an aggregate for them, which will be all simplified later (simplification cannot handle landingpad, hence all this)
      InsertValueInst *IVA = InsertValueInst::Create(UndefValue::get(LP->getType()), NewLP, 0, "", LP);
      InsertValueInst *IVB = InsertValueInst::Create(IVA, High, 1, "", LP);

      LP->replaceAllUsesWith(IVB);
      ToErase.push_back(LP);
    }

    // erase everything we no longer need in this function
    for (unsigned i = 0; i < ToErase.size(); i++) ToErase[i]->eraseFromParent();
  }

  return Changed;
}

ModulePass *llvm::createLowerEmExceptionsPass() {
  return new LowerEmExceptions();
}

