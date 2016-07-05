//===- LowerEmAsyncify - transform asynchronous functions for Emscripten/JS   -----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Lu Wang <coolwanglu@gmail.com>
//
// In JS we don't have functions like sleep(), which is on the other hand very popuar in C/C++ etc.
// This pass tries to convert funcitons calling sleep() into a valid form in JavaScript
// The basic idea is to split the callee at the place where sleep() is called,
// then the first half may schedule the second half using setTimeout.
// But we need to pay lots of attention to analyzing/saving/restoring context variables and return values
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/CallSite.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Transforms/NaCl.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/Local.h" // for DemoteRegToStack, removeUnreachableBlocks
#include "llvm/Transforms/Utils/PromoteMemToReg.h" // for PromoteMemToReg
#include "llvm/Transforms/Utils/ValueMapper.h"
#include "llvm/Pass.h"

#include <vector>

using namespace llvm;

static cl::list<std::string>
AsyncifyFunctions("emscripten-asyncify-functions",
                  cl::desc("Functions that call one of these functions, directly or indirectly, will be asyncified"),
                  cl::CommaSeparated);

static cl::list<std::string>
AsyncifyWhiteList("emscripten-asyncify-whitelist",
                  cl::desc("Functions that should not be asyncified"),
                  cl::CommaSeparated);

namespace {
  class LowerEmAsyncify: public ModulePass {
    Module *TheModule;

  public:
    static char ID; // Pass identification, replacement for typeid
    explicit LowerEmAsyncify() : ModulePass(ID), TheModule(NULL) {
      initializeLowerEmAsyncifyPass(*PassRegistry::getPassRegistry());
    }
    virtual ~LowerEmAsyncify() { }
    bool runOnModule(Module &M);

  private:
    const DataLayout *DL;

    Type *Void, *I1, *I32, *I32Ptr;
    FunctionType *VFunction, *I1Function, *I32PFunction;
    FunctionType *VI32PFunction, *I32PI32Function;
    FunctionType *CallbackFunctionType;

    Function *AllocAsyncCtxFunction, *ReallocAsyncCtxFunction, *FreeAsyncCtxFunction;
    Function *CheckAsyncFunction;
    Function *DoNotUnwindFunction, *DoNotUnwindAsyncFunction;
    Function *GetAsyncReturnValueAddrFunction;

    void initTypesAndFunctions(void);

    typedef std::vector<Instruction *> Instructions;
    typedef DenseMap<Function*, Instructions> FunctionInstructionsMap;
    typedef std::vector<Value*> Values;
    typedef SmallPtrSet<BasicBlock*, 16> BasicBlockSet;

    // all the information we want for an async call
    struct AsyncCallEntry {
      Instruction *AsyncCallInst; // calling an async function
      BasicBlock *AfterCallBlock; // the block we should continue on after getting the return value of AsynCallInst
      CallInst *AllocAsyncCtxInst;  // where we allocate the async ctx before the async call, in the original function
      Values ContextVariables; // those need to be saved and restored for the async call
      StructType *ContextStructType; // The structure constructing all the context variables
      BasicBlock *SaveAsyncCtxBlock; // the block in which we save all the variables
      Function *CallbackFunc; // the callback function for this async call, which is converted from the original function
    };

    BasicBlockSet FindReachableBlocksFrom(BasicBlock *src);

    // Find everything that we should save and restore for the async call
    // save them to Entry.ContextVariables
    void FindContextVariables(AsyncCallEntry & Entry);

    // The essential function
    // F is now in the sync form, transform it into an async form that is valid in JS
    void transformAsyncFunction(Function &F, Instructions const& AsyncCalls);

    bool IsFunctionPointerCall(const Instruction *I);
  };
}

char LowerEmAsyncify::ID = 0;
INITIALIZE_PASS(LowerEmAsyncify, "loweremasyncify",
    "Lower async functions for js/emscripten",
    false, false)

