//===- ExceptionInfoWriter.cpp - Generate C++ exception info for PNaCl-----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// The ExceptionInfoWriter class converts the clauses of a
// "landingpad" instruction into data tables stored in global
// variables.  These tables are interpreted by PNaCl's C++ runtime
// library (either libsupc++ or libcxxabi), which is linked into a
// pexe.
//
// This is similar to the lowering that the LLVM backend does to
// convert landingpad clauses into ".gcc_except_table" sections.  The
// difference is that ExceptionInfoWriter is an IR-to-IR
// transformation that runs on the PNaCl user toolchain side.  The
// format it produces is not part of PNaCl's stable ABI; the PNaCl
// translator and LLVM backend do not know about this format.
//
// Encoding:
//
// A landingpad instruction contains a list of clauses.
// ExceptionInfoWriter encodes each clause as a 32-bit "clause ID".  A
// clause is one of the following forms:
//
//  1) "catch i8* @ExcType"
//     * This clause means that the landingpad should be entered if
//       the C++ exception being thrown has type @ExcType (or a
//       subtype of @ExcType).  @ExcType is a pointer to the
//       std::type_info object (an RTTI object) for the C++ exception
//       type.
//     * Clang generates this for a "catch" block in the C++ source.
//     * @ExcType is NULL for "catch (...)" (catch-all) blocks.
//     * This is encoded as the "type ID" for @ExcType, defined below,
//       which is a positive integer.
//
//  2) "filter [i8* @ExcType1, ..., i8* @ExcTypeN]"
//     * This clause means that the landingpad should be entered if
//       the C++ exception being thrown *doesn't* match any of the
//       types in the list (which are again specified as
//       std::type_info pointers).
//     * Clang uses this to implement C++ exception specifications, e.g.
//          void foo() throw(ExcType1, ..., ExcTypeN) { ... }
//     * This is encoded as the filter ID, X, where X < 0, and
//       &__pnacl_eh_filter_table[-X-1] points to a 0-terminated
//       array of integer "type IDs".
//
//  3) "cleanup"
//     * This means that the landingpad should always be entered.
//     * Clang uses this for calling objects' destructors.
//     * This is encoded as 0.
//     * The runtime may treat "cleanup" differently from "catch i8*
//       null" (a catch-all).  In C++, if an unhandled exception
//       occurs, the language runtime may abort execution without
//       running any destructors.  The runtime may implement this by
//       searching for a matching non-"cleanup" clause, and aborting
//       if it does not find one, before entering any landingpad
//       blocks.
//
// The "type ID" for a type @ExcType is a 1-based index into the array
// __pnacl_eh_type_table[].  That is, the type ID is a value X such
// that __pnacl_eh_type_table[X-1] == @ExcType, and X >= 1.
//
// ExceptionInfoWriter generates the following data structures:
//
//   struct action_table_entry {
//     int32_t clause_id;
//     uint32_t next_clause_list_id;
//   };
//
//   // Represents singly linked lists of clauses.
//   extern const struct action_table_entry __pnacl_eh_action_table[];
//
//   // Allows std::type_infos to be represented using small integer IDs.
//   extern std::type_info *const __pnacl_eh_type_table[];
//
//   // Used to represent type arrays for "filter" clauses.
//   extern const uint32_t __pnacl_eh_filter_table[];
//
// A "clause list ID" is either:
//  * 0, representing the empty list; or
//  * an index into __pnacl_eh_action_table[] with 1 added, which
//    specifies a node in the clause list.
//
// Example:
//
//   std::type_info *const __pnacl_eh_type_table[] = {
//     // defines type ID 1 == ExcA and clause ID 1 == "catch ExcA"
//     &typeinfo(ExcA),
//     // defines type ID 2 == ExcB and clause ID 2 == "catch ExcB"
//     &typeinfo(ExcB),
//     // defines type ID 3 == ExcC and clause ID 3 == "catch ExcC"
//     &typeinfo(ExcC),
//   };
//
//   const uint32_t __pnacl_eh_filter_table[] = {
//     1,  // refers to ExcA;  defines clause ID -1 as "filter [ExcA, ExcB]"
//     2,  // refers to ExcB;  defines clause ID -2 as "filter [ExcB]"
//     0,  // list terminator; defines clause ID -3 as "filter []"
//     3,  // refers to ExcC;  defines clause ID -4 as "filter [ExcC]"
//     0,  // list terminator; defines clause ID -5 as "filter []"
//   };
//
//   const struct action_table_entry __pnacl_eh_action_table[] = {
//     // defines clause list ID 1:
//     {
//       -4,  // "filter [ExcC]"
//       0,  // end of list (no more actions)
//     },
//     // defines clause list ID 2:
//     {
//       -1,  // "filter [ExcA, ExcB]"
//       1,  // else go to clause list ID 1
//     },
//     // defines clause list ID 3:
//     {
//       2,  // "catch ExcB"
//       2,  // else go to clause list ID 2
//     },
//     // defines clause list ID 4:
//     {
//       1,  // "catch ExcA"
//       3,  // else go to clause list ID 3
//     },
//   };
//
// So if a landingpad contains the clause list:
//   [catch ExcA,
//    catch ExcB,
//    filter [ExcA, ExcB],
//    filter [ExcC]]
// then this can be represented as clause list ID 4 using the tables above.
//
// The C++ runtime library checks the clauses in order to decide
// whether to enter the landingpad.  If a clause matches, the
// landingpad BasicBlock is passed the clause ID.  The landingpad code
// can use the clause ID to decide which C++ catch() block (if any) to
// execute.
//
// The purpose of these exception tables is to keep code sizes
// relatively small.  The landingpad code only needs to check a small
// integer clause ID, rather than having to call a function to check
// whether the C++ exception matches a type.
//
// ExceptionInfoWriter's encoding corresponds loosely to the format of
// GCC's .gcc_except_table sections.  One difference is that
// ExceptionInfoWriter writes fixed-width 32-bit integers, whereas
// .gcc_except_table uses variable-length LEB128 encodings.  We could
// switch to LEB128 to save space in the future.
//
//===----------------------------------------------------------------------===//

