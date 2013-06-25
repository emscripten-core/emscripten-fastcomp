//===- StripAttributes.cpp - Remove attributes not supported by PNaCl------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass strips out attributes that are not supported by PNaCl's
// stable ABI.  Currently, this strips out:
//
//  * Function and argument attributes from functions and function
//    calls.
//  * Calling conventions from functions and function calls.
//  * The "align" attribute on functions.
//  * The alignment argument of memcpy/memmove/memset intrinsic calls.
//  * The "unnamed_addr" attribute on functions and global variables.
//  * The distinction between "internal" and "private" linkage.
//  * "protected" and "internal" visibility of functions and globals.
//  * The arithmetic attributes "nsw", "nuw" and "exact".
//  * It reduces the set of possible "align" attributes on memory
//    accesses.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/Pass.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

namespace {
  // This is a ModulePass so that it can modify attributes of global
  // variables.
  class StripAttributes : public ModulePass {
  public:
    static char ID; // Pass identification, replacement for typeid
    StripAttributes() : ModulePass(ID) {
      initializeStripAttributesPass(*PassRegistry::getPassRegistry());
    }

    virtual bool runOnModule(Module &M);
  };
}

char StripAttributes::ID = 0;
INITIALIZE_PASS(StripAttributes, "nacl-strip-attributes",
                "Strip out attributes that are not part of PNaCl's ABI",
                false, false)

// Most attributes are just hints which can safely be removed.  A few
// attributes can break programs if removed, so check all attributes
// before removing them, in case LLVM adds new attributes.
static void CheckAttributes(AttributeSet Attrs) {
  for (unsigned Slot = 0; Slot < Attrs.getNumSlots(); ++Slot) {
    for (AttributeSet::iterator Attr = Attrs.begin(Slot), E = Attrs.end(Slot);
         Attr != E; ++Attr) {
      switch (Attr->getKindAsEnum()) {
        // The following attributes can affect calling conventions.
        // Rather than complaining, we just strip these out.
        // ExpandSmallArguments should have rendered SExt/ZExt
        // meaningless since the function arguments will be at least
        // 32-bit.
        case Attribute::InReg:
        case Attribute::SExt:
        case Attribute::ZExt:
        // These attributes influence ABI decisions that should not be
        // visible to PNaCl pexes.
        case Attribute::NonLazyBind:  // Only relevant to dynamic linking.
        case Attribute::NoRedZone:
        case Attribute::StackAlignment:

        // The following attributes are just hints, which can be
        // safely removed.
        case Attribute::AlwaysInline:
        case Attribute::InlineHint:
        case Attribute::MinSize:
        case Attribute::NoAlias:
        case Attribute::NoBuiltin:
        case Attribute::NoCapture:
        case Attribute::NoDuplicate:
        case Attribute::NoImplicitFloat:
        case Attribute::NoInline:
        case Attribute::NoReturn:
        case Attribute::OptimizeForSize:
        case Attribute::ReadNone:
        case Attribute::ReadOnly:

        // PNaCl does not support -fstack-protector in the translator.
        case Attribute::StackProtect:
        case Attribute::StackProtectReq:
        case Attribute::StackProtectStrong:
        // PNaCl does not support ASan in the translator.
        case Attribute::SanitizeAddress:
        case Attribute::SanitizeThread:
        case Attribute::SanitizeMemory:

        // The Language References cites setjmp() as an example of a
        // function which returns twice, and says ReturnsTwice is
        // necessary to disable optimizations such as tail calls.
        // However, in the PNaCl ABI, setjmp() is an intrinsic, and
        // user-defined functions are not allowed to return twice.
        case Attribute::ReturnsTwice:

        // NoUnwind is not a hint if it causes unwind info to be
        // omitted, since this will prevent C++ exceptions from
        // propagating.  In the future, when PNaCl supports zero-cost
        // C++ exception handling using unwind info, we might allow
        // NoUnwind and UWTable.  Alternatively, we might continue to
        // disallow them, and just generate unwind info for all
        // functions.
        case Attribute::NoUnwind:
        case Attribute::UWTable:
          break;

        case Attribute::ByVal:
        case Attribute::StructRet:
        case Attribute::Alignment:
          Attrs.dump();
          report_fatal_error(
              "Attribute should already have been removed by ExpandByVal");

        case Attribute::Naked:
        case Attribute::Nest:
          Attrs.dump();
          report_fatal_error("Unsupported attribute");

        default:
          Attrs.dump();
          report_fatal_error("Unrecognized attribute");
      }
    }
  }
}

