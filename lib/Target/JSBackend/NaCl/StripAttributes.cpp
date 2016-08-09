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
//  * The "unnamed_addr" attribute on functions and global variables.
//  * The distinction between "internal" and "private" linkage.
//  * "protected" and "internal" visibility of functions and globals.
//  * All sections are stripped. A few sections cause warnings.
//  * The arithmetic attributes "nsw", "nuw" and "exact".
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/Pass.h"
#include "llvm/IR/CallSite.h"
#include "llvm/Transforms/NaCl.h"
#include "llvm/Support/raw_ostream.h"

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

    bool runOnModule(Module &M) override;
  };
}

char StripAttributes::ID = 0;
INITIALIZE_PASS(StripAttributes, "nacl-strip-attributes",
                "Strip out attributes that are not part of PNaCl's ABI",
                false, false)

static void CheckAttributes(AttributeSet Attrs) {
  for (unsigned Slot = 0; Slot < Attrs.getNumSlots(); ++Slot) {
    for (AttributeSet::iterator Attr = Attrs.begin(Slot), E = Attrs.end(Slot);
         Attr != E; ++Attr) {
      if (!Attr->isEnumAttribute()) {
        continue;
      }
      switch (Attr->getKindAsEnum()) {
        // The vast majority of attributes are hints that can safely
        // be removed, so don't complain if we see attributes we don't
        // recognize.
        default:

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

        // A few attributes can change program behaviour if removed,
        // so check for these.
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
      }
    }
  }
}

static const char* ShouldWarnAboutSection(const char* Section) {
  static const char* SpecialSections[] = {
    ".init_array",
    ".init",
    ".fini_array",
    ".fini",

    // Java/LSB:
    ".jcr",

    // LSB:
    ".ctors",
    ".dtors",
  };

  for (auto CheckSection : SpecialSections) {
    if (strcmp(Section, CheckSection) == 0) {
      return CheckSection;
    }
  }

  return nullptr;
}

void stripGlobalValueAttrs(GlobalValue *GV) {
  // In case source code uses __attribute__((visibility("hidden"))) or
  // __attribute__((visibility("protected"))), strip these attributes.
  GV->setVisibility(GlobalValue::DefaultVisibility);

  GV->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);

  if (GV->hasSection()) {
    const char *Section = GV->getSection().data();
    // check for a few special cases
    if (const char *WarnSection = ShouldWarnAboutSection(Section)) {
      errs() << "Warning: " << GV->getName() <<
        " will have its section (" <<
        WarnSection << ") stripped.\n";
    }

    if(GlobalObject* GO = dyn_cast<GlobalObject>(GV)) {
      GO->setSection("");
    }
    // Nothing we can do if GV isn't a GlobalObject.
  }

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

void stripFunctionAttrs(DataLayout *DL, Function *F) {
  CheckAttributes(F->getAttributes());
  F->setAttributes(AttributeSet());
  F->setCallingConv(CallingConv::C);
  F->setAlignment(0);

  for (BasicBlock &BB : *F) {
    for (Instruction &I : BB) {
      CallSite Call(&I);
      if (Call) {
        CheckAttributes(Call.getAttributes());
        Call.setAttributes(AttributeSet());
        Call.setCallingConv(CallingConv::C);
      } else if (OverflowingBinaryOperator *Op =
                     dyn_cast<OverflowingBinaryOperator>(&I)) {
        cast<BinaryOperator>(Op)->setHasNoUnsignedWrap(false);
        cast<BinaryOperator>(Op)->setHasNoSignedWrap(false);
      } else if (PossiblyExactOperator *Op =
                     dyn_cast<PossiblyExactOperator>(&I)) {
        cast<BinaryOperator>(Op)->setIsExact(false);
      }
    }
  }
}

bool StripAttributes::runOnModule(Module &M) {
  DataLayout DL(&M);
  for (Function &F : M)
    // Avoid stripping attributes from intrinsics because the
    // constructor for Functions just adds them back again.  It would
    // be confusing if the attributes were sometimes present on
    // intrinsics and sometimes not.
    if (!F.isIntrinsic()) {
      stripGlobalValueAttrs(&F);
      stripFunctionAttrs(&DL, &F);
    }

  for (GlobalVariable &GV : M.globals())
    stripGlobalValueAttrs(&GV);

  return true;
}

ModulePass *llvm::createStripAttributesPass() {
  return new StripAttributes();
}