bool LowerEmAsyncify::runOnModule(Module &M) {
  TheModule = &M;
  DL = &M.getDataLayout();

  std::set<std::string> WhiteList(AsyncifyWhiteList.begin(), AsyncifyWhiteList.end());

  /* 
   * collect all the functions that should be asyncified
   * any function that _might_ call an async function is also async
   */
  std::vector<Function*> AsyncFunctionsPending;
  for(unsigned i = 0; i < AsyncifyFunctions.size(); ++i) {
    std::string const& AFName = AsyncifyFunctions[i];
    Function *F = TheModule->getFunction(AFName);
    if (F && !WhiteList.count(F->getName())) {
      AsyncFunctionsPending.push_back(F);
    }  
  }

  // No function needed to transform
  if (AsyncFunctionsPending.empty()) return false;

  // Walk through the call graph and find all the async functions
  FunctionInstructionsMap AsyncFunctionCalls;
  {
    // pessimistic: consider all indirect calls as possibly async
    // TODO: deduce based on function types
    for (Module::iterator FI = TheModule->begin(), FE = TheModule->end(); FI != FE; ++FI) {
      if (WhiteList.count(FI->getName())) continue;

      bool has_indirect_call = false;
      for (inst_iterator I = inst_begin(&*FI), E = inst_end(&*FI); I != E; ++I) {
        if (IsFunctionPointerCall(&*I)) {
          has_indirect_call = true;
          AsyncFunctionCalls[&*FI].push_back(&*I);
        }
      }

      if (has_indirect_call) AsyncFunctionsPending.push_back(&*FI);
    }

    while (!AsyncFunctionsPending.empty()) {
      Function *CurFunction = AsyncFunctionsPending.back();
      AsyncFunctionsPending.pop_back();

      for (Value::user_iterator UI = CurFunction->user_begin(), E = CurFunction->user_end(); UI != E; ++UI) {
        ImmutableCallSite ICS(*UI);
        if (!ICS) continue;
        // we only need those instructions calling the function
        // if the function address is used for other purpose, we don't care
        if (CurFunction != ICS.getCalledValue()->stripPointerCasts()) continue;
        // Now I is either CallInst or InvokeInst
        Instruction *I = cast<Instruction>(*UI);
        Function *F = I->getParent()->getParent();
        if (AsyncFunctionCalls.count(F) == 0) {
          AsyncFunctionsPending.push_back(F);
        }
        AsyncFunctionCalls[F].push_back(I);
      }
    }
  }

  // exit if no async function is found at all
  if (AsyncFunctionCalls.empty()) return false;

  initTypesAndFunctions();

  for (FunctionInstructionsMap::iterator I = AsyncFunctionCalls.begin(), E = AsyncFunctionCalls.end();
      I != E; ++I) {
    transformAsyncFunction(*(I->first), I->second);
  }

  return true;
}

void LowerEmAsyncify::initTypesAndFunctions(void) {
  // Data types
  Void = Type::getVoidTy(TheModule->getContext());
  I1 = Type::getInt1Ty(TheModule->getContext());
  I32 = Type::getInt32Ty(TheModule->getContext());
  I32Ptr = Type::getInt32PtrTy(TheModule->getContext());

  // Function types
  SmallVector<Type*, 2> ArgTypes;
  VFunction = FunctionType::get(Void, false);
  I1Function = FunctionType::get(I1, false);
  I32PFunction = FunctionType::get(I32Ptr, false);

  ArgTypes.clear();
  ArgTypes.push_back(I32Ptr);
  VI32PFunction = FunctionType::get(Void, ArgTypes, false);

  ArgTypes.clear();
  ArgTypes.push_back(I32);
  I32PI32Function = FunctionType::get(I32Ptr, ArgTypes, false);

  CallbackFunctionType = VI32PFunction;

  // Functions
  CheckAsyncFunction = Function::Create(
    I1Function,
    GlobalValue::ExternalLinkage,
    "emscripten_check_async",
    TheModule
  );

  AllocAsyncCtxFunction = Function::Create(
    I32PI32Function,
    GlobalValue::ExternalLinkage,
    "emscripten_alloc_async_context",
    TheModule
  );

  ReallocAsyncCtxFunction = Function::Create(
    I32PI32Function,
    GlobalValue::ExternalLinkage,
    "emscripten_realloc_async_context",
    TheModule
  );

  FreeAsyncCtxFunction = Function::Create(
    VI32PFunction,
    GlobalValue::ExternalLinkage,
    "emscripten_free_async_context",
    TheModule
  );

  DoNotUnwindFunction = Function::Create(
    VFunction,
    GlobalValue::ExternalLinkage,
    "emscripten_do_not_unwind",
    TheModule
  );

  DoNotUnwindAsyncFunction = Function::Create(
    VFunction,
    GlobalValue::ExternalLinkage,
    "emscripten_do_not_unwind_async",
    TheModule
  );

  GetAsyncReturnValueAddrFunction = Function::Create(
    I32PFunction,
    GlobalValue::ExternalLinkage,
    "emscripten_get_async_return_value_addr",
    TheModule
  );
}

