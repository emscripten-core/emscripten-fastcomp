//===- PNaClABITypeChecker.h - Verify PNaCl ABI rules -----------*- C++ -*-===//
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

#ifndef LLVM_ANALYSIS_NACL_PNACLABITYPECHECKER_H
#define LLVM_ANALYSIS_NACL_PNACLABITYPECHECKER_H

#include "llvm/ADT/DenseSet.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {
class FunctionType;

class PNaClABITypeChecker {
  PNaClABITypeChecker(const PNaClABITypeChecker&) = delete;
  void operator=(const PNaClABITypeChecker&) = delete;
public:
  // Returns true if Ty is a valid argument or return value type for PNaCl.
  static bool isValidParamType(const Type *Ty);

  // Returns true if Ty is a valid function type for PNaCl.
  static bool isValidFunctionType(const FunctionType *FTy);

  // Returns true if Ty is a valid non-derived type for PNaCl.
  static bool isValidScalarType(const Type *Ty);

  // Returns true if Ty is a valid vector type for PNaCl.
  static bool isValidVectorType(const Type *Ty);

  // Returns true if type Ty can be used in (integer) arithmetic operations.
  static bool isValidIntArithmeticType(const Type *Ty);

  // Returns true if type Ty can be used to define the test condition of
  // a switch instruction.
  static bool isValidSwitchConditionType(const Type *Ty) {
    return PNaClABITypeChecker::isValidIntArithmeticType(Ty);
  }
  // Returns error message showing what was expected when given the
  // switch condition type Ty. Assumes isValidSwitchConditionType(Ty)
  // returned false.
  static const char *ExpectedSwitchConditionType(const Type *Ty) {
    if (!Ty->isIntegerTy())
      return "switch not on integer type";
    if (Ty->isIntegerTy(1))
      return "switch on i1 not allowed";
    return "switch disallowed for integer type";
  }

  // There's no built-in way to get the name of a type, so use a
  // string ostream to print it.
  static std::string getTypeName(const Type *T) {
    std::string TypeName;
    raw_string_ostream N(TypeName);
    T->print(N);
    return N.str();
  }

  // Returns true if T1 is equivalent to T2, converting to i32 if
  // a pointer type.
  static bool IsPointerEquivType(Type *T1, Type *T2) {
    if (T1->isPointerTy()) return T2->isIntegerTy(32);
    if (T2->isPointerTy()) return T1->isIntegerTy(32);
    return T1 == T2;
  }

};
} // namespace llvm

#endif // LLVM_ANALYSIS_NACL_PNACLABITYPECHECKER_H
