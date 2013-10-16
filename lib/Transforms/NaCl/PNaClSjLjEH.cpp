//===- PNaClSjLjEH.cpp - Lower C++ exception handling to use setjmp()------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// The PNaClSjLjEH pass is part of an implementation of C++ exception
// handling for PNaCl that uses setjmp() and longjmp() to handle C++
// exceptions.  The pass lowers LLVM "invoke" instructions to use
// setjmp().
//
// For example, consider the following C++ code fragment:
//
//   int catcher_func() {
//     try {
//       int result = external_func();
//       return result + 100;
//     } catch (MyException &exc) {
//       return exc.value + 200;
//     }
//   }
//
// PNaClSjLjEH converts the IR for that function to the following
// pseudo-code:
//
//   struct LandingPadResult {
//     void *exception_obj;  // For passing to __cxa_begin_catch()
//     int matched_clause_id;  // See ExceptionInfoWriter.cpp
//   };
//
//   struct ExceptionFrame {
//     union {
//       jmp_buf jmpbuf;  // Context for jumping to landingpad block
//       struct LandingPadResult result;  // Data returned to landingpad block
//     };
//     struct ExceptionFrame *next;  // Next frame in linked list
//     int clause_list_id;  // Reference to landingpad's exception info
//   };
//
//   // Thread-local exception state
//   __thread struct ExceptionFrame *__pnacl_eh_stack;
//
//   int catcher_func() {
//     struct ExceptionFrame frame;
//     int result;
//     if (!setjmp(&frame.jmpbuf)) {  // Save context
//       frame.next = __pnacl_eh_stack;
//       frame.clause_list_id = 123;
//       __pnacl_eh_stack = &frame;  // Add frame to stack
//       result = external_func();
//       __pnacl_eh_stack = frame.next;  // Remove frame from stack
//     } else {
//       // Handle exception.  This is a simplification.  Real code would
//       // call __cxa_begin_catch() to extract the thrown object.
//       MyException &exc = *(MyException *) frame.result.exception_obj;
//       return exc.value + 200;
//     }
//     return result + 100;
//   }
//
// The pass makes the following changes to IR:
//
//  * Convert "invoke" and "landingpad" instructions.
//  * Convert "resume" instructions into __pnacl_eh_resume() calls.
//  * Replace each call to llvm.eh.typeid.for() with an integer
//    constant representing the exception type.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/NaCl.h"
#include "ExceptionInfoWriter.h"

using namespace llvm;

namespace {
  // This is a ModulePass so that it can introduce new global variables.
  class PNaClSjLjEH : public ModulePass {
  public:
    static char ID; // Pass identification, replacement for typeid
    PNaClSjLjEH() : ModulePass(ID) {
      initializePNaClSjLjEHPass(*PassRegistry::getPassRegistry());
    }

    virtual bool runOnModule(Module &M);
  };

  class FuncRewriter {
    Type *ExceptionFrameTy;
    ExceptionInfoWriter *ExcInfoWriter;
    Function *Func;

    // FrameInitialized indicates whether the following variables have
    // been initialized.
    bool FrameInitialized;
    Function *SetjmpIntrinsic;  // setjmp() intrinsic function
    Instruction *EHStackTlsVar;  // Bitcast of thread-local __pnacl_eh_stack var
    Instruction *Frame;  // Frame allocated for this function
    Instruction *FrameJmpBuf;  // Frame's jmp_buf field
    Instruction *FrameNextPtr;  // Frame's next field
    Instruction *FrameExcInfo;  // Frame's clause_list_id field

    Function *EHResumeFunc;  // __pnacl_eh_resume() function

    // Initialize values that are shared across all "invoke"
    // instructions within the function.
    void initializeFrame();

  public:
    FuncRewriter(Type *ExceptionFrameTy, ExceptionInfoWriter *ExcInfoWriter,
                 Function *Func):
        ExceptionFrameTy(ExceptionFrameTy),
        ExcInfoWriter(ExcInfoWriter),
        Func(Func),
        FrameInitialized(false),
        SetjmpIntrinsic(NULL), EHStackTlsVar(NULL),
        Frame(NULL), FrameJmpBuf(NULL), FrameNextPtr(NULL), FrameExcInfo(NULL),
        EHResumeFunc(NULL) {}

    void expandInvokeInst(InvokeInst *Invoke);
    void expandResumeInst(ResumeInst *Resume);
    void expandFunc();
  };
}

char PNaClSjLjEH::ID = 0;
INITIALIZE_PASS(PNaClSjLjEH, "pnacl-sjlj-eh",
                "Lower C++ exception handling to use setjmp()",
                false, false)

static const int kPNaClJmpBufSize = 1024;
static const int kPNaClJmpBufAlign = 8;

