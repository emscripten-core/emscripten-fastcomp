//===- PNaClABITypeChecker.h - Verify PNaCl ABI rules ---------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Common type-checking code for module and function-level passes
//
//
//===----------------------------------------------------------------------===//

#ifndef LIB_ANALYSIS_NACL_CHECKTYPES_H
#define LIB_ANALYSIS_NACL_CHECKTYPES_H

#include "llvm/ADT/DenseSet.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {
class FunctionType;

class PNaClABITypeChecker {
  // Returns true if Ty is a valid argument or return value type for PNaCl.
  static bool isValidParamType(const Type *Ty);

 public:
  // Returns true if Ty is a valid function type for PNaCl.
  static bool isValidFunctionType(const FunctionType *FTy);

  // Returns true if Ty is a valid non-derived type for PNaCl.
  static bool isValidScalarType(const Type *Ty);

  // There's no built-in way to get the name of a type, so use a
  // string ostream to print it.
  static std::string getTypeName(const Type *T) {
    std::string TypeName;
    raw_string_ostream N(TypeName);
    T->print(N);
    return N.str();
  }
};
} // namespace llvm

#endif // LIB_ANALYSIS_NACL_CHECKTYPES_H
