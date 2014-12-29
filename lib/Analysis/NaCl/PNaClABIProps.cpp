//===- PNaClABIProps.cpp - Verify PNaCl ABI Function Rules ----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Verify function-level PNaCl ABI properties, at the construct level.
//
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/NaCl.h"
#include "llvm/Analysis/NaCl/PNaClABIProps.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"

using namespace llvm;

bool PNaClABIProps::isWhitelistedMetadata(unsigned MDKind) {
  return MDKind == LLVMContext::MD_dbg && PNaClABIAllowDebugMetadata;
}

bool PNaClABIProps::isWhitelistedMetadata(const NamedMDNode *MD) {
  return PNaClABIAllowDebugMetadata &&
         (MD->getName().startswith("llvm.dbg.") ||
          // "Debug Info Version" is in llvm.module.flags.
          MD->getName().equals("llvm.module.flags"));
}

bool PNaClABIProps::
isAllowedAlignment(const DataLayout *DL, uint64_t Alignment,
                   const Type *Ty) {
  // Non-atomic integer operations must always use "align 1", since we do not
  // want the backend to generate code with non-portable undefined behaviour
  // (such as misaligned access faults) if user code specifies "align 4" but
  // uses a misaligned pointer.  As a concession to performance, we allow larger
  // alignment values for floating point types, and we only allow vectors to be
  // aligned by their element's size.
  //
  // TODO(jfb) Allow vectors to be marked as align == 1. This requires proper
  //           testing on each supported ISA, and is probably not as common as
  //           align == elemsize.
  //
  // To reduce the set of alignment values that need to be encoded in pexes, we
  // disallow other alignment values.  We require alignments to be explicit by
  // disallowing Alignment == 0.
  if (Alignment > std::numeric_limits<uint64_t>::max() / CHAR_BIT)
    return false; // No overflow assumed below.
  else if (const VectorType *VTy = dyn_cast<VectorType>(Ty))
    return !VTy->getElementType()->isIntegerTy(1) &&
           (Alignment * CHAR_BIT ==
            DL->getTypeSizeInBits(VTy->getElementType()));
  else
    return Alignment == 1 ||
           (Ty->isDoubleTy() && Alignment == 8) ||
           (Ty->isFloatTy() && Alignment == 4);
}

const char *PNaClABIProps::CallingConvName(CallingConv::ID CallingConv) {
  // TODO(kschimpf): Add more calling conventions.
  switch (CallingConv) {
  case CallingConv::C:                return "ccc";
  case CallingConv::Fast:             return "fastcc";
  case CallingConv::Cold:             return "cold";
  default:
    return "unknown";
  }
}

const char *PNaClABIProps::LinkageName(GlobalValue::LinkageTypes LT) {
  // This logic is taken from PrintLinkage in lib/IR/AsmWriter.cpp
  switch (LT) {
  case GlobalValue::ExternalLinkage:      return "external ";
  case GlobalValue::PrivateLinkage:       return "private ";
  case GlobalValue::InternalLinkage:      return "internal ";
  case GlobalValue::LinkOnceAnyLinkage:   return "linkonce ";
  case GlobalValue::LinkOnceODRLinkage:   return "linkonce_odr ";
  case GlobalValue::WeakAnyLinkage:       return "weak ";
  case GlobalValue::WeakODRLinkage:       return "weak_odr ";
  case GlobalValue::CommonLinkage:        return "common ";
  case GlobalValue::AppendingLinkage:     return "appending ";
  case GlobalValue::ExternalWeakLinkage:  return "extern_weak ";
  case GlobalValue::AvailableExternallyLinkage:
    return "available_externally ";
    default: return "unknown";
  }
}

bool PNaClABIProps::isValidGlobalLinkage(GlobalValue::LinkageTypes Linkage) {
  switch (Linkage) {
    case GlobalValue::ExternalLinkage:
      return true;
    case GlobalValue::InternalLinkage:
      return true;
    default:
      return false;
  }
}
