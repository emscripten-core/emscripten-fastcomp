//===- LowerEmSetjmp - Lower setjmp/longjmp for Emscripten/JS   -----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Lowers setjmp to a reasonably-performant approach for emscripten. The idea
// is that each block with a setjmp is broken up into the part right after
// the setjmp, and a new basic block is added which is either reached from
// the setjmp, or later from a longjmp. To handle the longjmp, all calls that
// might longjmp are checked immediately afterwards.
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
#include <set>

#include "llvm/Support/raw_ostream.h"

#ifdef NDEBUG
#undef assert
#define assert(x) { if (!(x)) dumpfail(#x); }
#endif

using namespace llvm;

namespace {
  class LowerEmSetjmp : public ModulePass {
    Module *TheModule;

  public:
    static char ID; // Pass identification, replacement for typeid
    explicit LowerEmSetjmp() : ModulePass(ID), TheModule(NULL) {
      initializeLowerEmSetjmpPass(*PassRegistry::getPassRegistry());
    }
    bool runOnModule(Module &M);
  };
}

char LowerEmSetjmp::ID = 0;
INITIALIZE_PASS(LowerEmSetjmp, "loweremsetjmp",
                "Lower setjmp and longjmp for js/emscripten",
                false, false)

bool LowerEmSetjmp::runOnModule(Module &M) {
  TheModule = &M;

  Function *Setjmp = TheModule->getFunction("setjmp");
  Function *Longjmp = TheModule->getFunction("longjmp");
  if (!Setjmp && !Longjmp) return false;

  Type *i32 = Type::getInt32Ty(M.getContext());
  Type *Void = Type::getVoidTy(M.getContext());

  // Add functions

  Function *EmSetjmp = NULL;

  if (Setjmp) {
    SmallVector<Type*, 2> EmSetjmpTypes;
    EmSetjmpTypes.push_back(Setjmp->getFunctionType()->getParamType(0));
    EmSetjmpTypes.push_back(i32); // extra param that says which setjmp in the function it is
    FunctionType *EmSetjmpFunc = FunctionType::get(i32, EmSetjmpTypes, false);
    EmSetjmp = Function::Create(EmSetjmpFunc, GlobalValue::ExternalLinkage, "emscripten_setjmp", TheModule);
  }

  Function *EmLongjmp = Longjmp ? Function::Create(Longjmp->getFunctionType(), GlobalValue::ExternalLinkage, "emscripten_longjmp", TheModule) : NULL;

  SmallVector<Type*, 1> IntArgTypes;
  IntArgTypes.push_back(i32);
  FunctionType *IntIntFunc = FunctionType::get(i32, IntArgTypes, false);

  Function *CheckLongjmp = Function::Create(IntIntFunc, GlobalValue::ExternalLinkage, "emscripten_check_longjmp", TheModule); // gets control flow

  Function *GetLongjmpResult = Function::Create(IntIntFunc, GlobalValue::ExternalLinkage, "emscripten_get_longjmp_result", TheModule); // gets int value longjmp'd

  FunctionType *VoidFunc = FunctionType::get(Void, false);
  Function *PrepSetjmp = Function::Create(VoidFunc, GlobalValue::ExternalLinkage, "emscripten_prep_setjmp", TheModule);

  Function *PreInvoke = TheModule->getFunction("emscripten_preinvoke");
  if (!PreInvoke) PreInvoke = Function::Create(VoidFunc, GlobalValue::ExternalLinkage, "emscripten_preinvoke", TheModule);

  FunctionType *IntFunc = FunctionType::get(i32, false);
  Function *PostInvoke = TheModule->getFunction("emscripten_postinvoke");
  if (!PostInvoke) PostInvoke = Function::Create(IntFunc, GlobalValue::ExternalLinkage, "emscripten_postinvoke", TheModule);

  // Process all callers of setjmp and longjmp. Start with setjmp.

  typedef std::vector<PHINode*> Phis;
  typedef std::map<Function*, Phis> FunctionPhisMap;
  FunctionPhisMap SetjmpOutputPhis;

  if (Setjmp) {
    for (Instruction::use_iterator UI = Setjmp->use_begin(), UE = Setjmp->use_end(); UI != UE; ++UI) {
      if (CallInst *CI = dyn_cast<CallInst>(*UI)) {
        BasicBlock *SJBB = CI->getParent();
        // The tail is everything right after the call, and will be reached once when setjmp is
        // called, and later when longjmp returns to the setjmp
        BasicBlock *Tail = SplitBlock(SJBB, CI->getNextNode(), this);
        // Add a phi to the tail, which will be the output of setjmp, which indicates if this is the
        // first call or a longjmp back. The phi directly uses the right value based on where we
        // arrive from
        PHINode *SetjmpOutput = PHINode::Create(i32, 2, "", Tail->getFirstNonPHI());
        SetjmpOutput->addIncoming(ConstantInt::get(i32, 0), SJBB); // setjmp initial call returns 0
        CI->replaceAllUsesWith(SetjmpOutput); // The proper output is now this, not the setjmp call itself
        // longjmp returns to the setjmp will add themselves to this phi
        Phis& P = SetjmpOutputPhis[SJBB->getParent()];
        P.push_back(SetjmpOutput);
        // fix call target
        SmallVector<Value *, 2> Args;
        Args.push_back(CI->getArgOperand(0));
        Args.push_back(ConstantInt::get(i32, P.size())); // our index in the function is our place in the array + 1
        CallInst::Create(EmSetjmp, Args, "", CI);
        CI->eraseFromParent();
      } else {
        errs() << **UI << "\n";
        report_fatal_error("bad use of setjmp, should only call it");
      }
    }
  }

  // Update longjmp FIXME: we could avoid throwing in longjmp as an optimization when longjmping back into the current function perhaps?

  if (Longjmp) Longjmp->replaceAllUsesWith(EmLongjmp);

  // Update all setjmping functions

  for (FunctionPhisMap::iterator I = SetjmpOutputPhis.begin(); I != SetjmpOutputPhis.end(); I++) {
    Function *F = I->first;
    Phis& P = I->second;

    CallInst::Create(PrepSetjmp, "", F->begin()->begin()); // FIXME: adding after other allocas might be better

    // Update each call that can longjmp so it can return to a setjmp where relevant

    for (Function::iterator BBI = F->begin(), E = F->end(); BBI != E; ) {
      BasicBlock *BB = BBI++;
      for (BasicBlock::iterator Iter = BB->begin(), E = BB->end(); Iter != E; ) {
        Instruction *I = Iter++;
        CallInst *CI;
        if ((CI = dyn_cast<CallInst>(I))) {
          Value *V = CI->getCalledValue();
          if (V == PrepSetjmp || V == EmSetjmp || V == CheckLongjmp || V == GetLongjmpResult || V == PreInvoke || V == PostInvoke) continue;
          if (Function *CF = dyn_cast<Function>(V)) if (CF->isIntrinsic()) continue;
          // TODO: proper analysis of what can actually longjmp. Currently we assume anything but setjmp can.
          // This may longjmp, so we need to check if it did. Split at that point, and
          // envelop the call in pre/post invoke, if we need to
          CallInst *After;
          Instruction *Check = NULL;
          if (Iter != E && (After = dyn_cast<CallInst>(Iter)) && After->getCalledValue() == PostInvoke) {
            // use the pre|postinvoke that exceptions lowering already made
            Check = Iter++;
          }
          BasicBlock *Tail = SplitBlock(BB, Iter, this); // Iter already points to the next instruction, as we need
          TerminatorInst *TI = BB->getTerminator();
          if (!Check) {
            // no existing pre|postinvoke, create our own
            CallInst::Create(PreInvoke, "", CI);
            Check = CallInst::Create(PostInvoke, "", TI); // CI is at the end of the block
          }

          // We need to replace the terminator in Tail - SplitBlock makes BB go straight to Tail, we need to check if a longjmp occurred, and
          // go to the right setjmp-tail if so
          SmallVector<Value *, 1> Args;
          Args.push_back(Check);
          Instruction *LongjmpCheck = CallInst::Create(CheckLongjmp, Args, "", BB);
          Instruction *LongjmpResult = CallInst::Create(GetLongjmpResult, Args, "", BB);
          SwitchInst *SI = SwitchInst::Create(LongjmpCheck, Tail, 2, BB);
          // -1 means no longjmp happened, continue normally (will hit the default switch case). 0 means a longjmp that is not ours to handle, needs a rethrow. Otherwise
          // the index mean is the same as the index in P+1 (to avoid 0).
          for (unsigned i = 0; i < P.size(); i++) {
            SI->addCase(cast<ConstantInt>(ConstantInt::get(i32, i+1)), P[i]->getParent());
            P[i]->addIncoming(LongjmpResult, BB);
          }
          TI->eraseFromParent(); // new terminator is now the switch

          // we are splitting the block here, and must continue to find other calls in the block - which is now split. so continue
          // to traverse in the Tail
          BB = Tail;
          Iter = BB->begin();
          E = BB->end();
        } else if (InvokeInst *CI = dyn_cast<InvokeInst>(I)) { // XXX check if target is setjmp
          assert("TODO: invoke inside setjmping functions");
        }
      }
    }
  }

  return true;
}

ModulePass *llvm::createLowerEmSetjmpPass() {
  return new LowerEmSetjmp();
}