LowerEmAsyncify::BasicBlockSet LowerEmAsyncify::FindReachableBlocksFrom(BasicBlock *src) {
  BasicBlockSet ReachableBlockSet;
  std::vector<BasicBlock*> pending;
  ReachableBlockSet.insert(src);
  pending.push_back(src);
  while (!pending.empty()) {
    BasicBlock *CurBlock = pending.back();
    pending.pop_back();
    for (succ_iterator SI = succ_begin(CurBlock), SE = succ_end(CurBlock); SI != SE; ++SI) {
      if (ReachableBlockSet.count(*SI) == 0) {
        ReachableBlockSet.insert(*SI);
        pending.push_back(*SI);
      }
    }
  }
  return ReachableBlockSet;
}

void LowerEmAsyncify::FindContextVariables(AsyncCallEntry & Entry) {
  BasicBlock *AfterCallBlock = Entry.AfterCallBlock;

  Function & F = *AfterCallBlock->getParent();

  // Create a new entry block as if in the callback function
  // theck check variables that no longer properly dominate their uses
  BasicBlock *EntryBlock = BasicBlock::Create(TheModule->getContext(), "", &F, &F.getEntryBlock());
  BranchInst::Create(AfterCallBlock, EntryBlock);

  DominatorTreeWrapperPass DTW;
  DTW.runOnFunction(F);
  DominatorTree& DT = DTW.getDomTree();

  // These blocks may be using some values defined at or before AsyncCallBlock
  BasicBlockSet Ramifications = FindReachableBlocksFrom(AfterCallBlock); 

  SmallPtrSet<Value*, 32> ContextVariables;
  Values Pending;

  // Examine the instructions, find all variables that we need to store in the context
  for (BasicBlockSet::iterator RI = Ramifications.begin(), RE = Ramifications.end(); RI != RE; ++RI) {
    for (BasicBlock::iterator I = (*RI)->begin(), E = (*RI)->end(); I != E; ++I) {
      for (unsigned i = 0, NumOperands = I->getNumOperands(); i < NumOperands; ++i) {
        Value *O = I->getOperand(i);
        if (Instruction *Inst = dyn_cast<Instruction>(O)) {
          if (Inst == Entry.AsyncCallInst) continue; // for the original async call, we will load directly from async return value
          if (ContextVariables.count(Inst) != 0)  continue; // already examined 

          if (!DT.dominates(Inst, I->getOperandUse(i))) {
            // `I` is using `Inst`, yet `Inst` does not dominate `I` if we arrive directly at AfterCallBlock
            // so we need to save `Inst` in the context
            ContextVariables.insert(Inst);
            Pending.push_back(Inst);
          }
        } else if (Argument *Arg = dyn_cast<Argument>(O)) {
          // count() should be as fast/slow as insert, so just insert here 
          ContextVariables.insert(Arg);
        }
      }
    }
  }

  // restore F
  EntryBlock->eraseFromParent();  

  Entry.ContextVariables.clear();
  Entry.ContextVariables.reserve(ContextVariables.size());
  for (SmallPtrSet<Value*, 32>::iterator I = ContextVariables.begin(), E = ContextVariables.end(); I != E; ++I) {
    Entry.ContextVariables.push_back(*I);
  }
}