#include "ExceptionInfoWriter.h"
#include "llvm/IR/Constants.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

ExceptionInfoWriter::ExceptionInfoWriter(LLVMContext *Context):
    Context(Context) {
  Type *I32 = Type::getInt32Ty(*Context);
  Type *Fields[] = { I32, I32 };
  ActionTableEntryTy = StructType::create(Fields, "action_table_entry");
}

unsigned ExceptionInfoWriter::getIDForExceptionType(Value *ExcTy) {
  Constant *ExcTyConst = dyn_cast<Constant>(ExcTy);
  if (!ExcTyConst)
    report_fatal_error("Exception type not a constant");

  // Reuse existing ID if one has already been assigned.
  TypeTableIDMapType::iterator Iter = TypeTableIDMap.find(ExcTyConst);
  if (Iter != TypeTableIDMap.end())
    return Iter->second;

  unsigned Index = TypeTableData.size() + 1;
  TypeTableIDMap[ExcTyConst] = Index;
  TypeTableData.push_back(ExcTyConst);
  return Index;
}

unsigned ExceptionInfoWriter::getIDForClauseListNode(
    unsigned ClauseID, unsigned NextClauseListID) {
  // Reuse existing ID if one has already been assigned.
  ActionTableEntry Key(ClauseID, NextClauseListID);
  ActionTableIDMapType::iterator Iter = ActionTableIDMap.find(Key);
  if (Iter != ActionTableIDMap.end())
    return Iter->second;

  Type *I32 = Type::getInt32Ty(*Context);
  Constant *Fields[] = { ConstantInt::get(I32, ClauseID),
                         ConstantInt::get(I32, NextClauseListID) };
  Constant *Entry = ConstantStruct::get(ActionTableEntryTy, Fields);

  // Add 1 so that the empty list can be represented as 0.
  unsigned ClauseListID = ActionTableData.size() + 1;
  ActionTableIDMap[Key] = ClauseListID;
  ActionTableData.push_back(Entry);
  return ClauseListID;
}

