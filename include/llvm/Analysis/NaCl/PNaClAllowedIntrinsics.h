//===- PNaClAllowedIntrinsics.h - Set of allowed intrinsics -----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Declares class that holds set of allowed PNaCl intrinsics.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_NACL_PNACLALLOWEDINTRINSICS_H
#define LLVM_ANALYSIS_NACL_PNACLALLOWEDINTRINSICS_H

#include "llvm/ADT/StringMap.h"
#include "llvm/IR/Intrinsics.h"

namespace llvm {

class LLVMContext;
class Function;
class FunctionType;

// Holds the set of allowed instrinsics.
class PNaClAllowedIntrinsics {
  PNaClAllowedIntrinsics(const PNaClAllowedIntrinsics&) = delete;
  void operator=(const PNaClAllowedIntrinsics&) = delete;
public:
  PNaClAllowedIntrinsics(LLVMContext *Context);

  // Checks if there is an allowable PNaCl intrinsic function with the
  // given name and type signature.
  bool isAllowed(const std::string &FcnName, const FunctionType *FcnType) {
    return isIntrinsicName(FcnName) && FcnType == getIntrinsicType(FcnName);
  }
  // Checks if Func is an allowed PNaCl intrinsic function.  Note:
  // This function also allows debugging intrinsics if
  // PNaClABIAllowDebugMetadata is true.
  bool isAllowed(const Function *Func);

  // Returns the type signature for the Name'd intrinsic, if entered
  // via a call to AddIntrinsic. Returns 0 otherwise (implying we
  // don't know the expected type signature).
  FunctionType *getIntrinsicType(const std::string &Name) {
    return isIntrinsicName(Name) ? TypeMap[Name] : 0;
  }

  static bool isAllowedDebugInfoIntrinsic(unsigned IntrinsicID);

private:
  LLVMContext *Context;
  // Maps from an allowed intrinsic's name to its type.
  StringMap<FunctionType *> TypeMap;

  // Tys is an array of type parameters for the intrinsic.  This
  // defaults to an empty array.
  void addIntrinsic(Intrinsic::ID ID,
                    ArrayRef<Type *> Tys = ArrayRef<Type*>());

  // Returns true if a valid PNaCl intrinsic name.
  bool isIntrinsicName(const std::string &Name) {
    return TypeMap.count(Name) == 1;
  }

  // Returns true if intrinsic ID is allowed as a PNaCl intrinsic.
  bool isAllowedIntrinsicID(unsigned ID);
};

}

#endif // LLVM_ANALYSIS_NACL_PNACLALLOWEDINTRINSICS_H
