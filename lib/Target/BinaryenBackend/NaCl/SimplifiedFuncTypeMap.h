//===-- SimplifiedFuncTypeMap.h - Consistent type remapping------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_SIMPLIFIEDFUNCTYPEMAP_H
#define LLVM_SIMPLIFIEDFUNCTYPEMAP_H

#include <llvm/ADT/DenseMap.h>
#include "llvm/IR/DerivedTypes.h"

namespace llvm {
// SimplifiedFuncTypeMap provides a consistent type map, given a rule
// for mapping function types - which is provided by implementing
// getSimpleFuncType.
// A few transformations require changing function types, for example
// SimplifyStructRegSignatures or PromoteIntegers. When doing so, we also
// want to change any references to function types - for example structs
// with fields typed as function pointer(s). Structs are not interned by LLVM,
// which is what SimplifiedFuncTypeMap addresses.
class SimplifiedFuncTypeMap {
public:
  typedef DenseMap<StructType *, StructType *> StructMap;
  Type *getSimpleType(LLVMContext &Ctx, Type *Ty);
  virtual ~SimplifiedFuncTypeMap() {}

protected:
  class MappingResult {
  public:
    MappingResult(Type *ATy, bool Chg) {
      Ty = ATy;
      Changed = Chg;
    }
    bool isChanged() { return Changed; }
    Type *operator->() { return Ty; }
    operator Type *() { return Ty; }

  private:
    Type *Ty;
    bool Changed;
  };

  virtual MappingResult getSimpleFuncType(LLVMContext &Ctx,
                                          StructMap &Tentatives,
                                          FunctionType *OldFnTy) = 0;

  typedef SmallVector<Type *, 8> ParamTypeVector;
  DenseMap<Type *, Type *> MappedTypes;

  MappingResult getSimpleAggregateTypeInternal(LLVMContext &Ctx, Type *Ty,
                                               StructMap &Tentatives);

  bool isChangedStruct(LLVMContext &Ctx, StructType *StructTy,
                       ParamTypeVector &ElemTypes, StructMap &Tentatives);
};
}
#endif // LLVM_SIMPLIFIEDFUNCTYPEMAP_H
