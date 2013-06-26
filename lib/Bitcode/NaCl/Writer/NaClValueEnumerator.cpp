//===-- NaClValueEnumerator.cpp ------------------------------------------===//
//     Number values and types for bitcode writer
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the NaClValueEnumerator class.
//
//===----------------------------------------------------------------------===//

#include "NaClValueEnumerator.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <set>

using namespace llvm;

static bool isIntOrIntVectorValue(const std::pair<const Value*, unsigned> &V) {
  return V.first->getType()->isIntOrIntVectorTy();
}

/// NaClValueEnumerator - Enumerate module-level information.
NaClValueEnumerator::NaClValueEnumerator(const Module *M) {
  // Create map for counting frequency of types, and set field
  // TypeCountMap accordingly.  Note: Pointer field TypeCountMap is
  // used to deal with the fact that types are added through various
  // method calls in this routine. Rather than pass it as an argument,
  // we use a field. The field is a pointer so that the memory
  // footprint of count_map can be garbage collected when this
  // constructor completes.
  TypeCountMapType count_map;
  TypeCountMap = &count_map;
  // Enumerate the global variables.
  for (Module::const_global_iterator I = M->global_begin(),
         E = M->global_end(); I != E; ++I)
    EnumerateValue(I);

  // Enumerate the functions.
  for (Module::const_iterator I = M->begin(), E = M->end(); I != E; ++I) {
    EnumerateValue(I);
  }

  // Enumerate the aliases.
  for (Module::const_alias_iterator I = M->alias_begin(), E = M->alias_end();
       I != E; ++I)
    EnumerateValue(I);

  // Remember what is the cutoff between globalvalue's and other constants.
  unsigned FirstConstant = Values.size();

  // Enumerate the global variable initializers.
  for (Module::const_global_iterator I = M->global_begin(),
         E = M->global_end(); I != E; ++I)
    if (I->hasInitializer())
      EnumerateValue(I->getInitializer());

  // Enumerate the aliasees.
  for (Module::const_alias_iterator I = M->alias_begin(), E = M->alias_end();
       I != E; ++I)
    EnumerateValue(I->getAliasee());

  // Insert constants that are named at module level into the slot
  // pool so that the module symbol table can refer to them...
  EnumerateValueSymbolTable(M->getValueSymbolTable());

  // Enumerate types used by function bodies and argument lists.
  for (Module::const_iterator F = M->begin(), E = M->end(); F != E; ++F) {

    for (Function::const_arg_iterator I = F->arg_begin(), E = F->arg_end();
         I != E; ++I)
      EnumerateType(I->getType());

    for (Function::const_iterator BB = F->begin(), E = F->end(); BB != E; ++BB)
      for (BasicBlock::const_iterator I = BB->begin(), E = BB->end(); I!=E;++I){
        for (User::const_op_iterator OI = I->op_begin(), E = I->op_end();
             OI != E; ++OI) {
          EnumerateOperandType(*OI);
        }
        EnumerateType(I->getType());
      }
  }

  // Optimized type indicies to put "common" expected types in with small
  // indices.
  OptimizeTypes(M);
  TypeCountMap = NULL;

  // Optimize constant ordering.
  OptimizeConstants(FirstConstant, Values.size());
}

void NaClValueEnumerator::OptimizeTypes(const Module *M) {

  // Sort types by count, so that we can index them based on
  // frequency. Use indices of built TypeMap, so that order of
  // construction is repeatable.
  std::set<unsigned> type_counts;
  typedef std::set<unsigned> TypeSetType;
  std::map<unsigned, TypeSetType> usage_count_map;
  TypeList IdType(Types);

  for (TypeCountMapType::iterator iter = TypeCountMap->begin();
       iter != TypeCountMap->end(); ++ iter) {
    type_counts.insert(iter->second);
    usage_count_map[iter->second].insert(TypeMap[iter->first]-1);
  }

  // Reset type tracking maps, so that we can re-enter based
  // on fequency ordering.
  TypeCountMap = NULL;
  Types.clear();
  TypeMap.clear();

  // Reinsert types, based on frequency.
  for (std::set<unsigned>::reverse_iterator count_iter = type_counts.rbegin();
       count_iter != type_counts.rend(); ++count_iter) {
    TypeSetType& count_types = usage_count_map[*count_iter];
    for (TypeSetType::iterator type_iter = count_types.begin();
         type_iter != count_types.end(); ++type_iter)
      EnumerateType((IdType[*type_iter]), true);
  }
}

