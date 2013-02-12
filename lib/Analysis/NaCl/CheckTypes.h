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
#include "llvm/IR/DerivedTypes.h"
#include "llvm/Support/raw_ostream.h"

class TypeChecker {
 public:
  bool isValidType(const llvm::Type *Ty) {
    if (VisitedTypes.count(Ty))
      return VisitedTypes[Ty];

    unsigned Width;
    bool Valid = false;
    switch (Ty->getTypeID()) {
      // Allowed primitive types
      case llvm::Type::VoidTyID:
      case llvm::Type::FloatTyID:
      case llvm::Type::DoubleTyID:
      case llvm::Type::LabelTyID:
      case llvm::Type::MetadataTyID:
        Valid = true;
        break;
      // Disallowed primitive types
      case llvm::Type::HalfTyID:
      case llvm::Type::X86_FP80TyID:
      case llvm::Type::FP128TyID:
      case llvm::Type::PPC_FP128TyID:
      case llvm::Type::X86_MMXTyID:
        Valid = false;
        break;
      // Derived types
      case llvm::Type::VectorTyID:
        Valid = false;
        break;
      case llvm::Type::IntegerTyID:
        Width = llvm::cast<const llvm::IntegerType>(Ty)->getBitWidth();
        Valid = (Width == 1 || Width == 8 || Width == 16 ||
                 Width == 32 || Width == 64);
        break;
      case llvm::Type::FunctionTyID:
      case llvm::Type::StructTyID:
      case llvm::Type::ArrayTyID:
      case llvm::Type::PointerTyID:
        // These types are valid if their contained or pointed-to types are
        // valid. Since struct/pointer subtype relationships may be circular,
        // mark the current type as valid to avoid infinite recursion
        Valid = true;
        VisitedTypes[Ty] = true;
        for (llvm::Type::subtype_iterator I = Ty->subtype_begin(),
                 E = Ty->subtype_end(); I != E; ++I)
          Valid &= isValidType(*I);
        break;
        // Handle NumTypeIDs, and no default case,
        // so we get a warning if new types are added
      case llvm::Type::NumTypeIDs:
        Valid = false;
        break;
    }

    VisitedTypes[Ty] = Valid;
    return Valid;
  }

  // If the value contains an invalid type, return a pointer to the type.
  // Return null if there are no invalid types.
  llvm::Type *checkTypesInValue(const llvm::Value *V) {
    // TODO: Checking types in values probably belongs in its
    // own value checker which also handles the various types of
    // constexpr (in particular, blockaddr constexprs cause this code
    // to assert rather than go off and try to verify the BBs of a function)
    // But this code is in a good consistent checkpoint-able state.
    assert(llvm::isa<llvm::Constant>(V));
    if (VisitedConstants.count(V))
      return VisitedConstants[V];

    if (!isValidType(V->getType())) {
      VisitedConstants[V] = V->getType();
      return V->getType();
    }

    // Operand values must also be valid. Values may be circular, so
    // mark the current value as valid to avoid infinite recursion.
    VisitedConstants[V] = NULL;
    const llvm::User *U = llvm::cast<llvm::User>(V);
    for (llvm::Constant::const_op_iterator I = U->op_begin(),
             E = U->op_end(); I != E; ++I) {
      llvm::Type *Invalid = checkTypesInValue(*I);
      if (Invalid) {
        VisitedConstants[V] = Invalid;
        return Invalid;
      }
    }
    VisitedConstants[V] = NULL;
    return NULL;
  }

  // There's no built-in way to get the name of a type, so use a
  // string ostream to print it.
  static std::string getTypeName(const llvm::Type *T) {
    std::string TypeName;
    llvm::raw_string_ostream N(TypeName);
    T->print(N);
    return N.str();
  }

 private:
  // To avoid walking constexprs and types multiple times, keep a cache of
  // what we have seen. This is also used to prevent infinite recursion e.g.
  // in case of structures like linked lists with pointers to themselves.
  llvm::DenseMap<const llvm::Value*, llvm::Type*> VisitedConstants;
  llvm::DenseMap<const llvm::Type*, bool> VisitedTypes;
};

#endif // LIB_ANALYSIS_NACL_CHECKTYPES_H