/*
 * Consider that F contains a call to G, both of which are async:
 *
 * function F:
 * ...
 * %0 = G(%1, %2, ...);
 * ...
 * return %%;
 *
 * We want to convert F and generate F__asyn_cb
 * they are similar, but with minor yet important differences
 * Note those `main func only` and `callback func only` instructions 

//////////////////////////////////////////////////////////
  function F:
  ...
  ctx = alloc_ctx(len, sp); // main func only
                         // TODO
                         // we could also do this only after an async call
                         // but in that case we will need to pass ctx to the function
                         // since ctx is no longer in the top async stack frame
  %0 = G(%1, %2, ...);
  if (async) { // G was async
    save context variables in ctx
    register F.async_cb as the callback in frame
    return without unwinding the stack frame
  } else { // G was sync
    // use %0 as normal
    free_ctx(ctx); // main func only
    // ctx is freed here, because so far F is still a sync function
    // and we don't want any side effects
    ...
    async return value = %%;
    return & normally unwind the stack frame // main func only
  }
//////////////////////////////////////////////////////////

 * And here's F.async_cb

//////////////////////////////////////////////////////////
  function F.async_cb(ctx):
  load variables from ctx // callback func only
  goto resume_point;      // callback func only
  ...
  ctx = realloc_ctx(len); // callback func only
                          // realloc_ctx is different from alloc_ctx
                          // which reused the current async stack frame
                          // we want to keep the saved stack pointer
  %0 = G(%1, %2, ...);
  if (async) {
    save context variables in ctx
    register F.async_cb as the callback
    return without unwinding the stack frame
  } else {
    resume_point:
    %0'= either $0 or the async return value // callback func only
    ...
    async return value = %%
    return restore the stack pointer back to the value stored in F // callback func only
    // no need to free the ctx
    // the scheduler will be aware of this return and handle the stack frames
  }
//////////////////////////////////////////////////////////

 */

