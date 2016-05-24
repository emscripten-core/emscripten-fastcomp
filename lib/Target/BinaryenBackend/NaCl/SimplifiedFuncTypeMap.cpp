//===-- SimplifiedFuncTypeMap.cpp - Consistent type remapping----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "SimplifiedFuncTypeMap.h"

using namespace llvm;

Type *SimplifiedFuncTypeMap::getSimpleType(LLVMContext &Ctx, Type *Ty) {
  auto Found = MappedTypes.find(Ty);
  if (Found != MappedTypes.end()) {
    return Found->second;
  }

  StructMap Tentatives;
  auto Ret = getSimpleAggregateTypeInternal(Ctx, Ty, Tentatives);
  assert(Tentatives.size() == 0);

  if (!Ty->isStructTy()) {
    // Structs are memoized in getSimpleAggregateTypeInternal.
    MappedTypes[Ty] = Ret;
  }
  return Ret;
}

// Transforms any type that could transitively reference a function pointer
// into a simplified type.
// We enter this function trying to determine the mapping of a type. Because
// of how structs are handled (not interned by llvm - see further comments
// below) we may be working with temporary types - types (pointers, for example)
// transitively referencing "tentative" structs. For that reason, we do not
// memoize anything here, except for structs. The latter is so that we avoid
// unnecessary repeated creation of types (pointers, function types, etc),
// as we try to map a given type.
SimplifiedFuncTypeMap::MappingResult
SimplifiedFuncTypeMap::getSimpleAggregateTypeInternal(LLVMContext &Ctx,
                                                      Type *Ty,
                                                      StructMap &Tentatives) {
  // Leverage the map for types we encounter on the way.
  auto Found = MappedTypes.find(Ty);
  if (Found != MappedTypes.end()) {
    return {Found->second, Found->second != Ty};
  }

  if (auto *OldFnTy = dyn_cast<FunctionType>(Ty)) {
    return getSimpleFuncType(Ctx, Tentatives, OldFnTy);
  }

  if (auto PtrTy = dyn_cast<PointerType>(Ty)) {
    auto NewTy = getSimpleAggregateTypeInternal(
        Ctx, PtrTy->getPointerElementType(), Tentatives);

    return {NewTy->getPointerTo(PtrTy->getAddressSpace()), NewTy.isChanged()};
  }

  if (auto ArrTy = dyn_cast<ArrayType>(Ty)) {
    auto NewTy = getSimpleAggregateTypeInternal(
        Ctx, ArrTy->getArrayElementType(), Tentatives);
    return {ArrayType::get(NewTy, ArrTy->getArrayNumElements()),
            NewTy.isChanged()};
  }

  if (auto VecTy = dyn_cast<VectorType>(Ty)) {
    auto NewTy = getSimpleAggregateTypeInternal(
        Ctx, VecTy->getVectorElementType(), Tentatives);
    return {VectorType::get(NewTy, VecTy->getVectorNumElements()),
            NewTy.isChanged()};
  }

  // LLVM doesn't intern identified structs (the ones with a name). This,
  // together with the fact that such structs can be recursive,
  // complicates things a bit. We want to make sure that we only change
  // "unsimplified" structs (those that somehow reference funcs that
  // are not simple).
  // We don't want to change "simplified" structs, otherwise converting
  // instruction types will become trickier.
  if (auto StructTy = dyn_cast<StructType>(Ty)) {
    ParamTypeVector ElemTypes;
    if (!StructTy->isLiteral()) {
      // Literals - struct without a name - cannot be recursive, so we
      // don't need to form tentatives.
      auto Found = Tentatives.find(StructTy);

      // Having a tentative means we are in a recursion trying to map this
      // particular struct, so arriving back to it is not a change.
      // We will determine if this struct is actually
      // changed by checking its other fields.
      if (Found != Tentatives.end()) {
        return {Found->second, false};
      }
      // We have never seen this struct, so we start a tentative.
      std::string NewName = StructTy->getStructName();
      NewName += ".simplified";
      StructType *Tentative = StructType::create(Ctx, NewName);
      Tentatives[StructTy] = Tentative;

      bool Changed = isChangedStruct(Ctx, StructTy, ElemTypes, Tentatives);

      Tentatives.erase(StructTy);
      // We can now decide the mapping of the struct. We will register it
      // early with MappedTypes, to avoid leaking tentatives unnecessarily.
      // We are leaking the created struct here, but there is no way to
      // correctly delete it.
      if (!Changed) {
        return {MappedTypes[StructTy] = StructTy, false};
      } else {
        Tentative->setBody(ElemTypes, StructTy->isPacked());
        return {MappedTypes[StructTy] = Tentative, true};
      }
    } else {
      bool Changed = isChangedStruct(Ctx, StructTy, ElemTypes, Tentatives);
      return {MappedTypes[StructTy] =
                  StructType::get(Ctx, ElemTypes, StructTy->isPacked()),
              Changed};
    }
  }

  // Anything else stays the same.
  return {Ty, false};
}

bool SimplifiedFuncTypeMap::isChangedStruct(LLVMContext &Ctx,
                                            StructType *StructTy,
                                            ParamTypeVector &ElemTypes,
                                            StructMap &Tentatives) {
  bool Changed = false;
  unsigned StructElemCount = StructTy->getStructNumElements();
  for (unsigned I = 0; I < StructElemCount; I++) {
    auto NewElem = getSimpleAggregateTypeInternal(
        Ctx, StructTy->getStructElementType(I), Tentatives);
    ElemTypes.push_back(NewElem);
    Changed |= NewElem.isChanged();
  }
  return Changed;
}