unsigned NaClValueEnumerator::getInstructionID(const Instruction *Inst) const {
  InstructionMapType::const_iterator I = InstructionMap.find(Inst);
  assert(I != InstructionMap.end() && "Instruction is not mapped!");
  return I->second;
}

void NaClValueEnumerator::setInstructionID(const Instruction *I) {
  InstructionMap[I] = InstructionCount++;
}

unsigned NaClValueEnumerator::getValueID(const Value *V) const {
  ValueMapType::const_iterator I = ValueMap.find(V);
  assert(I != ValueMap.end() && "Value not in slotcalculator!");
  return I->second-1;
}

void NaClValueEnumerator::dump() const {
  print(dbgs(), ValueMap, "Default");
  dbgs() << '\n';
}

void NaClValueEnumerator::print(raw_ostream &OS, const ValueMapType &Map,
                            const char *Name) const {

  OS << "Map Name: " << Name << "\n";
  OS << "Size: " << Map.size() << "\n";
  for (ValueMapType::const_iterator I = Map.begin(),
         E = Map.end(); I != E; ++I) {

    const Value *V = I->first;
    if (V->hasName())
      OS << "Value: " << V->getName();
    else
      OS << "Value: [null]\n";
    V->dump();

    OS << " Uses(" << std::distance(V->use_begin(),V->use_end()) << "):";
    for (Value::const_use_iterator UI = V->use_begin(), UE = V->use_end();
         UI != UE; ++UI) {
      if (UI != V->use_begin())
        OS << ",";
      if((*UI)->hasName())
        OS << " " << (*UI)->getName();
      else
        OS << " [null]";

    }
    OS <<  "\n\n";
  }
}

// Optimize constant ordering.
namespace {
  struct CstSortPredicate {
    NaClValueEnumerator &VE;
    explicit CstSortPredicate(NaClValueEnumerator &ve) : VE(ve) {}
    bool operator()(const std::pair<const Value*, unsigned> &LHS,
                    const std::pair<const Value*, unsigned> &RHS) {
      // Sort by plane.
      if (LHS.first->getType() != RHS.first->getType())
        return VE.getTypeID(LHS.first->getType()) <
               VE.getTypeID(RHS.first->getType());
      // Then by frequency.
      return LHS.second > RHS.second;
    }
  };
}

/// OptimizeConstants - Reorder constant pool for denser encoding.
void NaClValueEnumerator::OptimizeConstants(unsigned CstStart, unsigned CstEnd) {
  if (CstStart == CstEnd || CstStart+1 == CstEnd) return;

  CstSortPredicate P(*this);
  std::stable_sort(Values.begin()+CstStart, Values.begin()+CstEnd, P);

  // Ensure that integer and vector of integer constants are at the start of the
  // constant pool.  This is important so that GEP structure indices come before
  // gep constant exprs.
  std::partition(Values.begin()+CstStart, Values.begin()+CstEnd,
                 isIntOrIntVectorValue);

  // Rebuild the modified portion of ValueMap.
  for (; CstStart != CstEnd; ++CstStart)
    ValueMap[Values[CstStart].first] = CstStart+1;
}


/// EnumerateValueSymbolTable - Insert all of the values in the specified symbol
/// table into the values table.
void NaClValueEnumerator::EnumerateValueSymbolTable(const ValueSymbolTable &VST) {
  for (ValueSymbolTable::const_iterator VI = VST.begin(), VE = VST.end();
       VI != VE; ++VI)
    EnumerateValue(VI->getValue());
}

