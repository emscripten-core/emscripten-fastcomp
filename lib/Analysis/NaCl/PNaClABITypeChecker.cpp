//===- PNaClABICheckTypes.h - Verify PNaCl ABI rules --------===//
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
  if (!isValidType(Ty))
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

bool PNaClABITypeChecker::isValidType(const Type *Ty) {
  if (VisitedTypes.count(Ty))
    return VisitedTypes[Ty];

  // unsigned Width;
  bool Valid = false;
  switch (Ty->getTypeID()) {
    // Allowed primitive types
    case Type::VoidTyID:
    case Type::FloatTyID:
    case Type::DoubleTyID:
    case Type::LabelTyID:
    case Type::MetadataTyID:
      Valid = true;
      break;
      // Disallowed primitive types
    case Type::HalfTyID:
    case Type::X86_FP80TyID:
    case Type::FP128TyID:
    case Type::PPC_FP128TyID:
    case Type::X86_MMXTyID:
      Valid = false;
      break;
      // Derived types
    case Type::VectorTyID:
      Valid = false;
      break;
    case Type::IntegerTyID:
      // The check for integer sizes below currently does not pass on
      // all NaCl tests because some LLVM passes introduce unusual
      // integer sizes.
      // TODO(mseaborn): Re-enable the check when it passes.
      // See https://code.google.com/p/nativeclient/issues/detail?id=3360
      Valid = true;
      // Width = cast<const IntegerType>(Ty)->getBitWidth();
      // Valid = (Width == 1 || Width == 8 || Width == 16 ||
      //          Width == 32 || Width == 64);
      break;
    case Type::FunctionTyID:
      Valid = isValidFunctionType(cast<FunctionType>(Ty));
      break;
    case Type::StructTyID:
    case Type::ArrayTyID:
    case Type::PointerTyID:
      // These types are valid if their contained or pointed-to types are
      // valid. Since struct/pointer subtype relationships may be circular,
      // mark the current type as valid to avoid infinite recursion
      Valid = true;
      VisitedTypes[Ty] = true;
      for (Type::subtype_iterator I = Ty->subtype_begin(),
               E = Ty->subtype_end(); I != E; ++I)
        Valid &= isValidType(*I);
      break;
      // Handle NumTypeIDs, and no default case,
      // so we get a warning if new types are added
    case Type::NumTypeIDs:
      Valid = false;
      break;
  }

  VisitedTypes[Ty] = Valid;
  return Valid;
}

Type *PNaClABITypeChecker::checkTypesInConstant(const Constant *V) {
  if (!V) return NULL;
  if (VisitedConstants.count(V))
    return VisitedConstants[V];

  if (!isValidType(V->getType())) {
    VisitedConstants[V] = V->getType();
    return V->getType();
  }

  // Check for BlockAddress because it contains a non-Constant
  // BasicBlock operand.
  // TODO(mseaborn): This produces an error which is misleading
  // because it complains about the type being "i8*".  It should
  // instead produce an error saying that BlockAddress and computed
  // gotos are not allowed.
  if (isa<BlockAddress>(V)) {
    VisitedConstants[V] = V->getType();
    return V->getType();
  }

  // Operand values must also be valid. Values may be circular, so
  // mark the current value as valid to avoid infinite recursion.
  VisitedConstants[V] = NULL;
  for (Constant::const_op_iterator I = V->op_begin(),
           E = V->op_end(); I != E; ++I) {
    Type *Invalid = checkTypesInConstant(cast<Constant>(*I));
    if (Invalid) {
      VisitedConstants[V] = Invalid;
      return Invalid;
    }
  }
  VisitedConstants[V] = NULL;
  return NULL;
}


// MDNodes don't support the same way of iterating over operands that Users do
Type *PNaClABITypeChecker::checkTypesInMDNode(const MDNode *N) {
  if (VisitedConstants.count(N))
    return VisitedConstants[N];

  for (unsigned i = 0, e = N->getNumOperands(); i != e; i++) {
    if (Value *Op = N->getOperand(i)) {
      if (Type *Invalid = checkTypesInConstant(dyn_cast<Constant>(Op))) {
        VisitedConstants[N] = Invalid;
        return Invalid;
      }
    }
  }
  return NULL;
}
