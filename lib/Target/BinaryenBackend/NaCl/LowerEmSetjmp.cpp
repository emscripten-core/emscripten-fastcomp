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
#include "llvm/IR/Dominators.h"
#include "llvm/Transforms/Utils/PromoteMemToReg.h"
#include <vector>
#include <set>
#include <list>

#include "llvm/Support/raw_ostream.h"

#ifdef NDEBUG
#undef assert
#define assert(x) { if (!(x)) report_fatal_error(#x); }
#endif

using namespace llvm;

// Utilities for mem/reg: based on Reg2Mem and MemToReg

bool valueEscapes(const Instruction *Inst) {
  const BasicBlock *BB = Inst->getParent();
  for (Value::const_user_iterator UI = Inst->user_begin(),E = Inst->user_end();
       UI != E; ++UI) {
    const User *U = *UI;
    const Instruction *I = cast<Instruction>(U);
    if (I->getParent() != BB || isa<PHINode>(I))
      return true;
  }
  return false;
}

void doRegToMem(Function &F) { // see Reg2Mem.cpp
  // Insert all new allocas into entry block.
  BasicBlock *BBEntry = &F.getEntryBlock();
  assert(pred_begin(BBEntry) == pred_end(BBEntry) &&
         "Entry block to function must not have predecessors!");

  // Find first non-alloca instruction and create insertion point. This is
  // safe if block is well-formed: it always have terminator, otherwise
  // we'll get and assertion.
  BasicBlock::iterator I = BBEntry->begin();
  while (isa<AllocaInst>(I)) ++I;

  CastInst *AllocaInsertionPoint =
    new BitCastInst(Constant::getNullValue(Type::getInt32Ty(F.getContext())),
                    Type::getInt32Ty(F.getContext()),
                    "reg2mem alloca point", &*I);

  // Find the escaped instructions. But don't create stack slots for
  // allocas in entry block.
  std::list<Instruction*> WorkList;
  for (Function::iterator ibb = F.begin(), ibe = F.end();
       ibb != ibe; ++ibb)
    for (BasicBlock::iterator iib = ibb->begin(), iie = ibb->end();
         iib != iie; ++iib) {
      if (!(isa<AllocaInst>(iib) && iib->getParent() == BBEntry) &&
          valueEscapes(&*iib)) {
        WorkList.push_front(&*iib);
      }
    }

  // Demote escaped instructions
  for (std::list<Instruction*>::iterator ilb = WorkList.begin(),
       ile = WorkList.end(); ilb != ile; ++ilb)
    DemoteRegToStack(**ilb, false, AllocaInsertionPoint);

  WorkList.clear();

  // Find all phi's
  for (Function::iterator ibb = F.begin(), ibe = F.end();
       ibb != ibe; ++ibb)
    for (BasicBlock::iterator iib = ibb->begin(), iie = ibb->end();
         iib != iie; ++iib)
      if (isa<PHINode>(iib))
        WorkList.push_front(&*iib);

  // Demote phi nodes
  for (std::list<Instruction*>::iterator ilb = WorkList.begin(),
       ile = WorkList.end(); ilb != ile; ++ilb)
    DemotePHIToStack(cast<PHINode>(*ilb), AllocaInsertionPoint);
}

void doMemToReg(Function &F) {
  std::vector<AllocaInst*> Allocas;

  BasicBlock &BB = F.getEntryBlock();  // Get the entry node for the function

  DominatorTreeWrapperPass DTW;
  DTW.runOnFunction(F);
  DominatorTree& DT = DTW.getDomTree();

  while (1) {
    Allocas.clear();

    // Find allocas that are safe to promote, by looking at all instructions in
    // the entry node
    for (BasicBlock::iterator I = BB.begin(), E = --BB.end(); I != E; ++I)
      if (AllocaInst *AI = dyn_cast<AllocaInst>(I))       // Is it an alloca?
        if (isAllocaPromotable(AI))
          Allocas.push_back(AI);

    if (Allocas.empty()) break;

    PromoteMemToReg(Allocas, DT);
  }
}