void FuncRewriter::initializeFrame() {
  if (FrameInitialized)
    return;
  FrameInitialized = true;
  Module *M = Func->getParent();

  SetjmpIntrinsic = Intrinsic::getDeclaration(M, Intrinsic::nacl_setjmp);

  Value *EHStackTlsVarUncast = M->getGlobalVariable("__pnacl_eh_stack");
  if (!EHStackTlsVarUncast)
    report_fatal_error("__pnacl_eh_stack not defined");
  EHStackTlsVar = new BitCastInst(
      EHStackTlsVarUncast, ExceptionFrameTy->getPointerTo()->getPointerTo(),
      "pnacl_eh_stack");
  Func->getEntryBlock().getInstList().push_front(EHStackTlsVar);

  // Allocate the new exception frame.  This is reused across all
  // invoke instructions in the function.
  Type *I32 = Type::getInt32Ty(M->getContext());
  Frame = new AllocaInst(ExceptionFrameTy, ConstantInt::get(I32, 1),
                         kPNaClJmpBufAlign, "invoke_frame");
  Func->getEntryBlock().getInstList().push_front(Frame);

  // Calculate addresses of fields in the exception frame.
  Value *JmpBufIndexes[] = { ConstantInt::get(I32, 0),
                             ConstantInt::get(I32, 0),
                             ConstantInt::get(I32, 0) };
  FrameJmpBuf = GetElementPtrInst::Create(Frame, JmpBufIndexes,
                                          "invoke_jmp_buf");
  FrameJmpBuf->insertAfter(Frame);

  Value *NextPtrIndexes[] = { ConstantInt::get(I32, 0),
                              ConstantInt::get(I32, 1) };
  FrameNextPtr = GetElementPtrInst::Create(Frame, NextPtrIndexes,
                                           "invoke_next");
  FrameNextPtr->insertAfter(Frame);

  Value *ExcInfoIndexes[] = { ConstantInt::get(I32, 0),
                              ConstantInt::get(I32, 2) };
  FrameExcInfo = GetElementPtrInst::Create(Frame, ExcInfoIndexes,
                                           "exc_info_ptr");
  FrameExcInfo->insertAfter(Frame);
}

static void updateEdge(BasicBlock *Dest,
                       BasicBlock *OldIncoming,
                       BasicBlock *NewIncoming) {
  for (BasicBlock::iterator Inst = Dest->begin(); Inst != Dest->end(); ++Inst) {
    PHINode *Phi = dyn_cast<PHINode>(Inst);
    if (!Phi)
      break;
    for (unsigned I = 0, E = Phi->getNumIncomingValues(); I < E; ++I) {
      if (Phi->getIncomingBlock(I) == OldIncoming)
        Phi->setIncomingBlock(I, NewIncoming);
    }
  }
}

void FuncRewriter::expandInvokeInst(InvokeInst *Invoke) {
  initializeFrame();

  LandingPadInst *LP = Invoke->getLandingPadInst();
  Type *I32 = Type::getInt32Ty(Func->getContext());
  Value *ExcInfo = ConstantInt::get(
      I32, ExcInfoWriter->getIDForLandingPadClauseList(LP));

  // Create setjmp() call.
  Value *SetjmpArgs[] = { FrameJmpBuf };
  Value *SetjmpCall = CopyDebug(CallInst::Create(SetjmpIntrinsic, SetjmpArgs,
                                                 "invoke_sj", Invoke), Invoke);
  // Check setjmp()'s result.
  Value *IsZero = CopyDebug(new ICmpInst(Invoke, CmpInst::ICMP_EQ, SetjmpCall,
                                         ConstantInt::get(I32, 0),
                                         "invoke_sj_is_zero"), Invoke);

  BasicBlock *CallBB = BasicBlock::Create(Func->getContext(), "invoke_do_call",
                                          Func);
  CallBB->moveAfter(Invoke->getParent());

  // Append the new frame to the list.
  Value *OldList = CopyDebug(
      new LoadInst(EHStackTlsVar, "old_eh_stack", CallBB), Invoke);
  CopyDebug(new StoreInst(OldList, FrameNextPtr, CallBB), Invoke);
  CopyDebug(new StoreInst(ExcInfo, FrameExcInfo, CallBB), Invoke);
  CopyDebug(new StoreInst(Frame, EHStackTlsVar, CallBB), Invoke);

  SmallVector<Value *, 10> CallArgs;
  for (unsigned I = 0, E = Invoke->getNumArgOperands(); I < E; ++I)
    CallArgs.push_back(Invoke->getArgOperand(I));
  CallInst *NewCall = CallInst::Create(Invoke->getCalledValue(), CallArgs, "",
                                       CallBB);
  CopyDebug(NewCall, Invoke);
  NewCall->takeName(Invoke);
  NewCall->setAttributes(Invoke->getAttributes());
  NewCall->setCallingConv(Invoke->getCallingConv());
  // Restore the old frame list.  We only need to do this on the
  // non-exception code path.  If an exception is raised, the frame
  // list state will be restored for us.
  CopyDebug(new StoreInst(OldList, EHStackTlsVar, CallBB), Invoke);

  CopyDebug(BranchInst::Create(CallBB, Invoke->getUnwindDest(), IsZero, Invoke),
            Invoke);
  CopyDebug(BranchInst::Create(Invoke->getNormalDest(), CallBB), Invoke);

  updateEdge(Invoke->getNormalDest(), Invoke->getParent(), CallBB);

  Invoke->replaceAllUsesWith(NewCall);
  Invoke->eraseFromParent();
}

