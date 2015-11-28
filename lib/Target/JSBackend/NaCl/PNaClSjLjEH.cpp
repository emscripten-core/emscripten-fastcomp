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
//     frame.next = __pnacl_eh_stack;
//     frame.clause_list_id = 123;
//     __pnacl_eh_stack = &frame;  // Add frame to stack
//     int result;
//     if (!catcher_func_setjmp_caller(external_func, &frame.jmpbuf, &result)) {
//       __pnacl_eh_stack = frame.next;  // Remove frame from stack
//       return result + 100;
//     } else {
//       // Handle exception.  This is a simplification.  Real code would
//       // call __cxa_begin_catch() to extract the thrown object.
//       MyException &exc = *(MyException *) frame.result.exception_obj;
//       return exc.value + 200;
//     }
//   }
//
//   // Helper function
//   static int catcher_func_setjmp_caller(int (*func)(void), jmp_buf jmpbuf,
//                                         int *result) {
//     if (!setjmp(jmpbuf)) {
//       *result = func();
//       return 0;
//     }
//     return 1;
//   }
//
// We use a helper function so that setjmp() is not called directly
// from catcher_func(), due to a quirk of how setjmp() and longjmp()
// are specified in C.
//
// func() might modify variables (allocas) that are local to
// catcher_func() (if the variables' addresses are taken).  The C
// standard says that these variables' values would become undefined
// after longjmp() returned if setjmp() were called from
// catcher_func().  Specifically, LLVM's GVN pass can optimize away
// stores to allocas between setjmp() and longjmp() (see
// pnacl-sjlj-eh-bug.ll for an example).  But this only applies to
// allocas inside the caller of setjmp(), not to allocas inside the
// caller of the caller of setjmp(), so doing the setjmp() call inside
// a helper function that catcher_func() calls avoids the problem.
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

    Value *createSetjmpWrappedCall(InvokeInst *Invoke);
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
  FrameJmpBuf = GetElementPtrInst::Create(
      ExceptionFrameTy, Frame, JmpBufIndexes, "invoke_jmp_buf");
  FrameJmpBuf->insertAfter(Frame);

  Value *NextPtrIndexes[] = { ConstantInt::get(I32, 0),
                              ConstantInt::get(I32, 1) };
  FrameNextPtr = GetElementPtrInst::Create(
      ExceptionFrameTy, Frame, NextPtrIndexes, "invoke_next");
  FrameNextPtr->insertAfter(Frame);

  Value *ExcInfoIndexes[] = { ConstantInt::get(I32, 0),
                              ConstantInt::get(I32, 2) };
  FrameExcInfo = GetElementPtrInst::Create(
      ExceptionFrameTy, Frame, ExcInfoIndexes, "exc_info_ptr");
  FrameExcInfo->insertAfter(Frame);
}

// Creates the helper function that will do the setjmp() call and
// function call for implementing Invoke.  Creates the call to the
// helper function.  Returns a Value which is zero on the normal
// execution path and non-zero if the landingpad block should be
// entered.
Value *FuncRewriter::createSetjmpWrappedCall(InvokeInst *Invoke) {
  Type *I32 = Type::getInt32Ty(Func->getContext());

  // Allocate space for storing the invoke's result temporarily (so
  // that the helper function can return multiple values).  We don't
  // need to do this if the result is unused, and we can't if its type
  // is void.
  Instruction *ResultAlloca = NULL;
  if (!Invoke->use_empty()) {
    ResultAlloca = new AllocaInst(Invoke->getType(), "invoke_result_ptr");
    Func->getEntryBlock().getInstList().push_front(ResultAlloca);
  }

  // Create type for the helper function.
  SmallVector<Type *, 10> ArgTypes;
  for (unsigned I = 0, E = Invoke->getNumArgOperands(); I < E; ++I)
    ArgTypes.push_back(Invoke->getArgOperand(I)->getType());
  ArgTypes.push_back(Invoke->getCalledValue()->getType());
  ArgTypes.push_back(FrameJmpBuf->getType());
  if (ResultAlloca)
    ArgTypes.push_back(Invoke->getType()->getPointerTo());
  FunctionType *FTy = FunctionType::get(I32, ArgTypes, false);

  // Create the helper function.
  Function *HelperFunc = Function::Create(
      FTy, GlobalValue::InternalLinkage, Func->getName() + "_setjmp_caller");
  Func->getParent()->getFunctionList().insertAfter(Func->getIterator(), HelperFunc);
  BasicBlock *EntryBB = BasicBlock::Create(Func->getContext(), "", HelperFunc);
  BasicBlock *NormalBB = BasicBlock::Create(Func->getContext(), "normal",
                                            HelperFunc);
  BasicBlock *ExceptionBB = BasicBlock::Create(Func->getContext(), "exception",
                                               HelperFunc);

  // Unpack the helper function's arguments.
  Function::arg_iterator ArgIter = HelperFunc->arg_begin();
  SmallVector<Value *, 10> InnerCallArgs;
  for (unsigned I = 0, E = Invoke->getNumArgOperands(); I < E; ++I) {
    ArgIter->setName("arg");
    InnerCallArgs.push_back(&*ArgIter++);
  }
  Argument *CalleeArg = &*ArgIter++;
  Argument *JmpBufArg = &*ArgIter++;
  CalleeArg->setName("func_ptr");
  JmpBufArg->setName("jmp_buf");

  // Create setjmp() call.
  Value *SetjmpArgs[] = { JmpBufArg };
  CallInst *SetjmpCall = CallInst::Create(SetjmpIntrinsic, SetjmpArgs,
                                          "invoke_sj", EntryBB);
  CopyDebug(SetjmpCall, Invoke);
  // Setting the "returns_twice" attribute here prevents optimization
  // passes from inlining HelperFunc into its caller.
  SetjmpCall->setCanReturnTwice();
  // Check setjmp()'s result.
  Value *IsZero = CopyDebug(new ICmpInst(*EntryBB, CmpInst::ICMP_EQ, SetjmpCall,
                                         ConstantInt::get(I32, 0),
                                         "invoke_sj_is_zero"), Invoke);
  CopyDebug(BranchInst::Create(NormalBB, ExceptionBB, IsZero, EntryBB), Invoke);
  // Handle the normal, non-exceptional code path.
  CallInst *InnerCall = CallInst::Create(CalleeArg, InnerCallArgs, "",
                                         NormalBB);
  CopyDebug(InnerCall, Invoke);
  InnerCall->setAttributes(Invoke->getAttributes());
  InnerCall->setCallingConv(Invoke->getCallingConv());
  if (ResultAlloca) {
    InnerCall->setName("result");
    Argument *ResultArg = &*ArgIter++;
    ResultArg->setName("result_ptr");
    CopyDebug(new StoreInst(InnerCall, ResultArg, NormalBB), Invoke);
  }
  ReturnInst::Create(Func->getContext(), ConstantInt::get(I32, 0), NormalBB);
  // Handle the exceptional code path.
  ReturnInst::Create(Func->getContext(), ConstantInt::get(I32, 1), ExceptionBB);

  // Create the outer call to the helper function.
  SmallVector<Value *, 10> OuterCallArgs;
  for (unsigned I = 0, E = Invoke->getNumArgOperands(); I < E; ++I)
    OuterCallArgs.push_back(Invoke->getArgOperand(I));
  OuterCallArgs.push_back(Invoke->getCalledValue());
  OuterCallArgs.push_back(FrameJmpBuf);
  if (ResultAlloca)
    OuterCallArgs.push_back(ResultAlloca);
  CallInst *OuterCall = CallInst::Create(HelperFunc, OuterCallArgs,
                                         "invoke_is_exc", Invoke);
  CopyDebug(OuterCall, Invoke);

  // Retrieve the function return value stored in the alloca.  We only
  // need to do this on the non-exceptional path, but we currently do
  // it unconditionally because that is simpler.
  if (ResultAlloca) {
    Value *Result = new LoadInst(ResultAlloca, "", Invoke);
    Result->takeName(Invoke);
    Invoke->replaceAllUsesWith(Result);
  }
  return OuterCall;
}