// LowerEmSetjmp

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
  FunctionType *VoidIntFunc = FunctionType::get(Void, IntArgTypes, false);

  Function *CheckLongjmp = Function::Create(IntIntFunc, GlobalValue::ExternalLinkage, "emscripten_check_longjmp", TheModule); // gets control flow

  Function *GetLongjmpResult = Function::Create(IntIntFunc, GlobalValue::ExternalLinkage, "emscripten_get_longjmp_result", TheModule); // gets int value longjmp'd

  FunctionType *VoidFunc = FunctionType::get(Void, false);
  Function *PrepSetjmp = Function::Create(VoidFunc, GlobalValue::ExternalLinkage, "emscripten_prep_setjmp", TheModule);

  Function *CleanupSetjmp = Function::Create(VoidFunc, GlobalValue::ExternalLinkage, "emscripten_cleanup_setjmp", TheModule);

  Function *PreInvoke = TheModule->getFunction("emscripten_preinvoke");
  if (!PreInvoke) PreInvoke = Function::Create(VoidIntFunc, GlobalValue::ExternalLinkage, "emscripten_preinvoke", TheModule);

  Function *PostInvoke = TheModule->getFunction("emscripten_postinvoke");
  if (!PostInvoke) PostInvoke = Function::Create(IntIntFunc, GlobalValue::ExternalLinkage, "emscripten_postinvoke", TheModule);

  // Process all callers of setjmp and longjmp. Start with setjmp.

  typedef std::vector<PHINode*> Phis;
  typedef std::map<Function*, Phis> FunctionPhisMap;
  FunctionPhisMap SetjmpOutputPhis;
  std::vector<Instruction*> ToErase;

  if (Setjmp) {
    for (Instruction::user_iterator UI = Setjmp->user_begin(), UE = Setjmp->user_end(); UI != UE; ++UI) {
      User *U = *UI;
      if (CallInst *CI = dyn_cast<CallInst>(U)) {
        BasicBlock *SJBB = CI->getParent();
        // The tail is everything right after the call, and will be reached once when setjmp is
        // called, and later when longjmp returns to the setjmp
        BasicBlock *Tail = SplitBlock(SJBB, CI->getNextNode());
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
        ToErase.push_back(CI);
      } else {
        errs() << **UI << "\n";
        report_fatal_error("bad use of setjmp, should only call it");
      }
    }
  }

  // Update longjmp FIXME: we could avoid throwing in longjmp as an optimization when longjmping back into the current function perhaps?

  if (Longjmp) Longjmp->replaceAllUsesWith(EmLongjmp);

  // Update all setjmping functions

  unsigned InvokeId = 0;

  for (FunctionPhisMap::iterator I = SetjmpOutputPhis.begin(); I != SetjmpOutputPhis.end(); I++) {
    Function *F = I->first;
    Phis& P = I->second;

    CallInst::Create(PrepSetjmp, "", &*F->begin()->begin());

    // Update each call that can longjmp so it can return to a setjmp where relevant

    for (Function::iterator BBI = F->begin(), E = F->end(); BBI != E; ) {
      BasicBlock *BB = &*BBI++;
      for (BasicBlock::iterator Iter = BB->begin(), E = BB->end(); Iter != E; ) {
        Instruction *I = &*Iter++;
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
            Check = &*Iter++;
          }
          BasicBlock *Tail = SplitBlock(BB, &*Iter); // Iter already points to the next instruction, as we need
          TerminatorInst *TI = BB->getTerminator();
          if (!Check) {
            // no existing pre|postinvoke, create our own
            SmallVector<Value*,1> HelperArgs;
            HelperArgs.push_back(ConstantInt::get(i32, InvokeId++));

            CallInst::Create(PreInvoke, HelperArgs, "", CI);
            Check = CallInst::Create(PostInvoke, HelperArgs, "", TI); // CI is at the end of the block

            // If we are calling a function that is noreturn, we must remove that attribute. The code we
            // insert here does expect it to return, after we catch the exception.
            if (CI->doesNotReturn()) {
              if (Function *F = dyn_cast<Function>(CI->getCalledValue())) {
                F->removeFnAttr(Attribute::NoReturn);
              }
              CI->setAttributes(CI->getAttributes().removeAttribute(TheModule->getContext(), AttributeSet::FunctionIndex, Attribute::NoReturn));
              assert(!CI->doesNotReturn());
            }
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
          ToErase.push_back(TI); // new terminator is now the switch

          // we are splitting the block here, and must continue to find other calls in the block - which is now split. so continue
          // to traverse in the Tail
          BB = Tail;
          Iter = BB->begin();
          E = BB->end();
        } else if (InvokeInst *CI = dyn_cast<InvokeInst>(I)) { // XXX check if target is setjmp
          (void)CI;
          report_fatal_error("TODO: invoke inside setjmping functions");
        }
      }
    }

    // add a cleanup before each return
    for (Function::iterator BBI = F->begin(), E = F->end(); BBI != E; ) {
      BasicBlock *BB = &*BBI++;
      TerminatorInst *TI = BB->getTerminator();
      if (isa<ReturnInst>(TI)) {
        CallInst::Create(CleanupSetjmp, "", TI);
      }
    }
  }

  for (unsigned i = 0; i < ToErase.size(); i++) {
    ToErase[i]->eraseFromParent();
  }

  // Finally, our modifications to the cfg can break dominance of SSA variables. For example,
  //   if (x()) { .. setjmp() .. }
  //   if (y()) { .. longjmp() .. }
  // We must split the longjmp block, and it can jump into the setjmp one. But that means that when
  // we split the setjmp block, it's first part no longer dominates its second part - there is
  // a theoretically possible control flow path where x() is false, then y() is true and we
  // reach the second part of the setjmp block, without ever reaching the first part. So,
  // we recalculate regs vs. mem
  for (FunctionPhisMap::iterator I = SetjmpOutputPhis.begin(); I != SetjmpOutputPhis.end(); I++) {
    Function *F = I->first;
    doRegToMem(*F);
    doMemToReg(*F);
  }

  return true;
}

ModulePass *llvm::createLowerEmSetjmpPass() {
  return new LowerEmSetjmp();
}
