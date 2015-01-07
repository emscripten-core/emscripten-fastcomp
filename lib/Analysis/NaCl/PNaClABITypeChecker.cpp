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

#include "llvm/Analysis/NaCl/PNaClABITypeChecker.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Metadata.h"

using namespace llvm;

bool PNaClABITypeChecker::isValidParamType(const Type *Ty) {
  if (!(isValidScalarType(Ty) || isValidVectorType(Ty)))
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

namespace {
static inline bool NaClIsValidIntType(const Type *Ty) {
  unsigned Width = cast<const IntegerType>(Ty)->getBitWidth();
  return Width == 1 || Width == 8 || Width == 16 ||
      Width == 32 || Width == 64;
}
}

bool PNaClABITypeChecker::isValidScalarType(const Type *Ty) {
  switch (Ty->getTypeID()) {
    case Type::IntegerTyID: {
      return NaClIsValidIntType(Ty);
    }
    case Type::VoidTyID:
    case Type::FloatTyID:
    case Type::DoubleTyID:
      return true;
    default:
      return false;
  }
}

// TODO(jfb) Handle 64-bit int and double, and 2xi1.
bool PNaClABITypeChecker::isValidVectorType(const Type *Ty) {
  if (!Ty->isVectorTy())
    return false;
  unsigned Elems = Ty->getVectorNumElements();
  const Type *VTy = Ty->getVectorElementType();

  switch (VTy->getTypeID()) {
  case Type::IntegerTyID: {
    unsigned Width = cast<const IntegerType>(VTy)->getBitWidth();
    switch (Width) {
    case 1: return Elems == 4 || Elems == 8 || Elems == 16;
    case 8: return Elems == 16;
    case 16: return Elems == 8;
    case 32: return Elems == 4;
    default: return false;
    }
  }
  case Type::FloatTyID:
    return Elems == 4;
  default:
    return false;
  }
}

namespace {
static inline bool NaClIsValidIntArithmeticType(const Type *Ty) {
  return Ty->isIntegerTy() && !Ty->isIntegerTy(1)
      && NaClIsValidIntType(Ty);
}

}

bool PNaClABITypeChecker::isValidIntArithmeticType(const Type *Ty) {
  if (isValidVectorType(Ty))
    return NaClIsValidIntArithmeticType(Ty->getVectorElementType());
  return NaClIsValidIntArithmeticType(Ty);
}