void FuncRewriter::expandResumeInst(ResumeInst *Resume) {
  if (!EHResumeFunc) {
    EHResumeFunc = Func->getParent()->getFunction("__pnacl_eh_resume");
    if (!EHResumeFunc)
      report_fatal_error("__pnacl_eh_resume() not defined");
  }

  // The "resume" instruction gets passed the landingpad's full result
  // (struct LandingPadResult above).  Extract the exception_obj field
  // to pass to __pnacl_eh_resume(), which doesn't need the
  // matched_clause_id field.
  unsigned Indexes[] = { 0 };
  Value *ExceptionPtr =
      CopyDebug(ExtractValueInst::Create(Resume->getValue(), Indexes,
                                         "resume_exc", Resume), Resume);

  // Cast to the pointer type that __pnacl_eh_resume() expects.
  if (EHResumeFunc->getFunctionType()->getFunctionNumParams() != 1)
    report_fatal_error("Bad type for __pnacl_eh_resume()");
  Type *ArgType = EHResumeFunc->getFunctionType()->getFunctionParamType(0);
  ExceptionPtr = new BitCastInst(ExceptionPtr, ArgType, "resume_cast", Resume);

  Value *Args[] = { ExceptionPtr };
  CopyDebug(CallInst::Create(EHResumeFunc, Args, "", Resume), Resume);
  new UnreachableInst(Func->getContext(), Resume);
  Resume->eraseFromParent();
}

void FuncRewriter::expandFunc() {
  Type *I32 = Type::getInt32Ty(Func->getContext());

  // We need to do two passes: When we process an invoke we need to
  // look at its landingpad, so we can't remove the landingpads until
  // all the invokes have been processed.
  for (Function::iterator BB = Func->begin(), E = Func->end(); BB != E; ++BB) {
    for (BasicBlock::iterator Iter = BB->begin(), E = BB->end(); Iter != E; ) {
      Instruction *Inst = Iter++;
      if (InvokeInst *Invoke = dyn_cast<InvokeInst>(Inst)) {
        expandInvokeInst(Invoke);
      } else if (ResumeInst *Resume = dyn_cast<ResumeInst>(Inst)) {
        expandResumeInst(Resume);
      } else if (IntrinsicInst *Intrinsic = dyn_cast<IntrinsicInst>(Inst)) {
        if (Intrinsic->getIntrinsicID() == Intrinsic::eh_typeid_for) {
          Value *ExcType = Intrinsic->getArgOperand(0);
          Value *Val = ConstantInt::get(
              I32, ExcInfoWriter->getIDForExceptionType(ExcType));
          Intrinsic->replaceAllUsesWith(Val);
          Intrinsic->eraseFromParent();
        }
      }
    }
  }
  for (Function::iterator BB = Func->begin(), E = Func->end(); BB != E; ++BB) {
    for (BasicBlock::iterator Iter = BB->begin(), E = BB->end(); Iter != E; ) {
      Instruction *Inst = Iter++;
      if (LandingPadInst *LP = dyn_cast<LandingPadInst>(Inst)) {
        initializeFrame();
        Value *LPPtr = new BitCastInst(
            FrameJmpBuf, LP->getType()->getPointerTo(), "landingpad_ptr", LP);
        Value *LPVal = CopyDebug(new LoadInst(LPPtr, "", LP), LP);
        LPVal->takeName(LP);
        LP->replaceAllUsesWith(LPVal);
        LP->eraseFromParent();
      }
    }
  }
}

bool PNaClSjLjEH::runOnModule(Module &M) {
  Type *JmpBufTy = ArrayType::get(Type::getInt8Ty(M.getContext()),
                                  kPNaClJmpBufSize);

  // Define "struct ExceptionFrame".
  StructType *ExceptionFrameTy = StructType::create(M.getContext(),
                                                    "ExceptionFrame");
  Type *ExceptionFrameFields[] = {
    JmpBufTy,  // jmp_buf
    ExceptionFrameTy->getPointerTo(),  // struct ExceptionFrame *next
    Type::getInt32Ty(M.getContext())  // Exception info (clause list ID)
  };
  ExceptionFrameTy->setBody(ExceptionFrameFields);

  ExceptionInfoWriter ExcInfoWriter(&M.getContext());
  for (Module::iterator Func = M.begin(), E = M.end(); Func != E; ++Func) {
    FuncRewriter Rewriter(ExceptionFrameTy, &ExcInfoWriter, Func);
    Rewriter.expandFunc();
  }
  ExcInfoWriter.defineGlobalVariables(&M);
  return true;
}

ModulePass *llvm::createPNaClSjLjEHPass() {
  return new PNaClSjLjEH();
}