unsigned ExceptionInfoWriter::getIDForFilterClause(Value *Filter) {
  unsigned FilterClauseID = -(FilterTableData.size() + 1);
  Type *I32 = Type::getInt32Ty(*Context);
  ArrayType *ArrayTy = dyn_cast<ArrayType>(Filter->getType());
  if (!ArrayTy)
    report_fatal_error("Landingpad filter clause is not of array type");
  unsigned FilterLength = ArrayTy->getNumElements();
  // Don't try the dyn_cast if the FilterLength is zero, because Array
  // could be a zeroinitializer.
  if (FilterLength > 0) {
    ConstantArray *Array = dyn_cast<ConstantArray>(Filter);
    if (!Array)
      report_fatal_error("Landingpad filter clause is not a ConstantArray");
    for (unsigned I = 0; I < FilterLength; ++I) {
      unsigned TypeID = getIDForExceptionType(Array->getOperand(I));
      assert(TypeID > 0);
      FilterTableData.push_back(ConstantInt::get(I32, TypeID));
    }
  }
  // Add array terminator.
  FilterTableData.push_back(ConstantInt::get(I32, 0));
  return FilterClauseID;
}

unsigned ExceptionInfoWriter::getIDForLandingPadClauseList(LandingPadInst *LP) {
  unsigned NextClauseListID = 0;  // ID for empty list.

  if (LP->isCleanup()) {
    // Add cleanup clause at the end of the list.
    NextClauseListID = getIDForClauseListNode(0, NextClauseListID);
  }

  for (int I = (int) LP->getNumClauses() - 1; I >= 0; --I) {
    unsigned ClauseID;
    if (LP->isCatch(I)) {
      ClauseID = getIDForExceptionType(LP->getClause(I));
    } else if (LP->isFilter(I)) {
      ClauseID = getIDForFilterClause(LP->getClause(I));
    } else {
      report_fatal_error("Unknown kind of landingpad clause");
    }
    assert(ClauseID > 0);
    NextClauseListID = getIDForClauseListNode(ClauseID, NextClauseListID);
  }

  return NextClauseListID;
}

static void defineArray(Module *M, const char *Name,
                        const SmallVectorImpl<Constant *> &Elements,
                        Type *ElementType) {
  ArrayType *ArrayTy = ArrayType::get(ElementType, Elements.size());
  Constant *ArrayData = ConstantArray::get(ArrayTy, Elements);
  GlobalVariable *OldGlobal = M->getGlobalVariable(Name);
  if (OldGlobal) {
    if (OldGlobal->hasInitializer()) {
      report_fatal_error(std::string("Variable ") + Name +
                         " already has an initializer");
    }
    Constant *NewGlobal = new GlobalVariable(
        *M, ArrayTy, /* isConstant= */ true,
        GlobalValue::InternalLinkage, ArrayData);
    NewGlobal->takeName(OldGlobal);
    OldGlobal->replaceAllUsesWith(ConstantExpr::getBitCast(
                                      NewGlobal, OldGlobal->getType()));
    OldGlobal->eraseFromParent();
  } else {
    if (Elements.size() > 0) {
      // This warning could happen for a program that does not link
      // against the C++ runtime libraries.  Such a program might
      // contain "invoke" instructions but never throw any C++
      // exceptions.
      errs() << "Warning: Variable " << Name << " not referenced\n";
    }
  }
}

void ExceptionInfoWriter::defineGlobalVariables(Module *M) {
  defineArray(M, "__pnacl_eh_type_table", TypeTableData,
              Type::getInt8PtrTy(M->getContext()));

  defineArray(M, "__pnacl_eh_action_table", ActionTableData,
              ActionTableEntryTy);

  defineArray(M, "__pnacl_eh_filter_table", FilterTableData,
              Type::getInt32Ty(M->getContext()));
}