void LowerEmAsyncify::transformAsyncFunction(Function &F, Instructions const& AsyncCalls) {
  assert(!AsyncCalls.empty());

  // Pass 0
  // collect all the return instructions from the original function
  // will use later
  std::vector<ReturnInst*> OrigReturns;
  for (inst_iterator I = inst_begin(&F), E = inst_end(&F); I != E; ++I) {
    if (ReturnInst *RI = dyn_cast<ReturnInst>(&*I)) {
      OrigReturns.push_back(RI);
    }
  }

  // Pass 1
  // Scan each async call and make the basic structure:
  // All these will be cloned into the callback functions
  // - allocate the async context before calling an async function
  // - check async right after calling an async function, save context & return if async, continue if not
  // - retrieve the async return value and free the async context if the called function turns out to be sync
  std::vector<AsyncCallEntry> AsyncCallEntries;
  AsyncCallEntries.reserve(AsyncCalls.size());
  for (Instructions::const_iterator I = AsyncCalls.begin(), E = AsyncCalls.end(); I != E; ++I) {
    // prepare blocks
    Instruction *CurAsyncCall = *I;

    // The block containing the async call
    BasicBlock *CurBlock = CurAsyncCall->getParent();
    // The block should run after the async call
    BasicBlock *AfterCallBlock = SplitBlock(CurBlock, CurAsyncCall->getNextNode());
    // The block where we store the context and return
    BasicBlock *SaveAsyncCtxBlock = BasicBlock::Create(TheModule->getContext(), "SaveAsyncCtx", &F, AfterCallBlock);
    // return a dummy value at the end, to make the block valid
    new UnreachableInst(TheModule->getContext(), SaveAsyncCtxBlock);

    // allocate the context before making the call
    // we don't know the size yet, will fix it later
    // we cannot insert the instruction later because,
    // we need to make sure that all the instructions and blocks are fixed before we can generate DT and find context variables
    // In CallHandler.h `sp` will be put as the second parameter
    // such that we can take a note of the original sp 
    CallInst *AllocAsyncCtxInst = CallInst::Create(AllocAsyncCtxFunction, Constant::getNullValue(I32), "AsyncCtx", CurAsyncCall);

    // Right after the call
    // check async and return if so
    // TODO: we can define truly async functions and partial async functions
    {
      // remove old terminator, which came from SplitBlock
      CurBlock->getTerminator()->eraseFromParent();
      // go to SaveAsyncCtxBlock if the previous call is async
      // otherwise just continue to AfterCallBlock
      CallInst *CheckAsync = CallInst::Create(CheckAsyncFunction, "IsAsync", CurBlock);
      BranchInst::Create(SaveAsyncCtxBlock, AfterCallBlock, CheckAsync, CurBlock);
    }

    // take a note of this async call
    AsyncCallEntry CurAsyncCallEntry;
    CurAsyncCallEntry.AsyncCallInst = CurAsyncCall;
    CurAsyncCallEntry.AfterCallBlock = AfterCallBlock;
    CurAsyncCallEntry.AllocAsyncCtxInst = AllocAsyncCtxInst;
    CurAsyncCallEntry.SaveAsyncCtxBlock = SaveAsyncCtxBlock;
    // create an empty function for the callback, which will be constructed later
    CurAsyncCallEntry.CallbackFunc = Function::Create(CallbackFunctionType, F.getLinkage(), F.getName() + "__async_cb", TheModule);
    AsyncCallEntries.push_back(CurAsyncCallEntry);
  }


  // Pass 2
  // analyze the context variables and construct SaveAsyncCtxBlock for each async call
  // also calculate the size of the context and allocate the async context accordingly
  for (std::vector<AsyncCallEntry>::iterator EI = AsyncCallEntries.begin(), EE = AsyncCallEntries.end();  EI != EE; ++EI) {
    AsyncCallEntry & CurEntry = *EI;

    // Collect everything to be saved
    FindContextVariables(CurEntry);

    // Pack the variables as a struct
    {
      // TODO: sort them from large memeber to small ones, in order to make the struct compact even when aligned
      SmallVector<Type*, 8> Types;
      Types.push_back(CallbackFunctionType->getPointerTo());
      for (Values::iterator VI = CurEntry.ContextVariables.begin(), VE = CurEntry.ContextVariables.end(); VI != VE; ++VI) {
        Types.push_back((*VI)->getType());
      }
      CurEntry.ContextStructType = StructType::get(TheModule->getContext(), Types);
    }

    // fix the size of allocation
    CurEntry.AllocAsyncCtxInst->setOperand(0, 
        ConstantInt::get(I32, DL->getTypeStoreSize(CurEntry.ContextStructType)));

    // construct SaveAsyncCtxBlock
    {
      // fill in SaveAsyncCtxBlock
      // temporarily remove the terminator for convenience
      CurEntry.SaveAsyncCtxBlock->getTerminator()->eraseFromParent();
      assert(CurEntry.SaveAsyncCtxBlock->empty());

      Type *AsyncCtxAddrTy = CurEntry.ContextStructType->getPointerTo();
      BitCastInst *AsyncCtxAddr = new BitCastInst(CurEntry.AllocAsyncCtxInst, AsyncCtxAddrTy, "AsyncCtxAddr", CurEntry.SaveAsyncCtxBlock);
      SmallVector<Value*, 2> Indices;
      // store the callback
      {
        Indices.push_back(ConstantInt::get(I32, 0));
        Indices.push_back(ConstantInt::get(I32, 0));
        GetElementPtrInst *AsyncVarAddr = GetElementPtrInst::Create(CurEntry.ContextStructType, AsyncCtxAddr, Indices, "", CurEntry.SaveAsyncCtxBlock);
        new StoreInst(CurEntry.CallbackFunc, AsyncVarAddr, CurEntry.SaveAsyncCtxBlock);
      }
      // store the context variables
      for (size_t i = 0; i < CurEntry.ContextVariables.size(); ++i) {
        Indices.clear();
        Indices.push_back(ConstantInt::get(I32, 0));
        Indices.push_back(ConstantInt::get(I32, i + 1)); // the 0th element is the callback function
        GetElementPtrInst *AsyncVarAddr = GetElementPtrInst::Create(CurEntry.ContextStructType, AsyncCtxAddr, Indices, "", CurEntry.SaveAsyncCtxBlock);
        new StoreInst(CurEntry.ContextVariables[i], AsyncVarAddr, CurEntry.SaveAsyncCtxBlock);
      }
      // to exit the block, we want to return without unwinding the stack frame
      CallInst::Create(DoNotUnwindFunction, "", CurEntry.SaveAsyncCtxBlock);
      ReturnInst::Create(TheModule->getContext(), 
          (F.getReturnType()->isVoidTy() ? 0 : Constant::getNullValue(F.getReturnType())),
          CurEntry.SaveAsyncCtxBlock);
    }
  }

  // Pass 3
  // now all the SaveAsyncCtxBlock's have been constructed
  // we can clone F and construct callback functions 
  // we could not construct the callbacks in Pass 2 because we need _all_ those SaveAsyncCtxBlock's appear in _each_ callback
  for (std::vector<AsyncCallEntry>::iterator EI = AsyncCallEntries.begin(), EE = AsyncCallEntries.end();  EI != EE; ++EI) {
    AsyncCallEntry & CurEntry = *EI;

    Function *CurCallbackFunc = CurEntry.CallbackFunc;
    ValueToValueMapTy VMap;

    // Add the entry block
    // load variables from the context
    // also update VMap for CloneFunction
    BasicBlock *EntryBlock = BasicBlock::Create(TheModule->getContext(), "AsyncCallbackEntry", CurCallbackFunc);
    std::vector<LoadInst *> LoadedAsyncVars;
    {
      Type *AsyncCtxAddrTy = CurEntry.ContextStructType->getPointerTo();
      BitCastInst *AsyncCtxAddr = new BitCastInst(&*CurCallbackFunc->arg_begin(), AsyncCtxAddrTy, "AsyncCtx", EntryBlock);
      SmallVector<Value*, 2> Indices;
      for (size_t i = 0; i < CurEntry.ContextVariables.size(); ++i) {
        Indices.clear();
        Indices.push_back(ConstantInt::get(I32, 0));
        Indices.push_back(ConstantInt::get(I32, i + 1)); // the 0th element of AsyncCtx is the callback function
        GetElementPtrInst *AsyncVarAddr = GetElementPtrInst::Create(CurEntry.ContextStructType, AsyncCtxAddr, Indices, "", EntryBlock);
        LoadedAsyncVars.push_back(new LoadInst(AsyncVarAddr, "", EntryBlock));
        // we want the argument to be replaced by the loaded value
        if (isa<Argument>(CurEntry.ContextVariables[i]))
          VMap[CurEntry.ContextVariables[i]] = LoadedAsyncVars.back();
      }
    }

    // we don't need any argument, just leave dummy entries there to cheat CloneFunctionInto
    for (Function::const_arg_iterator AI = F.arg_begin(), AE = F.arg_end(); AI != AE; ++AI) {
      if (VMap.count(&*AI) == 0)
        VMap[&*AI] = Constant::getNullValue(AI->getType());
    }

    // Clone the function
    {
      SmallVector<ReturnInst*, 8> Returns;
      CloneFunctionInto(CurCallbackFunc, &F, VMap, false, Returns);
      
      // return type of the callback functions is always void
      // need to fix the return type
      if (!F.getReturnType()->isVoidTy()) {
        // for those return instructions that are from the original function
        // it means we are 'truly' leaving this function
        // need to store the return value right before ruturn
        for (size_t i = 0; i < OrigReturns.size(); ++i) {
          ReturnInst *RI = cast<ReturnInst>(VMap[OrigReturns[i]]);
          // Need to store the return value into the global area
          CallInst *RawRetValAddr = CallInst::Create(GetAsyncReturnValueAddrFunction, "", RI);
          BitCastInst *RetValAddr = new BitCastInst(RawRetValAddr, F.getReturnType()->getPointerTo(), "AsyncRetValAddr", RI);
          new StoreInst(RI->getOperand(0), RetValAddr, RI);
        }
        // we want to unwind the stack back to where it was before the original function as called
        // but we don't actually need to do this here
        // at this point it must be true that no callback is pended
        // so the scheduler will correct the stack pointer and pop the frame
        // here we just fix the return type
        for (size_t i = 0; i < Returns.size(); ++i) {
          ReplaceInstWithInst(Returns[i], ReturnInst::Create(TheModule->getContext()));
        }
      }
    }

    // the callback function does not have any return value
    // so clear all the attributes for return
    {
      AttributeSet Attrs = CurCallbackFunc->getAttributes();
      CurCallbackFunc->setAttributes(
        Attrs.removeAttributes(TheModule->getContext(), AttributeSet::ReturnIndex, Attrs.getRetAttributes())
      );
    }

    // in the callback function, we never allocate a new async frame
    // instead we reuse the existing one
    for (std::vector<AsyncCallEntry>::iterator EI = AsyncCallEntries.begin(), EE = AsyncCallEntries.end();  EI != EE; ++EI) {
      Instruction *I = cast<Instruction>(VMap[EI->AllocAsyncCtxInst]);
      ReplaceInstWithInst(I, CallInst::Create(ReallocAsyncCtxFunction, I->getOperand(0), "ReallocAsyncCtx"));
    }

    // mapped entry point & async call
    BasicBlock *ResumeBlock = cast<BasicBlock>(VMap[CurEntry.AfterCallBlock]);
    Instruction *MappedAsyncCall = cast<Instruction>(VMap[CurEntry.AsyncCallInst]);
   
    // To save space, for each async call in the callback function, we just ignore the sync case, and leave it to the scheduler
    // TODO need an option for this
    {
      for (std::vector<AsyncCallEntry>::iterator EI = AsyncCallEntries.begin(), EE = AsyncCallEntries.end();  EI != EE; ++EI) {
        AsyncCallEntry & CurEntry = *EI;
        Instruction *MappedAsyncCallInst = cast<Instruction>(VMap[CurEntry.AsyncCallInst]);
        BasicBlock *MappedAsyncCallBlock = MappedAsyncCallInst->getParent();
        BasicBlock *MappedAfterCallBlock = cast<BasicBlock>(VMap[CurEntry.AfterCallBlock]);

        // for the sync case of the call, go to NewBlock (instead of MappedAfterCallBlock)
        BasicBlock *NewBlock = BasicBlock::Create(TheModule->getContext(), "", CurCallbackFunc, MappedAfterCallBlock);
        MappedAsyncCallBlock->getTerminator()->setSuccessor(1, NewBlock);
        // store the return value
        if (!MappedAsyncCallInst->use_empty()) {
          CallInst *RawRetValAddr = CallInst::Create(GetAsyncReturnValueAddrFunction, "", NewBlock);
          BitCastInst *RetValAddr = new BitCastInst(RawRetValAddr, MappedAsyncCallInst->getType()->getPointerTo(), "AsyncRetValAddr", NewBlock);
          new StoreInst(MappedAsyncCallInst, RetValAddr, NewBlock);
        }
        // tell the scheduler that we want to keep the current async stack frame
        CallInst::Create(DoNotUnwindAsyncFunction, "", NewBlock);
        // finally we go to the SaveAsyncCtxBlock, to register the callbac, save the local variables and leave
        BasicBlock *MappedSaveAsyncCtxBlock = cast<BasicBlock>(VMap[CurEntry.SaveAsyncCtxBlock]);
        BranchInst::Create(MappedSaveAsyncCtxBlock, NewBlock);
      }
    }

    std::vector<AllocaInst*> ToPromote;
    // applying loaded variables in the entry block
    {
      BasicBlockSet ReachableBlocks = FindReachableBlocksFrom(ResumeBlock);
      for (size_t i = 0; i < CurEntry.ContextVariables.size(); ++i) {
        Value *OrigVar = CurEntry.ContextVariables[i];
        if (isa<Argument>(OrigVar)) continue; // already processed
        Value *CurVar = VMap[OrigVar];
        assert(CurVar != MappedAsyncCall);
        if (Instruction *Inst = dyn_cast<Instruction>(CurVar)) {
          if (ReachableBlocks.count(Inst->getParent())) {
            // Inst could be either defined or loaded from the async context
            // Do the dirty works in memory
            // TODO: might need to check the safety first
            // TODO: can we create phi directly?
            AllocaInst *Addr = DemoteRegToStack(*Inst, false);
            new StoreInst(LoadedAsyncVars[i], Addr, EntryBlock);
            ToPromote.push_back(Addr);
          } else {
            // The parent block is not reachable, which means there is no confliction
            // it's safe to replace Inst with the loaded value
            assert(Inst != LoadedAsyncVars[i]); // this should only happen when OrigVar is an Argument
            Inst->replaceAllUsesWith(LoadedAsyncVars[i]); 
          }
        }
      }
    }

    // resolve the return value of the previous async function
    // it could be the value just loaded from the global area
    // or directly returned by the function (in its sync case)
    if (!CurEntry.AsyncCallInst->use_empty()) {
      // load the async return value
      CallInst *RawRetValAddr = CallInst::Create(GetAsyncReturnValueAddrFunction, "", EntryBlock);
      BitCastInst *RetValAddr = new BitCastInst(RawRetValAddr, MappedAsyncCall->getType()->getPointerTo(), "AsyncRetValAddr", EntryBlock);
      LoadInst *RetVal = new LoadInst(RetValAddr, "AsyncRetVal", EntryBlock);

      AllocaInst *Addr = DemoteRegToStack(*MappedAsyncCall, false);
      new StoreInst(RetVal, Addr, EntryBlock);
      ToPromote.push_back(Addr);
    }

    // TODO remove unreachable blocks before creating phi
   
    // We go right to ResumeBlock from the EntryBlock
    BranchInst::Create(ResumeBlock, EntryBlock);
   
    /*
     * Creating phi's
     * Normal stack frames and async stack frames are interleaving with each other.
     * In a callback function, if we call an async function, we might need to realloc the async ctx.
     * at this point we don't want anything stored after the ctx, 
     * such that we can free and extend the ctx by simply update STACKTOP.
     * Therefore we don't want any alloca's in callback functions.
     *
     */
    if (!ToPromote.empty()) {
      DominatorTreeWrapperPass DTW;
      DTW.runOnFunction(*CurCallbackFunc);
      PromoteMemToReg(ToPromote, DTW.getDomTree());
    }

    removeUnreachableBlocks(*CurCallbackFunc);
  }

  // Pass 4
  // Here are modifications to the original function, which we won't want to be cloned into the callback functions
  for (std::vector<AsyncCallEntry>::iterator EI = AsyncCallEntries.begin(), EE = AsyncCallEntries.end();  EI != EE; ++EI) {
    AsyncCallEntry & CurEntry = *EI;
    // remove the frame if no async functinon has been called
    CallInst::Create(FreeAsyncCtxFunction, CurEntry.AllocAsyncCtxInst, "", CurEntry.AfterCallBlock->getFirstNonPHI());
  }
}

bool LowerEmAsyncify::IsFunctionPointerCall(const Instruction *I) {
  // mostly from CallHandler.h
  ImmutableCallSite CS(I);
  if (!CS) return false; // not call nor invoke
  const Value *CV = CS.getCalledValue()->stripPointerCasts();
  return !isa<const Function>(CV);
}

ModulePass *llvm::createLowerEmAsyncifyPass() {
  return new LowerEmAsyncify();
}