void NaClValueEnumerator::EnumerateValue(const Value *V) {
  assert(!V->getType()->isVoidTy() && "Can't insert void values!");
  assert(!isa<MDNode>(V) && !isa<MDString>(V) &&
         "EnumerateValue doesn't handle Metadata!");

  // Check to see if it's already in!
  unsigned &ValueID = ValueMap[V];
  if (ValueID) {
    // Increment use count.
    Values[ValueID-1].second++;
    return;
  }

  // Enumerate the type of this value.
  EnumerateType(V->getType());

  if (const Constant *C = dyn_cast<Constant>(V)) {
    if (isa<GlobalValue>(C)) {
      // Initializers for globals are handled explicitly elsewhere.
    } else if (C->getNumOperands()) {
      // If a constant has operands, enumerate them.  This makes sure that if a
      // constant has uses (for example an array of const ints), that they are
      // inserted also.

      // We prefer to enumerate them with values before we enumerate the user
      // itself.  This makes it more likely that we can avoid forward references
      // in the reader.  We know that there can be no cycles in the constants
      // graph that don't go through a global variable.
      for (User::const_op_iterator I = C->op_begin(), E = C->op_end();
           I != E; ++I)
        if (!isa<BasicBlock>(*I)) // Don't enumerate BB operand to BlockAddress.
          EnumerateValue(*I);

      // Finally, add the value.  Doing this could make the ValueID reference be
      // dangling, don't reuse it.
      Values.push_back(std::make_pair(V, 1U));
      ValueMap[V] = Values.size();
      return;
    }
  }

  // Add the value.
  Values.push_back(std::make_pair(V, 1U));
  ValueID = Values.size();
}


void NaClValueEnumerator::EnumerateType(Type *Ty, bool InsideOptimizeTypes) {
  // This function is used to enumerate types referenced by the given
  // module. This function is called in two phases, based on the value
  // of TypeCountMap. These phases are:
  //
  // (1) In this phase, InsideOptimizeTypes=false. We are collecting types
  // and all corresponding (implicitly) referenced types. In addition,
  // we are keeping track of the number of references to each type in
  // TypeCountMap. These reference counts will be used by method
  // OptimizeTypes to associate the smallest type ID's with the most
  // referenced types.
  //
  // (2) In this phase, InsideOptimizeTypes=true. We are registering types
  // based on frequency. To minimize type IDs for frequently used
  // types, (unlike the other context) we are inserting the minimal
  // (implicitly) referenced types needed for each type.
  unsigned *TypeID = &TypeMap[Ty];

  if (TypeCountMap) ++((*TypeCountMap)[Ty]);

  // We've already seen this type.
  if (*TypeID)
    return;

  // If it is a non-anonymous struct, mark the type as being visited so that we
  // don't recursively visit it.  This is safe because we allow forward
  // references of these in the bitcode reader.
  if (StructType *STy = dyn_cast<StructType>(Ty))
    if (!STy->isLiteral())
      *TypeID = ~0U;

  // If in the second phase (i.e. inside optimize types), don't expand
  // pointers to structures, since we can just generate a forward
  // reference to it. This way, we don't use up unnecessary (small) ID
  // values just to define the pointer.
  bool EnumerateSubtypes = true;
  if (InsideOptimizeTypes)
    if (PointerType *PTy = dyn_cast<PointerType>(Ty))
      if (StructType *STy = dyn_cast<StructType>(PTy->getElementType()))
        if (!STy->isLiteral())
          EnumerateSubtypes = false;

  // Enumerate all of the subtypes before we enumerate this type.  This ensures
  // that the type will be enumerated in an order that can be directly built.
  if (EnumerateSubtypes) {
    for (Type::subtype_iterator I = Ty->subtype_begin(), E = Ty->subtype_end();
         I != E; ++I)
      EnumerateType(*I, InsideOptimizeTypes);
  }

  // Refresh the TypeID pointer in case the table rehashed.
  TypeID = &TypeMap[Ty];

  // Check to see if we got the pointer another way.  This can happen when
  // enumerating recursive types that hit the base case deeper than they start.
  //
  // If this is actually a struct that we are treating as forward ref'able,
  // then emit the definition now that all of its contents are available.
  if (*TypeID && *TypeID != ~0U)
    return;

  // Add this type now that its contents are all happily enumerated.
  Types.push_back(Ty);

  *TypeID = Types.size();
}

