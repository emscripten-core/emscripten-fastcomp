//===- ExpandByVal.cpp - Expand out use of "byval" and "sret" attributes---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass expands out by-value passing of structs as arguments and
// return values.  In LLVM IR terms, it expands out the "byval" and
// "sret" function argument attributes.
//
// The semantics of the "byval" attribute are that the callee function
// gets a private copy of the pointed-to argument that it is allowed
// to modify.  In implementing this, we have a choice between making
// the caller responsible for making the copy or making the callee
// responsible for making the copy.  We choose the former, because
// this matches how the normal native calling conventions work, and
// because it often allows the caller to write struct contents
// directly into the stack slot that it passes the callee, without an
// additional copy.
//
// Note that this pass does not attempt to modify functions that pass
// structs by value without using "byval" or "sret", such as:
//
//   define %struct.X @func()                           ; struct return
//   define void @func(%struct.X %arg)                  ; struct arg
//
// The pass only handles functions such as:
//
//   define void @func(%struct.X* sret %result_buffer)  ; struct return
//   define void @func(%struct.X* byval %ptr_to_arg)    ; struct arg
//
// This is because PNaCl Clang generates the latter and not the former.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/Attributes.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

namespace {
  // This is a ModulePass so that it can strip attributes from
  // declared functions as well as defined functions.
  class ExpandByVal : public ModulePass {
  public:
    static char ID; // Pass identification, replacement for typeid
    ExpandByVal() : ModulePass(ID) {
      initializeExpandByValPass(*PassRegistry::getPassRegistry());
    }

    virtual bool runOnModule(Module &M);
  };
}

char ExpandByVal::ID = 0;
INITIALIZE_PASS(ExpandByVal, "expand-byval",
                "Expand out by-value passing of structs",
                false, false)

// removeAttribute() currently does not work on Attribute::Alignment
// (it fails with an assertion error), so we have to take a more
// convoluted route to removing this attribute by recreating the
// AttributeSet.
AttributeSet RemoveAttrs(LLVMContext &Context, AttributeSet Attrs) {
  SmallVector<AttributeSet, 8> AttrList;
  for (unsigned Slot = 0; Slot < Attrs.getNumSlots(); ++Slot) {
    unsigned Index = Attrs.getSlotIndex(Slot);
    AttrBuilder AB;
    for (AttributeSet::iterator Attr = Attrs.begin(Slot), E = Attrs.end(Slot);
         Attr != E; ++Attr) {
      if (Attr->isEnumAttribute() &&
          Attr->getKindAsEnum() != Attribute::ByVal &&
          Attr->getKindAsEnum() != Attribute::StructRet) {
        AB.addAttribute(*Attr);
      }
      // IR semantics require that ByVal implies NoAlias.  However, IR
      // semantics do not require StructRet to imply NoAlias.  For
      // example, a global variable address can be passed as a
      // StructRet argument, although Clang does not do so and Clang
      // explicitly adds NoAlias to StructRet arguments.
      if (Attr->isEnumAttribute() &&
          Attr->getKindAsEnum() == Attribute::ByVal) {
        AB.addAttribute(Attribute::get(Context, Attribute::NoAlias));
      }
    }
    AttrList.push_back(AttributeSet::get(Context, Index, AB));
  }
  return AttributeSet::get(Context, AttrList);
}

