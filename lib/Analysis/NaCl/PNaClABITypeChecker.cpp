//===- PNaClABITypeChecker.cpp - Verify PNaCl ABI rules -------------------===//
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

#include "PNaClABITypeChecker.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Metadata.h"

using namespace llvm;

bool PNaClABITypeChecker::isValidParamType(const Type *Ty) {
  if (!isValidScalarType(Ty))
    return false;
  if (const IntegerType *IntTy = dyn_cast<IntegerType>(Ty)) {
    // PNaCl requires function arguments and return values to be 32
    // bits or larger.  This avoids exposing architecture
    // ABI-dependent differences about whether arguments or return
    // values are zero-extended when calling a function with the wrong
    // prototype.
    if (IntTy->getBitWidth() < 32)
      return false;
  }
  return true;
}

bool PNaClABITypeChecker::isValidFunctionType(const FunctionType *FTy) {
  if (FTy->isVarArg())
    return false;
  if (!isValidParamType(FTy->getReturnType()))
    return false;
  for (unsigned I = 0, E = FTy->getNumParams(); I < E; ++I) {
    if (!isValidParamType(FTy->getParamType(I)))
      return false;
  }
  return true;
}

bool PNaClABITypeChecker::isValidScalarType(const Type *Ty) {
  switch (Ty->getTypeID()) {
    case Type::IntegerTyID: {
      unsigned Width = cast<const IntegerType>(Ty)->getBitWidth();
      return Width == 1 || Width == 8 || Width == 16 ||
             Width == 32 || Width == 64;
    }
    case Type::VoidTyID:
    case Type::FloatTyID:
    case Type::DoubleTyID:
      return true;
    default:
      return false;
  }
}