static void convertInvokeToCall(InvokeInst *Invoke) {
  SmallVector<Value*, 16> CallArgs(Invoke->op_begin(), Invoke->op_end() - 3);
  // Insert a normal call instruction.
  CallInst *NewCall = CallInst::Create(Invoke->getCalledValue(),
                                       CallArgs, "", Invoke);
  CopyDebug(NewCall, Invoke);
  NewCall->takeName(Invoke);
  NewCall->setCallingConv(Invoke->getCallingConv());
  NewCall->setAttributes(Invoke->getAttributes());
  Invoke->replaceAllUsesWith(NewCall);

  // Insert an unconditional branch to the normal destination.
  BranchInst::Create(Invoke->getNormalDest(), Invoke);
  // Remove any PHI node entries from the exception destination.
  Invoke->getUnwindDest()->removePredecessor(Invoke->getParent());
  Invoke->eraseFromParent();
}

void FuncRewriter::expandInvokeInst(InvokeInst *Invoke) {
  // Calls to ReturnsTwice functions, i.e. setjmp(), can't be moved
  // into a helper function.  setjmp() can't throw an exception
  // anyway, so convert the invoke to a call.
  if (Invoke->hasFnAttr(Attribute::ReturnsTwice)) {
    convertInvokeToCall(Invoke);
    return;
  }

  initializeFrame();

  LandingPadInst *LP = Invoke->getLandingPadInst();
  Type *I32 = Type::getInt32Ty(Func->getContext());
  Value *ExcInfo = ConstantInt::get(
      I32, ExcInfoWriter->getIDForLandingPadClauseList(LP));

  // Append the new frame to the list.
  Value *OldList = CopyDebug(
      new LoadInst(EHStackTlsVar, "old_eh_stack", Invoke), Invoke);
  CopyDebug(new StoreInst(OldList, FrameNextPtr, Invoke), Invoke);
  CopyDebug(new StoreInst(ExcInfo, FrameExcInfo, Invoke), Invoke);
  CopyDebug(new StoreInst(Frame, EHStackTlsVar, Invoke), Invoke);
  Value *IsException = createSetjmpWrappedCall(Invoke);
  // Restore the old frame list.  We only need to do this on the
  // non-exception code path, but we currently do it unconditionally
  // because that is simpler.  (The PNaCl C++ runtime library restores
  // the old frame list on the exceptional path; doing it again here
  // redundantly is OK.)
  CopyDebug(new StoreInst(OldList, EHStackTlsVar, Invoke), Invoke);

  Value *IsZero = CopyDebug(new ICmpInst(Invoke, CmpInst::ICMP_EQ, IsException,
                                         ConstantInt::get(I32, 0),
                                         "invoke_sj_is_zero"), Invoke);
  CopyDebug(BranchInst::Create(Invoke->getNormalDest(), Invoke->getUnwindDest(),
                               IsZero, Invoke),
            Invoke);

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
      Instruction *Inst = &*Iter++;
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
      Instruction *Inst = &*Iter++;
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
    FuncRewriter Rewriter(ExceptionFrameTy, &ExcInfoWriter, &*Func);
    Rewriter.expandFunc();
  }
  ExcInfoWriter.defineGlobalVariables(&M);
  return true;
}

ModulePass *llvm::createPNaClSjLjEHPass() {
  return new PNaClSjLjEH();
}