void stripGlobalValueAttrs(GlobalValue *GV) {
  // In case source code uses __attribute__((visibility("hidden"))) or
  // __attribute__((visibility("protected"))), strip these attributes.
  GV->setVisibility(GlobalValue::DefaultVisibility);

  GV->setUnnamedAddr(false);

  // Convert "private" linkage to "internal" to reduce the number of
  // linkage types that need to be represented in PNaCl's wire format.
  //
  // We convert "private" to "internal" rather than vice versa because
  // "private" symbols are omitted from the nexe's symbol table, which
  // would get in the way of debugging when an unstripped pexe is
  // translated offline.
  if (GV->getLinkage() == GlobalValue::PrivateLinkage)
    GV->setLinkage(GlobalValue::InternalLinkage);
}

static unsigned normalizeAlignment(DataLayout *DL, unsigned Alignment,
                                   Type *Ty, bool IsAtomic) {
  unsigned MaxAllowed = 1;
  if (Ty->isDoubleTy() || Ty->isFloatTy() || IsAtomic)
    MaxAllowed = DL->getTypeAllocSize(Ty);
  // If the alignment is set to 0, this means "use the default
  // alignment for the target", which we fill in explicitly.
  if (Alignment == 0 || Alignment >= MaxAllowed)
    return MaxAllowed;
  return 1;
}

void stripFunctionAttrs(DataLayout *DL, Function *Func) {
  CheckAttributes(Func->getAttributes());
  Func->setAttributes(AttributeSet());
  Func->setCallingConv(CallingConv::C);
  Func->setAlignment(0);

  for (Function::iterator BB = Func->begin(), E = Func->end();
       BB != E; ++BB) {
    for (BasicBlock::iterator Inst = BB->begin(), E = BB->end();
         Inst != E; ++Inst) {
      CallSite Call(Inst);
      if (Call) {
        CheckAttributes(Call.getAttributes());
        Call.setAttributes(AttributeSet());
        Call.setCallingConv(CallingConv::C);

        // Set memcpy(), memmove() and memset() to use pessimistic
        // alignment assumptions.
        if (MemIntrinsic *MemOp = dyn_cast<MemIntrinsic>(Inst)) {
          Type *AlignTy = MemOp->getAlignmentCst()->getType();
          MemOp->setAlignment(ConstantInt::get(AlignTy, 1));
        }
      } else if (OverflowingBinaryOperator *Op =
                     dyn_cast<OverflowingBinaryOperator>(Inst)) {
        cast<BinaryOperator>(Op)->setHasNoUnsignedWrap(false);
        cast<BinaryOperator>(Op)->setHasNoSignedWrap(false);
      } else if (PossiblyExactOperator *Op =
                     dyn_cast<PossiblyExactOperator>(Inst)) {
        cast<BinaryOperator>(Op)->setIsExact(false);
      } else if (LoadInst *Load = dyn_cast<LoadInst>(Inst)) {
        Load->setAlignment(normalizeAlignment(
                               DL, Load->getAlignment(),
                               Load->getType(),
                               Load->isAtomic()));
      } else if (StoreInst *Store = dyn_cast<StoreInst>(Inst)) {
        Store->setAlignment(normalizeAlignment(
                                DL, Store->getAlignment(),
                                Store->getValueOperand()->getType(),
                                Store->isAtomic()));
      }
    }
  }
}

bool StripAttributes::runOnModule(Module &M) {
  DataLayout DL(&M);
  for (Module::iterator Func = M.begin(), E = M.end(); Func != E; ++Func) {
    // Avoid stripping attributes from intrinsics because the
    // constructor for Functions just adds them back again.  It would
    // be confusing if the attributes were sometimes present on
    // intrinsics and sometimes not.
    if (!Func->isIntrinsic()) {
      stripGlobalValueAttrs(Func);
      stripFunctionAttrs(&DL, Func);
    }
  }
  for (Module::global_iterator GV = M.global_begin(), E = M.global_end();
       GV != E; ++GV) {
    stripGlobalValueAttrs(GV);
  }
  return true;
}

ModulePass *llvm::createStripAttributesPass() {
  return new StripAttributes();
}