// Enumerate the types for the specified value.  If the value is a constant,
// walk through it, enumerating the types of the constant.
void NaClValueEnumerator::EnumerateOperandType(const Value *V) {
  EnumerateType(V->getType());

  if (const Constant *C = dyn_cast<Constant>(V)) {
    // If this constant is already enumerated, ignore it, we know its type must
    // be enumerated.
    if (ValueMap.count(V)) return;

    // This constant may have operands, make sure to enumerate the types in
    // them.
    for (unsigned i = 0, e = C->getNumOperands(); i != e; ++i) {
      const Value *Op = C->getOperand(i);

      // Don't enumerate basic blocks here, this happens as operands to
      // blockaddress.
      if (isa<BasicBlock>(Op)) continue;

      EnumerateOperandType(Op);
    }
  }
}

void NaClValueEnumerator::incorporateFunction(const Function &F) {
  InstructionCount = 0;
  NumModuleValues = Values.size();

  // Make sure no insertions outside of a function.
  assert(FnForwardTypeRefs.empty());

  // Adding function arguments to the value table.
  for (Function::const_arg_iterator I = F.arg_begin(), E = F.arg_end();
       I != E; ++I)
    EnumerateValue(I);

  FirstFuncConstantID = Values.size();

  // Add all function-level constants to the value table.
  for (Function::const_iterator BB = F.begin(), E = F.end(); BB != E; ++BB) {
    for (BasicBlock::const_iterator I = BB->begin(), E = BB->end(); I!=E; ++I)
      for (User::const_op_iterator OI = I->op_begin(), E = I->op_end();
           OI != E; ++OI) {
        if ((isa<Constant>(*OI) && !isa<GlobalValue>(*OI)) ||
            isa<InlineAsm>(*OI))
          EnumerateValue(*OI);
      }
    BasicBlocks.push_back(BB);
    ValueMap[BB] = BasicBlocks.size();
  }

  // Optimize the constant layout.
  OptimizeConstants(FirstFuncConstantID, Values.size());

  FirstInstID = Values.size();

  // Add all of the instructions.
  for (Function::const_iterator BB = F.begin(), E = F.end(); BB != E; ++BB) {
    for (BasicBlock::const_iterator I = BB->begin(), E = BB->end(); I!=E; ++I) {
      if (!I->getType()->isVoidTy())
        EnumerateValue(I);
    }
  }
}

void NaClValueEnumerator::purgeFunction() {
  /// Remove purged values from the ValueMap.
  for (unsigned i = NumModuleValues, e = Values.size(); i != e; ++i)
    ValueMap.erase(Values[i].first);
  for (unsigned i = 0, e = BasicBlocks.size(); i != e; ++i)
    ValueMap.erase(BasicBlocks[i]);

  Values.resize(NumModuleValues);
  BasicBlocks.clear();
  FnForwardTypeRefs.clear();
}

static void IncorporateFunctionInfoGlobalBBIDs(const Function *F,
                                 DenseMap<const BasicBlock*, unsigned> &IDMap) {
  unsigned Counter = 0;
  for (Function::const_iterator BB = F->begin(), E = F->end(); BB != E; ++BB)
    IDMap[BB] = ++Counter;
}

/// getGlobalBasicBlockID - This returns the function-specific ID for the
/// specified basic block.  This is relatively expensive information, so it
/// should only be used by rare constructs such as address-of-label.
unsigned NaClValueEnumerator::getGlobalBasicBlockID(const BasicBlock *BB) const {
  unsigned &Idx = GlobalBasicBlockIDs[BB];
  if (Idx != 0)
    return Idx-1;

  IncorporateFunctionInfoGlobalBBIDs(BB->getParent(), GlobalBasicBlockIDs);
  return getGlobalBasicBlockID(BB);
}
