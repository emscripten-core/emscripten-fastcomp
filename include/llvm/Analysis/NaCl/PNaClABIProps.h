//===- PNaClABIProps.h - Verify PNaCl ABI Properties ----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Verify PNaCl ABI properties.
//
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_NACL_PNACLABIPROPS_H
#define LLVM_ANALYSIS_NACL_PNACLABIPROPS_H

#include "llvm/ADT/APInt.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/Type.h"

namespace llvm {

class NamedMDNode;
class DataLayout;

// Checks properties needed to verify IR constructs. Unlike
// PNaClABIVerifyFunctions and PNaClABIVerifyModule, this class is
// pass-free, and checks individual elements within IR.
class PNaClABIProps {
  PNaClABIProps(const PNaClABIProps&) = delete;
  void operator=(const PNaClABIProps&) = delete;
public:
  // Returns true if metadata kind MDKind is allowed.
  static bool isWhitelistedMetadata(unsigned MDKind);
  // Returns true if metadata is allowed.
  static bool isWhitelistedMetadata(const NamedMDNode *MD);
  // Returns true if integer constant Idx is in [0..NumElements).
  static bool isVectorIndexSafe(const APInt &Idx,
                                unsigned NumElements) {
    return Idx.ult(NumElements);
  }
  // Returns true if Alignment is allowed for type Ty, assuming DL.
  static bool isAllowedAlignment(const DataLayout *DL, uint64_t Alignment,
                                 const Type *Ty);

  // Returns true if alloca type Ty is correct.
  static bool isAllocaAllocatedType(const Type *Ty) {
    return Ty->isIntegerTy(8);
  }
  // Returns true if the type associated with the size field in an alloca
  // instruction is valid.
  static bool isAllocaSizeType(const Type *Ty) {
    return Ty->isIntegerTy(32);
  }
  // Returns string describing expected type for the size field in an
  // alloca instruction.
  static const char *ExpectedAllocaSizeType() {
    return "alloca array size is not i32";
  }
  // Returns the name for the given calling convention.
  static const char *CallingConvName(CallingConv::ID CallingConv);
  // Returns true if CallingConv is valid.
  static bool isValidCallingConv(CallingConv::ID CallingConv) {
    return CallingConv == CallingConv::C;
  }
  // Returns the name for linkage type LT.
  static const char *LinkageName(GlobalValue::LinkageTypes LT);
  // Returns true if Linkage is valid.
  static bool isValidGlobalLinkage(GlobalValue::LinkageTypes Linkage);
  // Returns kind of global value name, based on IsFunction.
  static const char *GVTypeName(bool IsFunction) {
    return IsFunction ? "Function" : "Variable";
  }
};

}

#endif  // LLVM_ANALYSIS_NACL_PNACLABIPROPS_H