// ExpandCall() can take a CallInst or an InvokeInst.  It returns
// whether the instruction was modified.
template <class InstType>
static bool ExpandCall(DataLayout *DL, InstType *Call) {
  bool Modify = false;
  AttributeSet Attrs = Call->getAttributes();
  for (unsigned ArgIdx = 0; ArgIdx < Call->getNumArgOperands(); ++ArgIdx) {
    unsigned AttrIdx = ArgIdx + 1;

    if (Attrs.hasAttribute(AttrIdx, Attribute::StructRet))
      Modify = true;

    if (Attrs.hasAttribute(AttrIdx, Attribute::ByVal)) {
      Modify = true;

      Value *ArgPtr = Call->getArgOperand(ArgIdx);
      Type *ArgType = ArgPtr->getType()->getPointerElementType();
      ConstantInt *ArgSize = ConstantInt::get(
          Call->getContext(), APInt(64, DL->getTypeStoreSize(ArgType)));
      // In principle, using the alignment from the argument attribute
      // should be enough.  However, Clang is not emitting this
      // attribute for PNaCl.  LLVM alloca instructions do not use the
      // ABI alignment of the type, so this must be specified
      // explicitly.
      // See https://code.google.com/p/nativeclient/issues/detail?id=3403
      //
      // Note that the parameter may have no alignment, but we have
      // more useful information from the type which we can use here
      // -- 0 in the parameter means no alignment is specified there,
      // so it has default alignment, but in memcpy 0 means
      // pessimistic alignment, the same as 1.
      unsigned Alignment =
          std::max(Attrs.getParamAlignment(AttrIdx),
                   DL->getABITypeAlignment(ArgType));

      // Make a copy of the byval argument.
      Instruction *CopyBuf = new AllocaInst(ArgType, 0, Alignment,
                                            ArgPtr->getName() + ".byval_copy");
      Function *Func = Call->getParent()->getParent();
      Func->getEntryBlock().getInstList().push_front(CopyBuf);
      IRBuilder<> Builder(Call);
      Builder.CreateLifetimeStart(CopyBuf, ArgSize);
      // Using the argument's alignment attribute for the memcpy
      // should be OK because the LLVM Language Reference says that
      // the alignment attribute specifies "the alignment of the stack
      // slot to form and the known alignment of the pointer specified
      // to the call site".
      Instruction *MemCpy = Builder.CreateMemCpy(CopyBuf, ArgPtr, ArgSize,
                                                 Alignment);
      MemCpy->setDebugLoc(Call->getDebugLoc());

      Call->setArgOperand(ArgIdx, CopyBuf);

      // Mark the argument copy as unused using llvm.lifetime.end.
      if (isa<CallInst>(Call)) {
        BasicBlock::iterator It = BasicBlock::iterator(Call);
        Builder.SetInsertPoint(&*(++It));
        Builder.CreateLifetimeEnd(CopyBuf, ArgSize);
      } else if (InvokeInst *Invoke = dyn_cast<InvokeInst>(Call)) {
        Builder.SetInsertPoint(&*Invoke->getNormalDest()->getFirstInsertionPt());
        Builder.CreateLifetimeEnd(CopyBuf, ArgSize);
        Builder.SetInsertPoint(&*Invoke->getUnwindDest()->getFirstInsertionPt());
        Builder.CreateLifetimeEnd(CopyBuf, ArgSize);
      }
    }
  }
  if (Modify) {
    Call->setAttributes(RemoveAttrs(Call->getContext(), Attrs));

    if (CallInst *CI = dyn_cast<CallInst>(Call)) {
      // This is no longer a tail call because the callee references
      // memory alloca'd by the caller.
      CI->setTailCall(false);
    }
  }
  return Modify;
}

bool ExpandByVal::runOnModule(Module &M) {
  bool Modified = false;
  DataLayout DL(&M);

  for (Module::iterator Func = M.begin(), E = M.end(); Func != E; ++Func) {
    AttributeSet NewAttrs = RemoveAttrs(Func->getContext(),
                                        Func->getAttributes());
    Modified |= (NewAttrs != Func->getAttributes());
    Func->setAttributes(NewAttrs);

    for (Function::iterator BB = Func->begin(), E = Func->end();
         BB != E; ++BB) {
      for (BasicBlock::iterator Inst = BB->begin(), E = BB->end();
           Inst != E; ++Inst) {
        if (CallInst *Call = dyn_cast<CallInst>(Inst)) {
          Modified |= ExpandCall(&DL, Call);
        } else if (InvokeInst *Call = dyn_cast<InvokeInst>(Inst)) {
          Modified |= ExpandCall(&DL, Call);
        }
      }
    }
  }

  return Modified;
}

ModulePass *llvm::createExpandByValPass() {
  return new ExpandByVal();
}
