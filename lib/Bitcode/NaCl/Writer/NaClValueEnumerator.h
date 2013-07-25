//===-- Bitcode/NaCl/Writer/NaClValueEnumerator.h - ----------*- C++ -*-===//
//      Number values.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This class gives values and types Unique ID's.
//
//===----------------------------------------------------------------------===//

#ifndef NACL_VALUE_ENUMERATOR_H
#define NACL_VALUE_ENUMERATOR_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallVector.h"
#include <vector>

namespace llvm {

class Type;
class Value;
class Instruction;
class BasicBlock;
class Function;
class Module;
class ValueSymbolTable;
class raw_ostream;

class NaClValueEnumerator {
public:
  typedef std::vector<Type*> TypeList;

  // For each value, we remember its Value* and occurrence frequency.
  typedef std::vector<std::pair<const Value*, unsigned> > ValueList;
private:
  // Defines unique ID's for each type.
  typedef DenseMap<Type*, unsigned> TypeMapType;
  TypeMapType TypeMap;
  // Defines the number of references to each type. If defined,
  // we are in the first pass of collecting types, and reference counts
  // should be added to the map. If undefined, we are in the second pass
  // that actually assigns type IDs, based on frequency counts found in
  // the first pass.
  typedef TypeMapType TypeCountMapType;
  TypeCountMapType* TypeCountMap;

  TypeList Types;

  typedef DenseMap<const Value*, unsigned> ValueMapType;
  ValueMapType ValueMap;
  ValueList Values;

  typedef DenseMap<const Instruction*, unsigned> InstructionMapType;
  InstructionMapType InstructionMap;
  unsigned InstructionCount;

  /// BasicBlocks - This contains all the basic blocks for the currently
  /// incorporated function.  Their reverse mapping is stored in ValueMap.
  std::vector<const BasicBlock*> BasicBlocks;

  /// When a function is incorporated, this is the size of the Values list
  /// before incorporation.
  unsigned NumModuleValues;

  unsigned FirstFuncConstantID;
  unsigned FirstInstID;

  /// Holds values that have been forward referenced within a function.
  /// Used to make sure we don't generate more forward reference declarations
  /// than necessary.
  SmallSet<unsigned, 32> FnForwardTypeRefs;

  // The index of the first global variable ID in the bitcode file.
  unsigned FirstGlobalVarID;
  // The number of global variable IDs defined in the bitcode file.
  unsigned NumGlobalVarIDs;

  // The version of PNaCl bitcode to generate.
  uint32_t PNaClVersion;

  NaClValueEnumerator(const NaClValueEnumerator &) LLVM_DELETED_FUNCTION;
  void operator=(const NaClValueEnumerator &) LLVM_DELETED_FUNCTION;
public:
  NaClValueEnumerator(const Module *M, uint32_t PNaClVersion);

  void dump() const;
  void print(raw_ostream &OS, const ValueMapType &Map, const char *Name) const;

  unsigned getFirstGlobalVarID() const {
    return FirstGlobalVarID;
  }

  unsigned getNumGlobalVarIDs() const {
    return NumGlobalVarIDs;
  }

  unsigned getValueID(const Value *V) const;

  unsigned getTypeID(Type *T) const {
    TypeMapType::const_iterator I = TypeMap.find(T);
    assert(I != TypeMap.end() && "Type not in NaClValueEnumerator!");
    return I->second-1;
  }

  unsigned getInstructionID(const Instruction *I) const;
  void setInstructionID(const Instruction *I);

  /// getFunctionConstantRange - Return the range of values that corresponds to
  /// function-local constants.
  void getFunctionConstantRange(unsigned &Start, unsigned &End) const {
    Start = FirstFuncConstantID;
    End = FirstInstID;
  }

  /// \brief Inserts the give value into the set of known function forward
  /// value type refs. Returns true if the value id is added to the set.
  bool InsertFnForwardTypeRef(unsigned ValID) {
    return FnForwardTypeRefs.insert(ValID);
  }

  const ValueList &getValues() const { return Values; }
  const TypeList &getTypes() const { return Types; }
  const std::vector<const BasicBlock*> &getBasicBlocks() const {
    return BasicBlocks;
  }

  /// incorporateFunction/purgeFunction - If you'd like to deal with a function,
  /// use these two methods to get its data into the NaClValueEnumerator!
  ///
  void incorporateFunction(const Function &F);
  void purgeFunction();

private:
  void OptimizeTypes(const Module *M);
  void OptimizeConstants(unsigned CstStart, unsigned CstEnd);

  void EnumerateValue(const Value *V);
  void EnumerateType(Type *T, bool InsideOptimizeTypes=false);
  void EnumerateOperandType(const Value *V);

  void EnumerateValueSymbolTable(const ValueSymbolTable &ST);
};

} // End llvm namespace

#endif
