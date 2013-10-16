//===-- ExceptionInfoWriter.h - Generate C++ exception info------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef TRANSFORMS_NACL_EXCEPTIONINFOWRITER_H
#define TRANSFORMS_NACL_EXCEPTIONINFOWRITER_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"

namespace llvm {

// The ExceptionInfoWriter class converts the clauses of a
// "landingpad" instruction into data tables stored in global
// variables, which are interpreted by PNaCl's C++ runtime library.
// See ExceptionInfoWriter.cpp for a full description.
class ExceptionInfoWriter {
  LLVMContext *Context;
  StructType *ActionTableEntryTy;

  // Data for populating __pnacl_eh_type_table[], which is an array of
  // std::type_info* pointers.  Each of these pointers represents a
  // C++ exception type.
  SmallVector<Constant *, 10> TypeTableData;
  // Mapping from std::type_info* pointer to type ID (index in
  // TypeTableData).
  typedef DenseMap<Constant *, unsigned> TypeTableIDMapType;
  TypeTableIDMapType TypeTableIDMap;

  // Data for populating __pnacl_eh_action_table[], which is an array
  // of pairs.
  SmallVector<Constant *, 10> ActionTableData;
  // Pair of (clause_id, clause_list_id).
  typedef std::pair<unsigned, unsigned> ActionTableEntry;
  // Mapping from (clause_id, clause_list_id) to clause_id (index in
  // ActionTableData).
  typedef DenseMap<ActionTableEntry, unsigned> ActionTableIDMapType;
  ActionTableIDMapType ActionTableIDMap;

  // Data for populating __pnacl_eh_filter_table[], which is an array
  // of integers.
  SmallVector<Constant *, 10> FilterTableData;

  // Get the interned ID for an action.
  unsigned getIDForClauseListNode(unsigned ClauseID, unsigned NextClauseListID);

  // Get the clause ID for a "filter" clause.
  unsigned getIDForFilterClause(Value *Filter);

public:
  explicit ExceptionInfoWriter(LLVMContext *Context);

  // Get the interned type ID (a small integer) for a C++ exception type.
  unsigned getIDForExceptionType(Value *Ty);

  // Get the clause list ID for a landingpad's clause list.
  unsigned getIDForLandingPadClauseList(LandingPadInst *LP);

  // Add the exception info tables to the module.
  void defineGlobalVariables(Module *M);
};

}

#endif
