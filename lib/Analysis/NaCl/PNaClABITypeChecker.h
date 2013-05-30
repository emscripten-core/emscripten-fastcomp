//===- CheckTypes.h - Verify PNaCl ABI rules --------===//
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
class Constant;
class FunctionType;
class MDNode;
class Value;

class PNaClABITypeChecker {
  // Returns true if Ty is a valid argument or return value type for PNaCl.
  bool isValidParamType(const Type *Ty);

  // Returns true if Ty is a valid function type for PNaCl.
  bool isValidFunctionType(const FunctionType *FTy);

 public:
  // Returns true if Ty is a valid type for PNaCl.
  bool isValidType(const Type *Ty);

  // If the value contains an invalid type, return a pointer to the type.
  // Return null if there are no invalid types.
  Type *checkTypesInConstant(const Constant *V);

  // If the Metadata node contains an invalid type, return a pointer to the
  // type. Return null if there are no invalid types.
  Type *checkTypesInMDNode(const MDNode *V);

  // There's no built-in way to get the name of a type, so use a
  // string ostream to print it.
  static std::string getTypeName(const Type *T) {
    std::string TypeName;
    raw_string_ostream N(TypeName);
    T->print(N);
    return N.str();
  }

 private:
  // To avoid walking constexprs and types multiple times, keep a cache of
  // what we have seen. This is also used to prevent infinite recursion e.g.
  // in case of structures like linked lists with pointers to themselves.
  DenseMap<const Value*, Type*> VisitedConstants;
  DenseMap<const Type*, bool> VisitedTypes;
};
} // namespace llvm

#endif // LIB_ANALYSIS_NACL_CHECKTYPES_H
