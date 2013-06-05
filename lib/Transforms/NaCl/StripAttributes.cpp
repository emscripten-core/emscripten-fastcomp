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
// stable ABI.  Currently, this strips out attributes from functions
// and function calls.
//
// TODO(mseaborn): Strip out the following too:
//
//  * Calling conventions from functions and function calls.
//  * "nuw" and "nsw" arithmetic attributes.
//  * "align" attributes from integer memory accesses.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/CallSite.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

namespace {
  class StripAttributes : public FunctionPass {
  public:
    static char ID; // Pass identification, replacement for typeid
    StripAttributes() : FunctionPass(ID) {
      initializeStripAttributesPass(*PassRegistry::getPassRegistry());
    }

    virtual bool runOnFunction(Function &Func);
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

bool StripAttributes::runOnFunction(Function &Func) {
  CheckAttributes(Func.getAttributes());
  Func.setAttributes(AttributeSet());

  for (Function::iterator BB = Func.begin(), E = Func.end();
       BB != E; ++BB) {
    for (BasicBlock::iterator Inst = BB->begin(), E = BB->end();
         Inst != E; ++Inst) {
      CallSite Call(Inst);
      if (Call) {
        CheckAttributes(Call.getAttributes());
        Call.setAttributes(AttributeSet());
      }
    }
  }

  return true;
}

FunctionPass *llvm::createStripAttributesPass() {
  return new StripAttributes();
}
