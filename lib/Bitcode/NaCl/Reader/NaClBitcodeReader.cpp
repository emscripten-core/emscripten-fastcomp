//===- NaClBitcodeReader.cpp ----------------------------------------------===//
//     Internal NaClBitcodeReader implementation
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "NaClBitcodeReader"

#include "llvm/Bitcode/NaCl/NaClReaderWriter.h"
#include "NaClBitcodeReader.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/AutoUpgrade.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/OperandTraits.h"
#include "llvm/IR/Operator.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/DataStream.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/MemoryBuffer.h"
using namespace llvm;

enum {
  SWITCH_INST_MAGIC = 0x4B5 // May 2012 => 1205 => Hex
};

void NaClBitcodeReader::materializeForwardReferencedFunctions() {
  while (!BlockAddrFwdRefs.empty()) {
    Function *F = BlockAddrFwdRefs.begin()->first;
    F->Materialize();
  }
}

void NaClBitcodeReader::FreeState() {
  if (BufferOwned)
    delete Buffer;
  Buffer = 0;
  std::vector<Type*>().swap(TypeList);
  ValueList.clear();

  std::vector<BasicBlock*>().swap(FunctionBBs);
  std::vector<Function*>().swap(FunctionsWithBodies);
  DeferredFunctionInfo.clear();

  assert(BlockAddrFwdRefs.empty() && "Unresolved blockaddress fwd references");
}

//===----------------------------------------------------------------------===//
//  Helper functions to implement forward reference resolution, etc.
//===----------------------------------------------------------------------===//

/// ConvertToString - Convert a string from a record into an std::string, return
/// true on failure.
template<typename StrTy>
static bool ConvertToString(ArrayRef<uint64_t> Record, unsigned Idx,
                            StrTy &Result) {
  if (Idx > Record.size())
    return true;

  for (unsigned i = Idx, e = Record.size(); i != e; ++i)
    Result += (char)Record[i];
  return false;
}

static GlobalValue::LinkageTypes GetDecodedLinkage(unsigned Val) {
  switch (Val) {
  default: // Map unknown/new linkages to external
  case 0:  return GlobalValue::ExternalLinkage;
  case 1:  return GlobalValue::WeakAnyLinkage;
  case 2:  return GlobalValue::AppendingLinkage;
  case 3:  return GlobalValue::InternalLinkage;
  case 4:  return GlobalValue::LinkOnceAnyLinkage;
  case 5:  return GlobalValue::DLLImportLinkage;
  case 6:  return GlobalValue::DLLExportLinkage;
  case 7:  return GlobalValue::ExternalWeakLinkage;
  case 8:  return GlobalValue::CommonLinkage;
  case 9:  return GlobalValue::PrivateLinkage;
  case 10: return GlobalValue::WeakODRLinkage;
  case 11: return GlobalValue::LinkOnceODRLinkage;
  case 12: return GlobalValue::AvailableExternallyLinkage;
  case 13: return GlobalValue::LinkerPrivateLinkage;
  case 14: return GlobalValue::LinkerPrivateWeakLinkage;
  case 15: return GlobalValue::LinkOnceODRAutoHideLinkage;
  }
}

static GlobalValue::VisibilityTypes GetDecodedVisibility(unsigned Val) {
  switch (Val) {
  default: // Map unknown visibilities to default.
  case 0: return GlobalValue::DefaultVisibility;
  case 1: return GlobalValue::HiddenVisibility;
  case 2: return GlobalValue::ProtectedVisibility;
  }
}

static GlobalVariable::ThreadLocalMode GetDecodedThreadLocalMode(unsigned Val) {
  switch (Val) {
    case 0: return GlobalVariable::NotThreadLocal;
    default: // Map unknown non-zero value to general dynamic.
    case 1: return GlobalVariable::GeneralDynamicTLSModel;
    case 2: return GlobalVariable::LocalDynamicTLSModel;
    case 3: return GlobalVariable::InitialExecTLSModel;
    case 4: return GlobalVariable::LocalExecTLSModel;
  }
}

static int GetDecodedCastOpcode(unsigned Val) {
  switch (Val) {
  default: return -1;
  case naclbitc::CAST_TRUNC   : return Instruction::Trunc;
  case naclbitc::CAST_ZEXT    : return Instruction::ZExt;
  case naclbitc::CAST_SEXT    : return Instruction::SExt;
  case naclbitc::CAST_FPTOUI  : return Instruction::FPToUI;
  case naclbitc::CAST_FPTOSI  : return Instruction::FPToSI;
  case naclbitc::CAST_UITOFP  : return Instruction::UIToFP;
  case naclbitc::CAST_SITOFP  : return Instruction::SIToFP;
  case naclbitc::CAST_FPTRUNC : return Instruction::FPTrunc;
  case naclbitc::CAST_FPEXT   : return Instruction::FPExt;
  case naclbitc::CAST_PTRTOINT: return Instruction::PtrToInt;
  case naclbitc::CAST_INTTOPTR: return Instruction::IntToPtr;
  case naclbitc::CAST_BITCAST : return Instruction::BitCast;
  }
}
static int GetDecodedBinaryOpcode(unsigned Val, Type *Ty) {
  switch (Val) {
  default: return -1;
  case naclbitc::BINOP_ADD:
    return Ty->isFPOrFPVectorTy() ? Instruction::FAdd : Instruction::Add;
  case naclbitc::BINOP_SUB:
    return Ty->isFPOrFPVectorTy() ? Instruction::FSub : Instruction::Sub;
  case naclbitc::BINOP_MUL:
    return Ty->isFPOrFPVectorTy() ? Instruction::FMul : Instruction::Mul;
  case naclbitc::BINOP_UDIV: return Instruction::UDiv;
  case naclbitc::BINOP_SDIV:
    return Ty->isFPOrFPVectorTy() ? Instruction::FDiv : Instruction::SDiv;
  case naclbitc::BINOP_UREM: return Instruction::URem;
  case naclbitc::BINOP_SREM:
    return Ty->isFPOrFPVectorTy() ? Instruction::FRem : Instruction::SRem;
  case naclbitc::BINOP_SHL:  return Instruction::Shl;
  case naclbitc::BINOP_LSHR: return Instruction::LShr;
  case naclbitc::BINOP_ASHR: return Instruction::AShr;
  case naclbitc::BINOP_AND:  return Instruction::And;
  case naclbitc::BINOP_OR:   return Instruction::Or;
  case naclbitc::BINOP_XOR:  return Instruction::Xor;
  }
}

static AtomicRMWInst::BinOp GetDecodedRMWOperation(unsigned Val) {
  switch (Val) {
  default: return AtomicRMWInst::BAD_BINOP;
  case naclbitc::RMW_XCHG: return AtomicRMWInst::Xchg;
  case naclbitc::RMW_ADD: return AtomicRMWInst::Add;
  case naclbitc::RMW_SUB: return AtomicRMWInst::Sub;
  case naclbitc::RMW_AND: return AtomicRMWInst::And;
  case naclbitc::RMW_NAND: return AtomicRMWInst::Nand;
  case naclbitc::RMW_OR: return AtomicRMWInst::Or;
  case naclbitc::RMW_XOR: return AtomicRMWInst::Xor;
  case naclbitc::RMW_MAX: return AtomicRMWInst::Max;
  case naclbitc::RMW_MIN: return AtomicRMWInst::Min;
  case naclbitc::RMW_UMAX: return AtomicRMWInst::UMax;
  case naclbitc::RMW_UMIN: return AtomicRMWInst::UMin;
  }
}

static AtomicOrdering GetDecodedOrdering(unsigned Val) {
  switch (Val) {
  case naclbitc::ORDERING_NOTATOMIC: return NotAtomic;
  case naclbitc::ORDERING_UNORDERED: return Unordered;
  case naclbitc::ORDERING_MONOTONIC: return Monotonic;
  case naclbitc::ORDERING_ACQUIRE: return Acquire;
  case naclbitc::ORDERING_RELEASE: return Release;
  case naclbitc::ORDERING_ACQREL: return AcquireRelease;
  default: // Map unknown orderings to sequentially-consistent.
  case naclbitc::ORDERING_SEQCST: return SequentiallyConsistent;
  }
}

static SynchronizationScope GetDecodedSynchScope(unsigned Val) {
  switch (Val) {
  case naclbitc::SYNCHSCOPE_SINGLETHREAD: return SingleThread;
  default: // Map unknown scopes to cross-thread.
  case naclbitc::SYNCHSCOPE_CROSSTHREAD: return CrossThread;
  }
}

static CallingConv::ID GetDecodedCallingConv(unsigned Val) {
  switch (Val) {
  default:
    report_fatal_error("PNaCl bitcode contains invalid calling conventions.");
  case naclbitc::C_CallingConv: return CallingConv::C;
  }
}

namespace llvm {
namespace {
  /// @brief A class for maintaining the slot number definition
  /// as a placeholder for the actual definition for forward constants defs.
  class ConstantPlaceHolder : public ConstantExpr {
    void operator=(const ConstantPlaceHolder &) LLVM_DELETED_FUNCTION;
  public:
    // allocate space for exactly one operand
    void *operator new(size_t s) {
      return User::operator new(s, 1);
    }
    explicit ConstantPlaceHolder(Type *Ty, LLVMContext& Context)
      : ConstantExpr(Ty, Instruction::UserOp1, &Op<0>(), 1) {
      Op<0>() = UndefValue::get(Type::getInt32Ty(Context));
    }

    /// @brief Methods to support type inquiry through isa, cast, and dyn_cast.
    static bool classof(const Value *V) {
      return isa<ConstantExpr>(V) &&
             cast<ConstantExpr>(V)->getOpcode() == Instruction::UserOp1;
    }


    /// Provide fast operand accessors
    //DECLARE_TRANSPARENT_OPERAND_ACCESSORS(Value);
  };
}

// FIXME: can we inherit this from ConstantExpr?
template <>
struct OperandTraits<ConstantPlaceHolder> :
  public FixedNumOperandTraits<ConstantPlaceHolder, 1> {
};
}


void NaClBitcodeReaderValueList::AssignValue(Value *V, unsigned Idx) {
  if (Idx == size()) {
    push_back(V);
    return;
  }

  if (Idx >= size())
    resize(Idx+1);

  WeakVH &OldV = ValuePtrs[Idx];
  if (OldV == 0) {
    OldV = V;
    return;
  }

  // Handle constants and non-constants (e.g. instrs) differently for
  // efficiency.
  if (Constant *PHC = dyn_cast<Constant>(&*OldV)) {
    ResolveConstants.push_back(std::make_pair(PHC, Idx));
    OldV = V;
  } else {
    // If there was a forward reference to this value, replace it.
    Value *PrevVal = OldV;
    OldV->replaceAllUsesWith(V);
    delete PrevVal;
  }
}


Constant *NaClBitcodeReaderValueList::getConstantFwdRef(unsigned Idx,
                                                    Type *Ty) {
  if (Idx >= size())
    resize(Idx + 1);

  if (Value *V = ValuePtrs[Idx]) {
    assert(Ty == V->getType() && "Type mismatch in constant table!");
    return cast<Constant>(V);
  }

  // Create and return a placeholder, which will later be RAUW'd.
  Constant *C = new ConstantPlaceHolder(Ty, Context);
  ValuePtrs[Idx] = C;
  return C;
}

Value *NaClBitcodeReaderValueList::getValueFwdRef(unsigned Idx) {
  if (Idx >= size())
    return 0;

  if (Value *V = ValuePtrs[Idx])
    return V;

  return 0;
}

bool NaClBitcodeReaderValueList::createValueFwdRef(unsigned Idx, Type *Ty) {
  if (Idx >= size())
    resize(Idx + 1);

  // Return an error if this a duplicate definition of Idx.
  if (ValuePtrs[Idx])
    return true;

  // No type specified, must be invalid reference.
  if (Ty == 0)
    return true;

  // Create a placeholder, which will later be RAUW'd.
  ValuePtrs[Idx] = new Argument(Ty);
  return false;
}

/// ResolveConstantForwardRefs - Once all constants are read, this method bulk
/// resolves any forward references.  The idea behind this is that we sometimes
/// get constants (such as large arrays) which reference *many* forward ref
/// constants.  Replacing each of these causes a lot of thrashing when
/// building/reuniquing the constant.  Instead of doing this, we look at all the
/// uses and rewrite all the place holders at once for any constant that uses
/// a placeholder.
void NaClBitcodeReaderValueList::ResolveConstantForwardRefs() {
  // Sort the values by-pointer so that they are efficient to look up with a
  // binary search.
  std::sort(ResolveConstants.begin(), ResolveConstants.end());

  SmallVector<Constant*, 64> NewOps;

  while (!ResolveConstants.empty()) {
    Value *RealVal = operator[](ResolveConstants.back().second);
    Constant *Placeholder = ResolveConstants.back().first;
    ResolveConstants.pop_back();

    // Loop over all users of the placeholder, updating them to reference the
    // new value.  If they reference more than one placeholder, update them all
    // at once.
    while (!Placeholder->use_empty()) {
      Value::use_iterator UI = Placeholder->use_begin();
      User *U = *UI;

      // If the using object isn't uniqued, just update the operands.  This
      // handles instructions and initializers for global variables.
      if (!isa<Constant>(U) || isa<GlobalValue>(U)) {
        UI.getUse().set(RealVal);
        continue;
      }

      // Otherwise, we have a constant that uses the placeholder.  Replace that
      // constant with a new constant that has *all* placeholder uses updated.
      Constant *UserC = cast<Constant>(U);
      for (User::op_iterator I = UserC->op_begin(), E = UserC->op_end();
           I != E; ++I) {
        Value *NewOp;
        if (!isa<ConstantPlaceHolder>(*I)) {
          // Not a placeholder reference.
          NewOp = *I;
        } else if (*I == Placeholder) {
          // Common case is that it just references this one placeholder.
          NewOp = RealVal;
        } else {
          // Otherwise, look up the placeholder in ResolveConstants.
          ResolveConstantsTy::iterator It =
            std::lower_bound(ResolveConstants.begin(), ResolveConstants.end(),
                             std::pair<Constant*, unsigned>(cast<Constant>(*I),
                                                            0));
          assert(It != ResolveConstants.end() && It->first == *I);
          NewOp = operator[](It->second);
        }

        NewOps.push_back(cast<Constant>(NewOp));
      }

      // Make the new constant.
      Constant *NewC;
      if (ConstantArray *UserCA = dyn_cast<ConstantArray>(UserC)) {
        NewC = ConstantArray::get(UserCA->getType(), NewOps);
      } else if (ConstantStruct *UserCS = dyn_cast<ConstantStruct>(UserC)) {
        NewC = ConstantStruct::get(UserCS->getType(), NewOps);
      } else if (isa<ConstantVector>(UserC)) {
        NewC = ConstantVector::get(NewOps);
      } else {
        assert(isa<ConstantExpr>(UserC) && "Must be a ConstantExpr.");
        NewC = cast<ConstantExpr>(UserC)->getWithOperands(NewOps);
      }

      UserC->replaceAllUsesWith(NewC);
      UserC->destroyConstant();
      NewOps.clear();
    }

    // Update all ValueHandles, they should be the only users at this point.
    Placeholder->replaceAllUsesWith(RealVal);
    delete Placeholder;
  }
}

Type *NaClBitcodeReader::getTypeByID(unsigned ID) {
  // The type table size is always specified correctly.
  if (ID >= TypeList.size())
    return 0;

  if (Type *Ty = TypeList[ID])
    return Ty;

  // If we have a forward reference, the only possible case is when it is to a
  // named struct.  Just create a placeholder for now.
  return TypeList[ID] = StructType::create(Context);
}


//===----------------------------------------------------------------------===//
//  Functions for parsing blocks from the bitcode file
//===----------------------------------------------------------------------===//


bool NaClBitcodeReader::ParseTypeTable() {
  DEBUG(dbgs() << "-> ParseTypeTable\n");
  if (Stream.EnterSubBlock(naclbitc::TYPE_BLOCK_ID_NEW))
    return Error("Malformed block record");

  bool result = ParseTypeTableBody();
  if (!result)
    DEBUG(dbgs() << "<- ParseTypeTable\n");
  return result;
}

bool NaClBitcodeReader::ParseTypeTableBody() {
  if (!TypeList.empty())
    return Error("Multiple TYPE_BLOCKs found!");

  SmallVector<uint64_t, 64> Record;
  unsigned NumRecords = 0;

  SmallString<64> TypeName;

  // Read all the records for this type table.
  while (1) {
    NaClBitstreamEntry Entry = Stream.advanceSkippingSubblocks();

    switch (Entry.Kind) {
    case NaClBitstreamEntry::SubBlock: // Handled for us already.
    case NaClBitstreamEntry::Error:
      Error("Error in the type table block");
      return true;
    case NaClBitstreamEntry::EndBlock:
      if (NumRecords != TypeList.size())
        return Error("Invalid type forward reference in TYPE_BLOCK");
      return false;
    case NaClBitstreamEntry::Record:
      // The interesting case.
      break;
    }

    // Read a record.
    Record.clear();
    Type *ResultTy = 0;
    switch (Stream.readRecord(Entry.ID, Record)) {
    default: return Error("unknown type in type table");
    case naclbitc::TYPE_CODE_NUMENTRY: // TYPE_CODE_NUMENTRY: [numentries]
      // TYPE_CODE_NUMENTRY contains a count of the number of types in the
      // type list.  This allows us to reserve space.
      if (Record.size() < 1)
        return Error("Invalid TYPE_CODE_NUMENTRY record");
      TypeList.resize(Record[0]);
      continue;
    case naclbitc::TYPE_CODE_VOID:      // VOID
      ResultTy = Type::getVoidTy(Context);
      break;
    case naclbitc::TYPE_CODE_HALF:     // HALF
      ResultTy = Type::getHalfTy(Context);
      break;
    case naclbitc::TYPE_CODE_FLOAT:     // FLOAT
      ResultTy = Type::getFloatTy(Context);
      break;
    case naclbitc::TYPE_CODE_DOUBLE:    // DOUBLE
      ResultTy = Type::getDoubleTy(Context);
      break;
    case naclbitc::TYPE_CODE_X86_FP80:  // X86_FP80
      ResultTy = Type::getX86_FP80Ty(Context);
      break;
    case naclbitc::TYPE_CODE_FP128:     // FP128
      ResultTy = Type::getFP128Ty(Context);
      break;
    case naclbitc::TYPE_CODE_PPC_FP128: // PPC_FP128
      ResultTy = Type::getPPC_FP128Ty(Context);
      break;
    case naclbitc::TYPE_CODE_LABEL:     // LABEL
      ResultTy = Type::getLabelTy(Context);
      break;
    case naclbitc::TYPE_CODE_X86_MMX:   // X86_MMX
      ResultTy = Type::getX86_MMXTy(Context);
      break;
    case naclbitc::TYPE_CODE_INTEGER:   // INTEGER: [width]
      if (Record.size() < 1)
        return Error("Invalid Integer type record");

      ResultTy = IntegerType::get(Context, Record[0]);
      break;
    case naclbitc::TYPE_CODE_POINTER: { // POINTER: [pointee type] or
                                    //          [pointee type, address space]
      if (Record.size() < 1)
        return Error("Invalid POINTER type record");
      unsigned AddressSpace = 0;
      if (Record.size() == 2)
        AddressSpace = Record[1];
      ResultTy = getTypeByID(Record[0]);
      if (ResultTy == 0) return Error("invalid element type in pointer type");
      ResultTy = PointerType::get(ResultTy, AddressSpace);
      break;
    }
    case naclbitc::TYPE_CODE_FUNCTION_OLD: {
      // FIXME: attrid is dead, remove it in LLVM 4.0
      // FUNCTION: [vararg, attrid, retty, paramty x N]
      if (Record.size() < 3)
        return Error("Invalid FUNCTION type record");
      SmallVector<Type*, 8> ArgTys;
      for (unsigned i = 3, e = Record.size(); i != e; ++i) {
        if (Type *T = getTypeByID(Record[i]))
          ArgTys.push_back(T);
        else
          break;
      }

      ResultTy = getTypeByID(Record[2]);
      if (ResultTy == 0 || ArgTys.size() < Record.size()-3)
        return Error("invalid type in function type");

      ResultTy = FunctionType::get(ResultTy, ArgTys, Record[0]);
      break;
    }
    case naclbitc::TYPE_CODE_FUNCTION: {
      // FUNCTION: [vararg, retty, paramty x N]
      if (Record.size() < 2)
        return Error("Invalid FUNCTION type record");
      SmallVector<Type*, 8> ArgTys;
      for (unsigned i = 2, e = Record.size(); i != e; ++i) {
        if (Type *T = getTypeByID(Record[i]))
          ArgTys.push_back(T);
        else
          break;
      }

      ResultTy = getTypeByID(Record[1]);
      if (ResultTy == 0 || ArgTys.size() < Record.size()-2)
        return Error("invalid type in function type");

      ResultTy = FunctionType::get(ResultTy, ArgTys, Record[0]);
      break;
    }
    case naclbitc::TYPE_CODE_STRUCT_ANON: {  // STRUCT: [ispacked, eltty x N]
      if (Record.size() < 1)
        return Error("Invalid STRUCT type record");
      SmallVector<Type*, 8> EltTys;
      for (unsigned i = 1, e = Record.size(); i != e; ++i) {
        if (Type *T = getTypeByID(Record[i]))
          EltTys.push_back(T);
        else
          break;
      }
      if (EltTys.size() != Record.size()-1)
        return Error("invalid type in struct type");
      ResultTy = StructType::get(Context, EltTys, Record[0]);
      break;
    }
    case naclbitc::TYPE_CODE_STRUCT_NAME:   // STRUCT_NAME: [strchr x N]
      if (ConvertToString(Record, 0, TypeName))
        return Error("Invalid STRUCT_NAME record");
      continue;

    case naclbitc::TYPE_CODE_STRUCT_NAMED: { // STRUCT: [ispacked, eltty x N]
      if (Record.size() < 1)
        return Error("Invalid STRUCT type record");

      if (NumRecords >= TypeList.size())
        return Error("invalid TYPE table");

      // Check to see if this was forward referenced, if so fill in the temp.
      StructType *Res = cast_or_null<StructType>(TypeList[NumRecords]);
      if (Res) {
        Res->setName(TypeName);
        TypeList[NumRecords] = 0;
      } else  // Otherwise, create a new struct.
        Res = StructType::create(Context, TypeName);
      TypeName.clear();

      SmallVector<Type*, 8> EltTys;
      for (unsigned i = 1, e = Record.size(); i != e; ++i) {
        if (Type *T = getTypeByID(Record[i]))
          EltTys.push_back(T);
        else
          break;
      }
      if (EltTys.size() != Record.size()-1)
        return Error("invalid STRUCT type record");
      Res->setBody(EltTys, Record[0]);
      ResultTy = Res;
      break;
    }
    case naclbitc::TYPE_CODE_OPAQUE: {       // OPAQUE: []
      if (Record.size() != 1)
        return Error("Invalid OPAQUE type record");

      if (NumRecords >= TypeList.size())
        return Error("invalid TYPE table");

      // Check to see if this was forward referenced, if so fill in the temp.
      StructType *Res = cast_or_null<StructType>(TypeList[NumRecords]);
      if (Res) {
        Res->setName(TypeName);
        TypeList[NumRecords] = 0;
      } else  // Otherwise, create a new struct with no body.
        Res = StructType::create(Context, TypeName);
      TypeName.clear();
      ResultTy = Res;
      break;
    }
    case naclbitc::TYPE_CODE_ARRAY:     // ARRAY: [numelts, eltty]
      if (Record.size() < 2)
        return Error("Invalid ARRAY type record");
      if ((ResultTy = getTypeByID(Record[1])))
        ResultTy = ArrayType::get(ResultTy, Record[0]);
      else
        return Error("Invalid ARRAY type element");
      break;
    case naclbitc::TYPE_CODE_VECTOR:    // VECTOR: [numelts, eltty]
      if (Record.size() < 2)
        return Error("Invalid VECTOR type record");
      if ((ResultTy = getTypeByID(Record[1])))
        ResultTy = VectorType::get(ResultTy, Record[0]);
      else
        return Error("Invalid ARRAY type element");
      break;
    }

    if (NumRecords >= TypeList.size())
      return Error("invalid TYPE table");
    assert(ResultTy && "Didn't read a type?");
    assert(TypeList[NumRecords] == 0 && "Already read type?");
    TypeList[NumRecords++] = ResultTy;
  }
}

bool NaClBitcodeReader::ParseValueSymbolTable() {
  DEBUG(dbgs() << "-> ParseValueSymbolTable\n");
  if (Stream.EnterSubBlock(naclbitc::VALUE_SYMTAB_BLOCK_ID))
    return Error("Malformed block record");

  SmallVector<uint64_t, 64> Record;

  // Read all the records for this value table.
  SmallString<128> ValueName;
  while (1) {
    NaClBitstreamEntry Entry = Stream.advanceSkippingSubblocks();

    switch (Entry.Kind) {
    case NaClBitstreamEntry::SubBlock: // Handled for us already.
    case NaClBitstreamEntry::Error:
      return Error("malformed value symbol table block");
    case NaClBitstreamEntry::EndBlock:
      DEBUG(dbgs() << "<- ParseValueSymbolTable\n");
      return false;
    case NaClBitstreamEntry::Record:
      // The interesting case.
      break;
    }

    // Read a record.
    Record.clear();
    switch (Stream.readRecord(Entry.ID, Record)) {
    default:  // Default behavior: unknown type.
      break;
    case naclbitc::VST_CODE_ENTRY: {  // VST_ENTRY: [valueid, namechar x N]
      if (ConvertToString(Record, 1, ValueName))
        return Error("Invalid VST_ENTRY record");
      unsigned ValueID = Record[0];
      if (ValueID >= ValueList.size())
        return Error("Invalid Value ID in VST_ENTRY record");
      Value *V = ValueList[ValueID];

      V->setName(StringRef(ValueName.data(), ValueName.size()));
      ValueName.clear();
      break;
    }
    case naclbitc::VST_CODE_BBENTRY: {
      if (ConvertToString(Record, 1, ValueName))
        return Error("Invalid VST_BBENTRY record");
      BasicBlock *BB = getBasicBlock(Record[0]);
      if (BB == 0)
        return Error("Invalid BB ID in VST_BBENTRY record");

      BB->setName(StringRef(ValueName.data(), ValueName.size()));
      ValueName.clear();
      break;
    }
    }
  }
}

/// ResolveGlobalAndAliasInits - Resolve all of the initializers for global
/// values and aliases that we can.
bool NaClBitcodeReader::ResolveGlobalAndAliasInits() {
  std::vector<std::pair<GlobalVariable*, unsigned> > GlobalInitWorklist;
  std::vector<std::pair<GlobalAlias*, unsigned> > AliasInitWorklist;

  GlobalInitWorklist.swap(GlobalInits);
  AliasInitWorklist.swap(AliasInits);

  while (!GlobalInitWorklist.empty()) {
    unsigned ValID = GlobalInitWorklist.back().second;
    if (ValID >= ValueList.size()) {
      // Not ready to resolve this yet, it requires something later in the file.
      GlobalInits.push_back(GlobalInitWorklist.back());
    } else {
      if (Constant *C = dyn_cast<Constant>(ValueList[ValID]))
        GlobalInitWorklist.back().first->setInitializer(C);
      else
        return Error("Global variable initializer is not a constant!");
    }
    GlobalInitWorklist.pop_back();
  }

  while (!AliasInitWorklist.empty()) {
    unsigned ValID = AliasInitWorklist.back().second;
    if (ValID >= ValueList.size()) {
      AliasInits.push_back(AliasInitWorklist.back());
    } else {
      if (Constant *C = dyn_cast<Constant>(ValueList[ValID]))
        AliasInitWorklist.back().first->setAliasee(C);
      else
        return Error("Alias initializer is not a constant!");
    }
    AliasInitWorklist.pop_back();
  }
  return false;
}

static APInt ReadWideAPInt(ArrayRef<uint64_t> Vals, unsigned TypeBits) {
  SmallVector<uint64_t, 8> Words(Vals.size());
  std::transform(Vals.begin(), Vals.end(), Words.begin(),
                 NaClDecodeSignRotatedValue);

  return APInt(TypeBits, Words);
}

bool NaClBitcodeReader::ParseConstants() {
  DEBUG(dbgs() << "-> ParseConstants\n");
  if (Stream.EnterSubBlock(naclbitc::CONSTANTS_BLOCK_ID))
    return Error("Malformed block record");

  SmallVector<uint64_t, 64> Record;

  // Read all the records for this value table.
  Type *CurTy = Type::getInt32Ty(Context);
  unsigned NextCstNo = ValueList.size();
  while (1) {
    NaClBitstreamEntry Entry = Stream.advanceSkippingSubblocks();

    switch (Entry.Kind) {
    case NaClBitstreamEntry::SubBlock: // Handled for us already.
    case NaClBitstreamEntry::Error:
      return Error("malformed block record in AST file");
    case NaClBitstreamEntry::EndBlock:
      if (NextCstNo != ValueList.size())
        return Error("Invalid constant reference!");

      // Once all the constants have been read, go through and resolve forward
      // references.
      ValueList.ResolveConstantForwardRefs();
      DEBUG(dbgs() << "<- ParseConstants\n");
      return false;
    case NaClBitstreamEntry::Record:
      // The interesting case.
      break;
    }

    // Read a record.
    Record.clear();
    Value *V = 0;
    unsigned BitCode = Stream.readRecord(Entry.ID, Record);
    switch (BitCode) {
    default:  // Default behavior: unknown constant
    case naclbitc::CST_CODE_UNDEF:     // UNDEF
      V = UndefValue::get(CurTy);
      break;
    case naclbitc::CST_CODE_SETTYPE:   // SETTYPE: [typeid]
      if (Record.empty())
        return Error("Malformed CST_SETTYPE record");
      if (Record[0] >= TypeList.size())
        return Error("Invalid Type ID in CST_SETTYPE record");
      CurTy = TypeList[Record[0]];
      continue;  // Skip the ValueList manipulation.
    case naclbitc::CST_CODE_NULL:      // NULL
      V = Constant::getNullValue(CurTy);
      break;
    case naclbitc::CST_CODE_INTEGER:   // INTEGER: [intval]
      if (!CurTy->isIntegerTy() || Record.empty())
        return Error("Invalid CST_INTEGER record");
      V = ConstantInt::get(CurTy, NaClDecodeSignRotatedValue(Record[0]));
      break;
    case naclbitc::CST_CODE_WIDE_INTEGER: {// WIDE_INTEGER: [n x intval]
      if (!CurTy->isIntegerTy() || Record.empty())
        return Error("Invalid WIDE_INTEGER record");

      APInt VInt = ReadWideAPInt(Record,
                                 cast<IntegerType>(CurTy)->getBitWidth());
      V = ConstantInt::get(Context, VInt);

      break;
    }
    case naclbitc::CST_CODE_FLOAT: {    // FLOAT: [fpval]
      if (Record.empty())
        return Error("Invalid FLOAT record");
      if (CurTy->isHalfTy())
        V = ConstantFP::get(Context, APFloat(APFloat::IEEEhalf,
                                             APInt(16, (uint16_t)Record[0])));
      else if (CurTy->isFloatTy())
        V = ConstantFP::get(Context, APFloat(APFloat::IEEEsingle,
                                             APInt(32, (uint32_t)Record[0])));
      else if (CurTy->isDoubleTy())
        V = ConstantFP::get(Context, APFloat(APFloat::IEEEdouble,
                                             APInt(64, Record[0])));
      else if (CurTy->isX86_FP80Ty()) {
        // Bits are not stored the same way as a normal i80 APInt, compensate.
        uint64_t Rearrange[2];
        Rearrange[0] = (Record[1] & 0xffffLL) | (Record[0] << 16);
        Rearrange[1] = Record[0] >> 48;
        V = ConstantFP::get(Context, APFloat(APFloat::x87DoubleExtended,
                                             APInt(80, Rearrange)));
      } else if (CurTy->isFP128Ty())
        V = ConstantFP::get(Context, APFloat(APFloat::IEEEquad,
                                             APInt(128, Record)));
      else if (CurTy->isPPC_FP128Ty())
        V = ConstantFP::get(Context, APFloat(APFloat::PPCDoubleDouble,
                                             APInt(128, Record)));
      else
        V = UndefValue::get(CurTy);
      break;
    }

    case naclbitc::CST_CODE_AGGREGATE: {// AGGREGATE: [n x value number]
      if (Record.empty())
        return Error("Invalid CST_AGGREGATE record");

      unsigned Size = Record.size();
      SmallVector<Constant*, 16> Elts;

      if (StructType *STy = dyn_cast<StructType>(CurTy)) {
        for (unsigned i = 0; i != Size; ++i)
          Elts.push_back(ValueList.getConstantFwdRef(Record[i],
                                                     STy->getElementType(i)));
        V = ConstantStruct::get(STy, Elts);
      } else if (ArrayType *ATy = dyn_cast<ArrayType>(CurTy)) {
        Type *EltTy = ATy->getElementType();
        for (unsigned i = 0; i != Size; ++i)
          Elts.push_back(ValueList.getConstantFwdRef(Record[i], EltTy));
        V = ConstantArray::get(ATy, Elts);
      } else if (VectorType *VTy = dyn_cast<VectorType>(CurTy)) {
        Type *EltTy = VTy->getElementType();
        for (unsigned i = 0; i != Size; ++i)
          Elts.push_back(ValueList.getConstantFwdRef(Record[i], EltTy));
        V = ConstantVector::get(Elts);
      } else {
        V = UndefValue::get(CurTy);
      }
      break;
    }
    case naclbitc::CST_CODE_STRING:    // STRING: [values]
    case naclbitc::CST_CODE_CSTRING: { // CSTRING: [values]
      if (Record.empty())
        return Error("Invalid CST_STRING record");

      SmallString<16> Elts(Record.begin(), Record.end());
      V = ConstantDataArray::getString(Context, Elts,
                                       BitCode == naclbitc::CST_CODE_CSTRING);
      break;
    }
    case naclbitc::CST_CODE_DATA: {// DATA: [n x value]
      if (Record.empty())
        return Error("Invalid CST_DATA record");

      Type *EltTy = cast<SequentialType>(CurTy)->getElementType();
      unsigned Size = Record.size();

      if (EltTy->isIntegerTy(8)) {
        SmallVector<uint8_t, 16> Elts(Record.begin(), Record.end());
        if (isa<VectorType>(CurTy))
          V = ConstantDataVector::get(Context, Elts);
        else
          V = ConstantDataArray::get(Context, Elts);
      } else if (EltTy->isIntegerTy(16)) {
        SmallVector<uint16_t, 16> Elts(Record.begin(), Record.end());
        if (isa<VectorType>(CurTy))
          V = ConstantDataVector::get(Context, Elts);
        else
          V = ConstantDataArray::get(Context, Elts);
      } else if (EltTy->isIntegerTy(32)) {
        SmallVector<uint32_t, 16> Elts(Record.begin(), Record.end());
        if (isa<VectorType>(CurTy))
          V = ConstantDataVector::get(Context, Elts);
        else
          V = ConstantDataArray::get(Context, Elts);
      } else if (EltTy->isIntegerTy(64)) {
        SmallVector<uint64_t, 16> Elts(Record.begin(), Record.end());
        if (isa<VectorType>(CurTy))
          V = ConstantDataVector::get(Context, Elts);
        else
          V = ConstantDataArray::get(Context, Elts);
      } else if (EltTy->isFloatTy()) {
        SmallVector<float, 16> Elts(Size);
        std::transform(Record.begin(), Record.end(), Elts.begin(), BitsToFloat);
        if (isa<VectorType>(CurTy))
          V = ConstantDataVector::get(Context, Elts);
        else
          V = ConstantDataArray::get(Context, Elts);
      } else if (EltTy->isDoubleTy()) {
        SmallVector<double, 16> Elts(Size);
        std::transform(Record.begin(), Record.end(), Elts.begin(),
                       BitsToDouble);
        if (isa<VectorType>(CurTy))
          V = ConstantDataVector::get(Context, Elts);
        else
          V = ConstantDataArray::get(Context, Elts);
      } else {
        return Error("Unknown element type in CE_DATA");
      }
      break;
    }

    case naclbitc::CST_CODE_CE_BINOP: {  // CE_BINOP: [opcode, opval, opval]
      if (Record.size() < 3) return Error("Invalid CE_BINOP record");
      int Opc = GetDecodedBinaryOpcode(Record[0], CurTy);
      if (Opc < 0) {
        V = UndefValue::get(CurTy);  // Unknown binop.
      } else {
        Constant *LHS = ValueList.getConstantFwdRef(Record[1], CurTy);
        Constant *RHS = ValueList.getConstantFwdRef(Record[2], CurTy);
        unsigned Flags = 0;
        if (Record.size() >= 4) {
          if (Opc == Instruction::Add ||
              Opc == Instruction::Sub ||
              Opc == Instruction::Mul ||
              Opc == Instruction::Shl) {
            if (Record[3] & (1 << naclbitc::OBO_NO_SIGNED_WRAP))
              Flags |= OverflowingBinaryOperator::NoSignedWrap;
            if (Record[3] & (1 << naclbitc::OBO_NO_UNSIGNED_WRAP))
              Flags |= OverflowingBinaryOperator::NoUnsignedWrap;
          } else if (Opc == Instruction::SDiv ||
                     Opc == Instruction::UDiv ||
                     Opc == Instruction::LShr ||
                     Opc == Instruction::AShr) {
            if (Record[3] & (1 << naclbitc::PEO_EXACT))
              Flags |= SDivOperator::IsExact;
          }
        }
        V = ConstantExpr::get(Opc, LHS, RHS, Flags);
      }
      break;
    }
    case naclbitc::CST_CODE_CE_CAST: {  // CE_CAST: [opcode, opty, opval]
      if (Record.size() < 3) return Error("Invalid CE_CAST record");
      int Opc = GetDecodedCastOpcode(Record[0]);
      if (Opc < 0) {
        V = UndefValue::get(CurTy);  // Unknown cast.
      } else {
        Type *OpTy = getTypeByID(Record[1]);
        if (!OpTy) return Error("Invalid CE_CAST record");
        Constant *Op = ValueList.getConstantFwdRef(Record[2], OpTy);
        V = ConstantExpr::getCast(Opc, Op, CurTy);
      }
      break;
    }
    case naclbitc::CST_CODE_CE_INBOUNDS_GEP:
    case naclbitc::CST_CODE_CE_GEP: {  // CE_GEP:        [n x operands]
      if (Record.size() & 1) return Error("Invalid CE_GEP record");
      SmallVector<Constant*, 16> Elts;
      for (unsigned i = 0, e = Record.size(); i != e; i += 2) {
        Type *ElTy = getTypeByID(Record[i]);
        if (!ElTy) return Error("Invalid CE_GEP record");
        Elts.push_back(ValueList.getConstantFwdRef(Record[i+1], ElTy));
      }
      ArrayRef<Constant *> Indices(Elts.begin() + 1, Elts.end());
      V = ConstantExpr::getGetElementPtr(Elts[0], Indices,
                                         BitCode ==
                                           naclbitc::CST_CODE_CE_INBOUNDS_GEP);
      break;
    }
    case naclbitc::CST_CODE_CE_SELECT:  // CE_SELECT: [opval#, opval#, opval#]
      if (Record.size() < 3) return Error("Invalid CE_SELECT record");
      V = ConstantExpr::getSelect(
                          ValueList.getConstantFwdRef(Record[0],
                                                      Type::getInt1Ty(Context)),
                          ValueList.getConstantFwdRef(Record[1],CurTy),
                          ValueList.getConstantFwdRef(Record[2],CurTy));
      break;
    case naclbitc::CST_CODE_CE_EXTRACTELT: {
      // CE_EXTRACTELT: [opty, opval, opval]
      if (Record.size() < 3) return Error("Invalid CE_EXTRACTELT record");
      VectorType *OpTy =
        dyn_cast_or_null<VectorType>(getTypeByID(Record[0]));
      if (OpTy == 0) return Error("Invalid CE_EXTRACTELT record");
      Constant *Op0 = ValueList.getConstantFwdRef(Record[1], OpTy);
      Constant *Op1 = ValueList.getConstantFwdRef(Record[2],
                                                  Type::getInt32Ty(Context));
      V = ConstantExpr::getExtractElement(Op0, Op1);
      break;
    }
    case naclbitc::CST_CODE_CE_INSERTELT: {// CE_INSERTELT: [opval, opval, opval]
      VectorType *OpTy = dyn_cast<VectorType>(CurTy);
      if (Record.size() < 3 || OpTy == 0)
        return Error("Invalid CE_INSERTELT record");
      Constant *Op0 = ValueList.getConstantFwdRef(Record[0], OpTy);
      Constant *Op1 = ValueList.getConstantFwdRef(Record[1],
                                                  OpTy->getElementType());
      Constant *Op2 = ValueList.getConstantFwdRef(Record[2],
                                                  Type::getInt32Ty(Context));
      V = ConstantExpr::getInsertElement(Op0, Op1, Op2);
      break;
    }
    case naclbitc::CST_CODE_CE_SHUFFLEVEC: { // CE_SHUFFLEVEC: [opval, opval, opval]
      VectorType *OpTy = dyn_cast<VectorType>(CurTy);
      if (Record.size() < 3 || OpTy == 0)
        return Error("Invalid CE_SHUFFLEVEC record");
      Constant *Op0 = ValueList.getConstantFwdRef(Record[0], OpTy);
      Constant *Op1 = ValueList.getConstantFwdRef(Record[1], OpTy);
      Type *ShufTy = VectorType::get(Type::getInt32Ty(Context),
                                                 OpTy->getNumElements());
      Constant *Op2 = ValueList.getConstantFwdRef(Record[2], ShufTy);
      V = ConstantExpr::getShuffleVector(Op0, Op1, Op2);
      break;
    }
    case naclbitc::CST_CODE_CE_SHUFVEC_EX: { // [opty, opval, opval, opval]
      VectorType *RTy = dyn_cast<VectorType>(CurTy);
      VectorType *OpTy =
        dyn_cast_or_null<VectorType>(getTypeByID(Record[0]));
      if (Record.size() < 4 || RTy == 0 || OpTy == 0)
        return Error("Invalid CE_SHUFVEC_EX record");
      Constant *Op0 = ValueList.getConstantFwdRef(Record[1], OpTy);
      Constant *Op1 = ValueList.getConstantFwdRef(Record[2], OpTy);
      Type *ShufTy = VectorType::get(Type::getInt32Ty(Context),
                                                 RTy->getNumElements());
      Constant *Op2 = ValueList.getConstantFwdRef(Record[3], ShufTy);
      V = ConstantExpr::getShuffleVector(Op0, Op1, Op2);
      break;
    }
    case naclbitc::CST_CODE_CE_CMP: {     // CE_CMP: [opty, opval, opval, pred]
      if (Record.size() < 4) return Error("Invalid CE_CMP record");
      Type *OpTy = getTypeByID(Record[0]);
      if (OpTy == 0) return Error("Invalid CE_CMP record");
      Constant *Op0 = ValueList.getConstantFwdRef(Record[1], OpTy);
      Constant *Op1 = ValueList.getConstantFwdRef(Record[2], OpTy);

      if (OpTy->isFPOrFPVectorTy())
        V = ConstantExpr::getFCmp(Record[3], Op0, Op1);
      else
        V = ConstantExpr::getICmp(Record[3], Op0, Op1);
      break;
    }
    // This maintains backward compatibility, pre-asm dialect keywords.
    // FIXME: Remove with the 4.0 release.
    case naclbitc::CST_CODE_INLINEASM_OLD: {
      if (Record.size() < 2) return Error("Invalid INLINEASM record");
      std::string AsmStr, ConstrStr;
      bool HasSideEffects = Record[0] & 1;
      bool IsAlignStack = Record[0] >> 1;
      unsigned AsmStrSize = Record[1];
      if (2+AsmStrSize >= Record.size())
        return Error("Invalid INLINEASM record");
      unsigned ConstStrSize = Record[2+AsmStrSize];
      if (3+AsmStrSize+ConstStrSize > Record.size())
        return Error("Invalid INLINEASM record");

      for (unsigned i = 0; i != AsmStrSize; ++i)
        AsmStr += (char)Record[2+i];
      for (unsigned i = 0; i != ConstStrSize; ++i)
        ConstrStr += (char)Record[3+AsmStrSize+i];
      PointerType *PTy = cast<PointerType>(CurTy);
      V = InlineAsm::get(cast<FunctionType>(PTy->getElementType()),
                         AsmStr, ConstrStr, HasSideEffects, IsAlignStack);
      break;
    }
    // This version adds support for the asm dialect keywords (e.g.,
    // inteldialect).
    case naclbitc::CST_CODE_INLINEASM: {
      if (Record.size() < 2) return Error("Invalid INLINEASM record");
      std::string AsmStr, ConstrStr;
      bool HasSideEffects = Record[0] & 1;
      bool IsAlignStack = (Record[0] >> 1) & 1;
      unsigned AsmDialect = Record[0] >> 2;
      unsigned AsmStrSize = Record[1];
      if (2+AsmStrSize >= Record.size())
        return Error("Invalid INLINEASM record");
      unsigned ConstStrSize = Record[2+AsmStrSize];
      if (3+AsmStrSize+ConstStrSize > Record.size())
        return Error("Invalid INLINEASM record");

      for (unsigned i = 0; i != AsmStrSize; ++i)
        AsmStr += (char)Record[2+i];
      for (unsigned i = 0; i != ConstStrSize; ++i)
        ConstrStr += (char)Record[3+AsmStrSize+i];
      PointerType *PTy = cast<PointerType>(CurTy);
      V = InlineAsm::get(cast<FunctionType>(PTy->getElementType()),
                         AsmStr, ConstrStr, HasSideEffects, IsAlignStack,
                         InlineAsm::AsmDialect(AsmDialect));
      break;
    }
    case naclbitc::CST_CODE_BLOCKADDRESS:{
      if (Record.size() < 3) return Error("Invalid CE_BLOCKADDRESS record");
      Type *FnTy = getTypeByID(Record[0]);
      if (FnTy == 0) return Error("Invalid CE_BLOCKADDRESS record");
      Function *Fn =
        dyn_cast_or_null<Function>(ValueList.getConstantFwdRef(Record[1],FnTy));
      if (Fn == 0) return Error("Invalid CE_BLOCKADDRESS record");

      // If the function is already parsed we can insert the block address right
      // away.
      if (!Fn->empty()) {
        Function::iterator BBI = Fn->begin(), BBE = Fn->end();
        for (size_t I = 0, E = Record[2]; I != E; ++I) {
          if (BBI == BBE)
            return Error("Invalid blockaddress block #");
          ++BBI;
        }
        V = BlockAddress::get(Fn, BBI);
      } else {
        // Otherwise insert a placeholder and remember it so it can be inserted
        // when the function is parsed.
        GlobalVariable *FwdRef = new GlobalVariable(*Fn->getParent(),
                                                    Type::getInt8Ty(Context),
                                            false, GlobalValue::InternalLinkage,
                                                    0, "");
        BlockAddrFwdRefs[Fn].push_back(std::make_pair(Record[2], FwdRef));
        V = FwdRef;
      }
      break;
    }
    }

    ValueList.AssignValue(V, NextCstNo);
    ++NextCstNo;
  }
}

bool NaClBitcodeReader::ParseUseLists() {
  DEBUG(dbgs() << "-> ParseUseLists\n");
  if (Stream.EnterSubBlock(naclbitc::USELIST_BLOCK_ID))
    return Error("Malformed block record");

  SmallVector<uint64_t, 64> Record;

  // Read all the records.
  while (1) {
    NaClBitstreamEntry Entry = Stream.advanceSkippingSubblocks();

    switch (Entry.Kind) {
    case NaClBitstreamEntry::SubBlock: // Handled for us already.
    case NaClBitstreamEntry::Error:
      return Error("malformed use list block");
    case NaClBitstreamEntry::EndBlock:
      DEBUG(dbgs() << "<- ParseUseLists\n");
      return false;
    case NaClBitstreamEntry::Record:
      // The interesting case.
      break;
    }

    // Read a use list record.
    Record.clear();
    switch (Stream.readRecord(Entry.ID, Record)) {
    default:  // Default behavior: unknown type.
      break;
    case naclbitc::USELIST_CODE_ENTRY: { // USELIST_CODE_ENTRY: TBD.
      unsigned RecordLength = Record.size();
      if (RecordLength < 1)
        return Error ("Invalid UseList reader!");
      UseListRecords.push_back(Record);
      break;
    }
    }
  }
}

/// RememberAndSkipFunctionBody - When we see the block for a function body,
/// remember where it is and then skip it.  This lets us lazily deserialize the
/// functions.
bool NaClBitcodeReader::RememberAndSkipFunctionBody() {
  DEBUG(dbgs() << "-> RememberAndSkipFunctionBody\n");
  // Get the function we are talking about.
  if (FunctionsWithBodies.empty())
    return Error("Insufficient function protos");

  Function *Fn = FunctionsWithBodies.back();
  FunctionsWithBodies.pop_back();

  // Save the current stream state.
  uint64_t CurBit = Stream.GetCurrentBitNo();
  DeferredFunctionInfo[Fn] = CurBit;

  // Skip over the function block for now.
  if (Stream.SkipBlock())
    return Error("Malformed block record");
  DEBUG(dbgs() << "<- RememberAndSkipFunctionBody\n");
  return false;
}

bool NaClBitcodeReader::GlobalCleanup() {
  // Patch the initializers for globals and aliases up.
  ResolveGlobalAndAliasInits();
  if (!GlobalInits.empty() || !AliasInits.empty())
    return Error("Malformed global initializer set");

  // Look for intrinsic functions which need to be upgraded at some point
  for (Module::iterator FI = TheModule->begin(), FE = TheModule->end();
       FI != FE; ++FI) {
    Function *NewFn;
    if (UpgradeIntrinsicFunction(FI, NewFn))
      UpgradedIntrinsics.push_back(std::make_pair(FI, NewFn));
  }

  // Look for global variables which need to be renamed.
  for (Module::global_iterator
         GI = TheModule->global_begin(), GE = TheModule->global_end();
       GI != GE; ++GI)
    UpgradeGlobalVariable(GI);
  // Force deallocation of memory for these vectors to favor the client that
  // want lazy deserialization.
  std::vector<std::pair<GlobalVariable*, unsigned> >().swap(GlobalInits);
  std::vector<std::pair<GlobalAlias*, unsigned> >().swap(AliasInits);
  return false;
}

bool NaClBitcodeReader::ParseModule(bool Resume) {
  DEBUG(dbgs() << "-> ParseModule\n");
  if (Resume)
    Stream.JumpToBit(NextUnreadBit);
  else if (Stream.EnterSubBlock(naclbitc::MODULE_BLOCK_ID))
    return Error("Malformed block record");

  SmallVector<uint64_t, 64> Record;
  std::vector<std::string> SectionTable;
  std::vector<std::string> GCTable;

  // Read all the records for this module.
  while (1) {
    NaClBitstreamEntry Entry = Stream.advance();

    switch (Entry.Kind) {
    case NaClBitstreamEntry::Error:
      Error("malformed module block");
      return true;
    case NaClBitstreamEntry::EndBlock:
      DEBUG(dbgs() << "<- ParseModule\n");
      return GlobalCleanup();

    case NaClBitstreamEntry::SubBlock:
      switch (Entry.ID) {
      default:  // Skip unknown content.
        DEBUG(dbgs() << "Skip unknown context\n");
        if (Stream.SkipBlock())
          return Error("Malformed block record");
        break;
      case naclbitc::BLOCKINFO_BLOCK_ID:
        if (Stream.ReadBlockInfoBlock())
          return Error("Malformed BlockInfoBlock");
        break;
      case naclbitc::TYPE_BLOCK_ID_NEW:
        if (ParseTypeTable())
          return true;
        break;
      case naclbitc::VALUE_SYMTAB_BLOCK_ID:
        if (ParseValueSymbolTable())
          return true;
        SeenValueSymbolTable = true;
        break;
      case naclbitc::CONSTANTS_BLOCK_ID:
        if (ParseConstants() || ResolveGlobalAndAliasInits())
          return true;
        break;
      case naclbitc::FUNCTION_BLOCK_ID:
        // If this is the first function body we've seen, reverse the
        // FunctionsWithBodies list.
        if (!SeenFirstFunctionBody) {
          std::reverse(FunctionsWithBodies.begin(), FunctionsWithBodies.end());
          if (GlobalCleanup())
            return true;
          SeenFirstFunctionBody = true;
        }

        if (RememberAndSkipFunctionBody())
          return true;

        // For streaming bitcode, suspend parsing when we reach the function
        // bodies. Subsequent materialization calls will resume it when
        // necessary. For streaming, the function bodies must be at the end of
        // the bitcode. If the bitcode file is old, the symbol table will be
        // at the end instead and will not have been seen yet. In this case,
        // just finish the parse now.
        if (LazyStreamer && SeenValueSymbolTable) {
          NextUnreadBit = Stream.GetCurrentBitNo();
          DEBUG(dbgs() << "<- ParseModule\n");
          return false;
        }
        break;
      case naclbitc::USELIST_BLOCK_ID:
        if (ParseUseLists())
          return true;
        break;
      }
      continue;

    case NaClBitstreamEntry::Record:
      // The interesting case.
      break;
    }


    // Read a record.
    switch (Stream.readRecord(Entry.ID, Record)) {
    default: break;  // Default behavior, ignore unknown content.
    case naclbitc::MODULE_CODE_VERSION: {  // VERSION: [version#]
      if (Record.size() < 1)
        return Error("Malformed MODULE_CODE_VERSION");
      // Only version #0 and #1 are supported so far.
      unsigned module_version = Record[0];
      switch (module_version) {
        default: return Error("Unknown bitstream version!");
        case 0:
          UseRelativeIDs = false;
          break;
        case 1:
          UseRelativeIDs = true;
          break;
      }
      break;
    }
    case naclbitc::MODULE_CODE_ASM: {  // ASM: [strchr x N]
      std::string S;
      if (ConvertToString(Record, 0, S))
        return Error("Invalid MODULE_CODE_ASM record");
      TheModule->setModuleInlineAsm(S);
      break;
    }
    case naclbitc::MODULE_CODE_DEPLIB: {  // DEPLIB: [strchr x N]
      // FIXME: Remove in 4.0.
      std::string S;
      if (ConvertToString(Record, 0, S))
        return Error("Invalid MODULE_CODE_DEPLIB record");
      // Ignore value.
      break;
    }
    case naclbitc::MODULE_CODE_SECTIONNAME: {  // SECTIONNAME: [strchr x N]
      std::string S;
      if (ConvertToString(Record, 0, S))
        return Error("Invalid MODULE_CODE_SECTIONNAME record");
      SectionTable.push_back(S);
      break;
    }
    case naclbitc::MODULE_CODE_GCNAME: {  // SECTIONNAME: [strchr x N]
      std::string S;
      if (ConvertToString(Record, 0, S))
        return Error("Invalid MODULE_CODE_GCNAME record");
      GCTable.push_back(S);
      break;
    }
    // GLOBALVAR: [pointer type, isconst, initid,
    //             linkage, alignment, section, visibility, threadlocal,
    //             unnamed_addr]
    case naclbitc::MODULE_CODE_GLOBALVAR: {
      if (Record.size() < 6)
        return Error("Invalid MODULE_CODE_GLOBALVAR record");
      Type *Ty = getTypeByID(Record[0]);
      if (!Ty) return Error("Invalid MODULE_CODE_GLOBALVAR record");
      if (!Ty->isPointerTy())
        return Error("Global not a pointer type!");
      unsigned AddressSpace = cast<PointerType>(Ty)->getAddressSpace();
      Ty = cast<PointerType>(Ty)->getElementType();

      bool isConstant = Record[1];
      GlobalValue::LinkageTypes Linkage = GetDecodedLinkage(Record[3]);
      unsigned Alignment = (1 << Record[4]) >> 1;
      std::string Section;
      if (Record[5]) {
        if (Record[5]-1 >= SectionTable.size())
          return Error("Invalid section ID");
        Section = SectionTable[Record[5]-1];
      }
      GlobalValue::VisibilityTypes Visibility = GlobalValue::DefaultVisibility;
      if (Record.size() > 6)
        Visibility = GetDecodedVisibility(Record[6]);

      GlobalVariable::ThreadLocalMode TLM = GlobalVariable::NotThreadLocal;
      if (Record.size() > 7)
        TLM = GetDecodedThreadLocalMode(Record[7]);

      bool UnnamedAddr = false;
      if (Record.size() > 8)
        UnnamedAddr = Record[8];

      bool ExternallyInitialized = false;
      if (Record.size() > 9)
        ExternallyInitialized = Record[9];

      GlobalVariable *NewGV =
        new GlobalVariable(*TheModule, Ty, isConstant, Linkage, 0, "", 0,
                           TLM, AddressSpace, ExternallyInitialized);
      NewGV->setAlignment(Alignment);
      if (!Section.empty())
        NewGV->setSection(Section);
      NewGV->setVisibility(Visibility);
      NewGV->setUnnamedAddr(UnnamedAddr);

      ValueList.push_back(NewGV);

      // Remember which value to use for the global initializer.
      if (unsigned InitID = Record[2])
        GlobalInits.push_back(std::make_pair(NewGV, InitID-1));
      break;
    }
    // FUNCTION:  [type, callingconv, isproto, linkage]
    case naclbitc::MODULE_CODE_FUNCTION: {
      if (Record.size() < 4)
        return Error("Invalid MODULE_CODE_FUNCTION record");
      Type *Ty = getTypeByID(Record[0]);
      if (!Ty) return Error("Invalid MODULE_CODE_FUNCTION record");
      if (!Ty->isPointerTy())
        return Error("Function not a pointer type!");
      FunctionType *FTy =
        dyn_cast<FunctionType>(cast<PointerType>(Ty)->getElementType());
      if (!FTy)
        return Error("Function not a pointer to function type!");

      Function *Func = Function::Create(FTy, GlobalValue::ExternalLinkage,
                                        "", TheModule);

      Func->setCallingConv(GetDecodedCallingConv(Record[1]));
      bool isProto = Record[2];
      Func->setLinkage(GetDecodedLinkage(Record[3]));
      ValueList.push_back(Func);

      // If this is a function with a body, remember the prototype we are
      // creating now, so that we can match up the body with them later.
      if (!isProto) {
        FunctionsWithBodies.push_back(Func);
        if (LazyStreamer) DeferredFunctionInfo[Func] = 0;
      }
      break;
    }
    // ALIAS: [alias type, aliasee val#, linkage]
    // ALIAS: [alias type, aliasee val#, linkage, visibility]
    case naclbitc::MODULE_CODE_ALIAS: {
      if (Record.size() < 3)
        return Error("Invalid MODULE_ALIAS record");
      Type *Ty = getTypeByID(Record[0]);
      if (!Ty) return Error("Invalid MODULE_ALIAS record");
      if (!Ty->isPointerTy())
        return Error("Function not a pointer type!");

      GlobalAlias *NewGA = new GlobalAlias(Ty, GetDecodedLinkage(Record[2]),
                                           "", 0, TheModule);
      // Old bitcode files didn't have visibility field.
      if (Record.size() > 3)
        NewGA->setVisibility(GetDecodedVisibility(Record[3]));
      ValueList.push_back(NewGA);
      AliasInits.push_back(std::make_pair(NewGA, Record[1]));
      break;
    }
    /// MODULE_CODE_PURGEVALS: [numvals]
    case naclbitc::MODULE_CODE_PURGEVALS:
      // Trim down the value list to the specified size.
      if (Record.size() < 1 || Record[0] > ValueList.size())
        return Error("Invalid MODULE_PURGEVALS record");
      ValueList.shrinkTo(Record[0]);
      break;
    }
    Record.clear();
  }
}

bool NaClBitcodeReader::ParseBitcodeInto(Module *M) {
  TheModule = 0;

  // PNaCl does not support different DataLayouts in pexes, so we
  // implicitly set the DataLayout to the following default.
  //
  // This is not usually needed by the backend, but it might be used
  // by IR passes that the PNaCl translator runs.  We set this in the
  // reader rather than in pnacl-llc so that 'opt' will also use the
  // correct DataLayout if it is run on a pexe.
  M->setDataLayout("e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-"
                   "f32:32:32-f64:64:64-p:32:32:32-v128:32:32");

  if (InitStream()) return true;

  // We expect a number of well-defined blocks, though we don't necessarily
  // need to understand them all.
  while (1) {
    if (Stream.AtEndOfStream())
      return false;

    NaClBitstreamEntry Entry =
      Stream.advance(NaClBitstreamCursor::AF_DontAutoprocessAbbrevs);

    switch (Entry.Kind) {
    case NaClBitstreamEntry::Error:
      Error("malformed module file");
      return true;
    case NaClBitstreamEntry::EndBlock:
      return false;

    case NaClBitstreamEntry::SubBlock:
      switch (Entry.ID) {
      case naclbitc::BLOCKINFO_BLOCK_ID:
        if (Stream.ReadBlockInfoBlock())
          return Error("Malformed BlockInfoBlock");
        break;
      case naclbitc::MODULE_BLOCK_ID:
        // Reject multiple MODULE_BLOCK's in a single bitstream.
        if (TheModule)
          return Error("Multiple MODULE_BLOCKs in same stream");
        TheModule = M;
        if (ParseModule(false))
          return true;
        if (LazyStreamer) return false;
        break;
      default:
        if (Stream.SkipBlock())
          return Error("Malformed block record");
        break;
      }
      continue;
    case NaClBitstreamEntry::Record:
      // There should be no records in the top-level of blocks.

      // The ranlib in Xcode 4 will align archive members by appending newlines
      // to the end of them. If this file size is a multiple of 4 but not 8, we
      // have to read and ignore these final 4 bytes :-(
      if (Stream.getAbbrevIDWidth() == 2 && Entry.ID == 2 &&
          Stream.Read(6) == 2 && Stream.Read(24) == 0xa0a0a &&
          Stream.AtEndOfStream())
        return false;

      return Error("Invalid record at top-level");
    }
  }
}

/// ParseFunctionBody - Lazily parse the specified function body block.
bool NaClBitcodeReader::ParseFunctionBody(Function *F) {
  DEBUG(dbgs() << "-> ParseFunctionBody\n");
  if (Stream.EnterSubBlock(naclbitc::FUNCTION_BLOCK_ID))
    return Error("Malformed block record");

  InstructionList.clear();
  unsigned ModuleValueListSize = ValueList.size();

  // Add all the function arguments to the value table.
  for(Function::arg_iterator I = F->arg_begin(), E = F->arg_end(); I != E; ++I)
    ValueList.push_back(I);

  unsigned NextValueNo = ValueList.size();
  BasicBlock *CurBB = 0;
  unsigned CurBBNo = 0;

  // Read all the records.
  SmallVector<uint64_t, 64> Record;
  while (1) {
    NaClBitstreamEntry Entry = Stream.advance();

    switch (Entry.Kind) {
    case NaClBitstreamEntry::Error:
      return Error("Bitcode error in function block");
    case NaClBitstreamEntry::EndBlock:
      goto OutOfRecordLoop;

    case NaClBitstreamEntry::SubBlock:
      switch (Entry.ID) {
      default:  // Skip unknown content.
        dbgs() << "default skip block\n";
        if (Stream.SkipBlock())
          return Error("Malformed block record");
        break;
      case naclbitc::CONSTANTS_BLOCK_ID:
        if (ParseConstants())
          return true;
        NextValueNo = ValueList.size();
        break;
      case naclbitc::VALUE_SYMTAB_BLOCK_ID:
        if (ParseValueSymbolTable())
          return true;
        break;
      }
      continue;

    case NaClBitstreamEntry::Record:
      // The interesting case.
      break;
    }

    // Read a record.
    Record.clear();
    Instruction *I = 0;
    unsigned BitCode = Stream.readRecord(Entry.ID, Record);
    switch (BitCode) {
    default: // Default behavior: reject
      return Error("Unknown instruction");
    case naclbitc::FUNC_CODE_DECLAREBLOCKS:     // DECLAREBLOCKS: [nblocks]
      if (Record.size() < 1 || Record[0] == 0)
        return Error("Invalid DECLAREBLOCKS record");
      // Create all the basic blocks for the function.
      FunctionBBs.resize(Record[0]);
      for (unsigned i = 0, e = FunctionBBs.size(); i != e; ++i)
        FunctionBBs[i] = BasicBlock::Create(Context, "", F);
      CurBB = FunctionBBs[0];
      continue;

    case naclbitc::FUNC_CODE_INST_BINOP: {
      // BINOP: [opval, opval, opcode[, flags]]
      unsigned OpNum = 0;
      Value *LHS, *RHS;
      if (popValue(Record, &OpNum, NextValueNo, &LHS) ||
          popValue(Record, &OpNum, NextValueNo, &RHS) ||
          OpNum+1 > Record.size())
        return Error("Invalid BINOP record");

      int Opc = GetDecodedBinaryOpcode(Record[OpNum++], LHS->getType());
      if (Opc == -1) return Error("Invalid BINOP record");
      I = BinaryOperator::Create((Instruction::BinaryOps)Opc, LHS, RHS);
      InstructionList.push_back(I);
      if (OpNum < Record.size()) {
        if (Opc == Instruction::Add ||
            Opc == Instruction::Sub ||
            Opc == Instruction::Mul ||
            Opc == Instruction::Shl) {
          if (Record[OpNum] & (1 << naclbitc::OBO_NO_SIGNED_WRAP))
            cast<BinaryOperator>(I)->setHasNoSignedWrap(true);
          if (Record[OpNum] & (1 << naclbitc::OBO_NO_UNSIGNED_WRAP))
            cast<BinaryOperator>(I)->setHasNoUnsignedWrap(true);
        } else if (Opc == Instruction::SDiv ||
                   Opc == Instruction::UDiv ||
                   Opc == Instruction::LShr ||
                   Opc == Instruction::AShr) {
          if (Record[OpNum] & (1 << naclbitc::PEO_EXACT))
            cast<BinaryOperator>(I)->setIsExact(true);
        } else if (isa<FPMathOperator>(I)) {
          FastMathFlags FMF;
          if (0 != (Record[OpNum] & (1 << naclbitc::FPO_UNSAFE_ALGEBRA)))
            FMF.setUnsafeAlgebra();
          if (0 != (Record[OpNum] & (1 << naclbitc::FPO_NO_NANS)))
            FMF.setNoNaNs();
          if (0 != (Record[OpNum] & (1 << naclbitc::FPO_NO_INFS)))
            FMF.setNoInfs();
          if (0 != (Record[OpNum] & (1 << naclbitc::FPO_NO_SIGNED_ZEROS)))
            FMF.setNoSignedZeros();
          if (0 != (Record[OpNum] & (1 << naclbitc::FPO_ALLOW_RECIPROCAL)))
            FMF.setAllowReciprocal();
          if (FMF.any())
            I->setFastMathFlags(FMF);
        }

      }
      break;
    }
    case naclbitc::FUNC_CODE_INST_CAST: {    // CAST: [opval, destty, castopc]
      unsigned OpNum = 0;
      Value *Op;
      if (popValue(Record, &OpNum, NextValueNo, &Op) ||
          OpNum+2 != Record.size())
        return Error("Invalid CAST record");

      Type *ResTy = getTypeByID(Record[OpNum]);
      int Opc = GetDecodedCastOpcode(Record[OpNum+1]);
      if (Opc == -1 || ResTy == 0)
        return Error("Invalid CAST record");
      I = CastInst::Create((Instruction::CastOps)Opc, Op, ResTy);
      InstructionList.push_back(I);
      break;
    }
    case naclbitc::FUNC_CODE_INST_INBOUNDS_GEP:
    case naclbitc::FUNC_CODE_INST_GEP: { // GEP: [n x operands]
      unsigned OpNum = 0;
      Value *BasePtr;
      if (popValue(Record, &OpNum, NextValueNo, &BasePtr))
        return Error("Invalid GEP record");

      SmallVector<Value*, 16> GEPIdx;
      while (OpNum != Record.size()) {
        Value *Op;
        if (popValue(Record, &OpNum, NextValueNo, &Op))
          return Error("Invalid GEP record");
        GEPIdx.push_back(Op);
      }

      I = GetElementPtrInst::Create(BasePtr, GEPIdx);
      InstructionList.push_back(I);
      if (BitCode == naclbitc::FUNC_CODE_INST_INBOUNDS_GEP)
        cast<GetElementPtrInst>(I)->setIsInBounds(true);
      break;
    }

    case naclbitc::FUNC_CODE_INST_EXTRACTVAL: {
                                       // EXTRACTVAL: [opval, n x indices]
      unsigned OpNum = 0;
      Value *Agg;
      if (popValue(Record, &OpNum, NextValueNo, &Agg))
        return Error("Invalid EXTRACTVAL record");

      SmallVector<unsigned, 4> EXTRACTVALIdx;
      for (unsigned RecSize = Record.size();
           OpNum != RecSize; ++OpNum) {
        uint64_t Index = Record[OpNum];
        if ((unsigned)Index != Index)
          return Error("Invalid EXTRACTVAL index");
        EXTRACTVALIdx.push_back((unsigned)Index);
      }

      I = ExtractValueInst::Create(Agg, EXTRACTVALIdx);
      InstructionList.push_back(I);
      break;
    }

    case naclbitc::FUNC_CODE_INST_INSERTVAL: {
                           // INSERTVAL: [opval, opval, n x indices]
      unsigned OpNum = 0;
      Value *Agg;
      if (popValue(Record, &OpNum, NextValueNo, &Agg))
        return Error("Invalid INSERTVAL record");
      Value *Val;
      if (popValue(Record, &OpNum, NextValueNo, &Val))
        return Error("Invalid INSERTVAL record");

      SmallVector<unsigned, 4> INSERTVALIdx;
      for (unsigned RecSize = Record.size();
           OpNum != RecSize; ++OpNum) {
        uint64_t Index = Record[OpNum];
        if ((unsigned)Index != Index)
          return Error("Invalid INSERTVAL index");
        INSERTVALIdx.push_back((unsigned)Index);
      }

      I = InsertValueInst::Create(Agg, Val, INSERTVALIdx);
      InstructionList.push_back(I);
      break;
    }

    case naclbitc::FUNC_CODE_INST_SELECT: { // SELECT: [opval, opval, opval]
      // obsolete form of select
      // handles select i1 ... in old bitcode
      unsigned OpNum = 0;
      Value *TrueVal, *FalseVal, *Cond;
      if (popValue(Record, &OpNum, NextValueNo, &TrueVal) ||
          popValue(Record, &OpNum, NextValueNo, &FalseVal) ||
          popValue(Record, &OpNum, NextValueNo, &Cond))
        return Error("Invalid SELECT record");

      I = SelectInst::Create(Cond, TrueVal, FalseVal);
      InstructionList.push_back(I);
      break;
    }

    case naclbitc::FUNC_CODE_INST_VSELECT: {// VSELECT: [opval, opval, pred]
      // new form of select
      // handles select i1 or select [N x i1]
      unsigned OpNum = 0;
      Value *TrueVal, *FalseVal, *Cond;
      if (popValue(Record, &OpNum, NextValueNo, &TrueVal) ||
          popValue(Record, &OpNum, NextValueNo, &FalseVal) ||
          popValue(Record, &OpNum, NextValueNo, &Cond))
        return Error("Invalid SELECT record");

      // select condition can be either i1 or [N x i1]
      if (VectorType* vector_type =
          dyn_cast<VectorType>(Cond->getType())) {
        // expect <n x i1>
        if (vector_type->getElementType() != Type::getInt1Ty(Context))
          return Error("Invalid SELECT condition type");
      } else {
        // expect i1
        if (Cond->getType() != Type::getInt1Ty(Context))
          return Error("Invalid SELECT condition type");
      }

      I = SelectInst::Create(Cond, TrueVal, FalseVal);
      InstructionList.push_back(I);
      break;
    }

    case naclbitc::FUNC_CODE_INST_EXTRACTELT: { // EXTRACTELT: [opval, opval]
      unsigned OpNum = 0;
      Value *Vec, *Idx;
      if (popValue(Record, &OpNum, NextValueNo, &Vec) ||
          popValue(Record, &OpNum, NextValueNo, &Idx))
        return Error("Invalid EXTRACTELT record");
      I = ExtractElementInst::Create(Vec, Idx);
      InstructionList.push_back(I);
      break;
    }

    case naclbitc::FUNC_CODE_INST_INSERTELT: { // INSERTELT: [opval, opval, opval]
      unsigned OpNum = 0;
      Value *Vec, *Elt, *Idx;
      if (popValue(Record, &OpNum, NextValueNo, &Vec) ||
          popValue(Record, &OpNum, NextValueNo, &Elt) ||
          popValue(Record, &OpNum, NextValueNo, &Idx))
        return Error("Invalid INSERTELT record");
      I = InsertElementInst::Create(Vec, Elt, Idx);
      InstructionList.push_back(I);
      break;
    }

    case naclbitc::FUNC_CODE_INST_SHUFFLEVEC: {// SHUFFLEVEC: [opval, opval, opval]
      unsigned OpNum = 0;
      Value *Vec1, *Vec2, *Mask;
      if (popValue(Record, &OpNum, NextValueNo, &Vec1) ||
          popValue(Record, &OpNum, NextValueNo, &Vec2))
        return Error("Invalid SHUFFLEVEC record");

      if (popValue(Record, &OpNum, NextValueNo, &Mask))
        return Error("Invalid SHUFFLEVEC record");
      I = new ShuffleVectorInst(Vec1, Vec2, Mask);
      InstructionList.push_back(I);
      break;
    }

    case naclbitc::FUNC_CODE_INST_CMP:   // CMP: [opval, opval, pred]
      // Old form of ICmp/FCmp returning bool
      // Existed to differentiate between icmp/fcmp and vicmp/vfcmp which were
      // both legal on vectors but had different behaviour.
    case naclbitc::FUNC_CODE_INST_CMP2: { // CMP2: [opval, opval, pred]
      // FCmp/ICmp returning bool or vector of bool

      unsigned OpNum = 0;
      Value *LHS, *RHS;
      if (popValue(Record, &OpNum, NextValueNo, &LHS) ||
          popValue(Record, &OpNum, NextValueNo, &RHS) ||
          OpNum+1 != Record.size())
        return Error("Invalid CMP record");

      if (LHS->getType()->isFPOrFPVectorTy())
        I = new FCmpInst((FCmpInst::Predicate)Record[OpNum], LHS, RHS);
      else
        I = new ICmpInst((ICmpInst::Predicate)Record[OpNum], LHS, RHS);
      InstructionList.push_back(I);
      break;
    }

    case naclbitc::FUNC_CODE_INST_RET: // RET: [opval<optional>]
      {
        unsigned Size = Record.size();
        if (Size == 0) {
          I = ReturnInst::Create(Context);
          InstructionList.push_back(I);
          break;
        }

        unsigned OpNum = 0;
        Value *Op = NULL;
        if (popValue(Record, &OpNum, NextValueNo, &Op))
          return Error("Invalid RET record");
        if (OpNum != Record.size())
          return Error("Invalid RET record");

        I = ReturnInst::Create(Context, Op);
        InstructionList.push_back(I);
        break;
      }
    case naclbitc::FUNC_CODE_INST_BR: { // BR: [bb#, bb#, opval] or [bb#]
      if (Record.size() != 1 && Record.size() != 3)
        return Error("Invalid BR record");
      BasicBlock *TrueDest = getBasicBlock(Record[0]);
      if (TrueDest == 0)
        return Error("Invalid BR record");

      if (Record.size() == 1) {
        I = BranchInst::Create(TrueDest);
        InstructionList.push_back(I);
      }
      else {
        BasicBlock *FalseDest = getBasicBlock(Record[1]);
        Value *Cond = getValue(Record, 2, NextValueNo);
        if (FalseDest == 0 || Cond == 0)
          return Error("Invalid BR record");
        I = BranchInst::Create(TrueDest, FalseDest, Cond);
        InstructionList.push_back(I);
      }
      break;
    }
    case naclbitc::FUNC_CODE_INST_SWITCH: { // SWITCH: [opty, op0, op1, ...]
      // New SwitchInst format with case ranges.
      if (Record.size() < 4)
        return Error("Invalid SWITCH record");
      Type *OpTy = getTypeByID(Record[0]);
      unsigned ValueBitWidth = cast<IntegerType>(OpTy)->getBitWidth();

      Value *Cond = getValue(Record, 1, NextValueNo);
      BasicBlock *Default = getBasicBlock(Record[2]);
      if (OpTy == 0 || Cond == 0 || Default == 0)
        return Error("Invalid SWITCH record");

      unsigned NumCases = Record[3];

      SwitchInst *SI = SwitchInst::Create(Cond, Default, NumCases);
      InstructionList.push_back(SI);

      unsigned CurIdx = 4;
      for (unsigned i = 0; i != NumCases; ++i) {
        IntegersSubsetToBB CaseBuilder;
        unsigned NumItems = Record[CurIdx++];
        for (unsigned ci = 0; ci != NumItems; ++ci) {
          bool isSingleNumber = Record[CurIdx++];

          APInt Low;
          unsigned ActiveWords = 1;
          if (ValueBitWidth > 64)
            ActiveWords = Record[CurIdx++];
          Low = ReadWideAPInt(makeArrayRef(&Record[CurIdx], ActiveWords),
                              ValueBitWidth);
          CurIdx += ActiveWords;

          if (!isSingleNumber) {
            ActiveWords = 1;
            if (ValueBitWidth > 64)
              ActiveWords = Record[CurIdx++];
            APInt High =
                ReadWideAPInt(makeArrayRef(&Record[CurIdx], ActiveWords),
                              ValueBitWidth);

            CaseBuilder.add(IntItem::fromType(OpTy, Low),
                            IntItem::fromType(OpTy, High));
            CurIdx += ActiveWords;
          } else
            CaseBuilder.add(IntItem::fromType(OpTy, Low));
        }
        BasicBlock *DestBB = getBasicBlock(Record[CurIdx++]);
        IntegersSubset Case = CaseBuilder.getCase();
        SI->addCase(Case, DestBB);
      }
      I = SI;
      break;
    }
    case naclbitc::FUNC_CODE_INST_INDIRECTBR: { // INDIRECTBR: [opty, op0, op1, ...]
      if (Record.size() < 2)
        return Error("Invalid INDIRECTBR record");
      Type *OpTy = getTypeByID(Record[0]);
      Value *Address = getValue(Record, 1, NextValueNo);
      if (OpTy == 0 || Address == 0)
        return Error("Invalid INDIRECTBR record");
      unsigned NumDests = Record.size()-2;
      IndirectBrInst *IBI = IndirectBrInst::Create(Address, NumDests);
      InstructionList.push_back(IBI);
      for (unsigned i = 0, e = NumDests; i != e; ++i) {
        if (BasicBlock *DestBB = getBasicBlock(Record[2+i])) {
          IBI->addDestination(DestBB);
        } else {
          delete IBI;
          return Error("Invalid INDIRECTBR record!");
        }
      }
      I = IBI;
      break;
    }

    case naclbitc::FUNC_CODE_INST_INVOKE:
      return Error("Invoke is not allowed");
      break;
    case naclbitc::FUNC_CODE_INST_RESUME: { // RESUME: [opval]
      unsigned Idx = 0;
      Value *Val = 0;
      if (popValue(Record, &Idx, NextValueNo, &Val))
        return Error("Invalid RESUME record");
      I = ResumeInst::Create(Val);
      InstructionList.push_back(I);
      break;
    }
    case naclbitc::FUNC_CODE_INST_UNREACHABLE: // UNREACHABLE
      I = new UnreachableInst(Context);
      InstructionList.push_back(I);
      break;
    case naclbitc::FUNC_CODE_INST_PHI: { // PHI: [ty, val0,bb0, ...]
      if (Record.size() < 1 || ((Record.size()-1)&1))
        return Error("Invalid PHI record");
      Type *Ty = getTypeByID(Record[0]);
      if (!Ty) return Error("Invalid PHI record");

      PHINode *PN = PHINode::Create(Ty, (Record.size()-1)/2);
      InstructionList.push_back(PN);

      for (unsigned i = 0, e = Record.size()-1; i != e; i += 2) {
        Value *V;
        // With the new function encoding, it is possible that operands have
        // negative IDs (for forward references).  Use a signed VBR
        // representation to keep the encoding small.
        if (UseRelativeIDs)
          V = getValueSigned(Record, 1+i, NextValueNo);
        else
          V = getValue(Record, 1+i, NextValueNo);
        BasicBlock *BB = getBasicBlock(Record[2+i]);
        if (!V || !BB) return Error("Invalid PHI record");
        PN->addIncoming(V, BB);
      }
      I = PN;
      break;
    }

    case naclbitc::FUNC_CODE_INST_LANDINGPAD: {
      // LANDINGPAD: [ty, val, val, num, (id0,val0 ...)?]
      unsigned Idx = 0;
      if (Record.size() < 4)
        return Error("Invalid LANDINGPAD record");
      Type *Ty = getTypeByID(Record[Idx++]);
      if (!Ty) return Error("Invalid LANDINGPAD record");
      Value *PersFn = 0;
      if (popValue(Record, &Idx, NextValueNo, &PersFn))
        return Error("Invalid LANDINGPAD record");

      bool IsCleanup = !!Record[Idx++];
      unsigned NumClauses = Record[Idx++];
      LandingPadInst *LP = LandingPadInst::Create(Ty, PersFn, NumClauses);
      LP->setCleanup(IsCleanup);
      for (unsigned J = 0; J != NumClauses; ++J) {
        LandingPadInst::ClauseType CT =
          LandingPadInst::ClauseType(Record[Idx++]); (void)CT;
        Value *Val;

        if (popValue(Record, &Idx, NextValueNo, &Val)) {
          delete LP;
          return Error("Invalid LANDINGPAD record");
        }

        assert((CT != LandingPadInst::Catch ||
                !isa<ArrayType>(Val->getType())) &&
               "Catch clause has a invalid type!");
        assert((CT != LandingPadInst::Filter ||
                isa<ArrayType>(Val->getType())) &&
               "Filter clause has invalid type!");
        LP->addClause(Val);
      }

      I = LP;
      InstructionList.push_back(I);
      break;
    }

    case naclbitc::FUNC_CODE_INST_ALLOCA: { // ALLOCA: [op, align]
      if (Record.size() != 2)
        return Error("Invalid ALLOCA record");
      Value *Size;
      unsigned OpNum = 0;
      if (popValue(Record, &OpNum, NextValueNo, &Size))
        return Error("Invalid ALLOCA record");
      unsigned Align = Record[1];
      I = new AllocaInst(Type::getInt8Ty(Context), Size, (1 << Align) >> 1);
      InstructionList.push_back(I);
      break;
    }
    case naclbitc::FUNC_CODE_INST_LOAD: { // LOAD: [op, align, vol]
      unsigned OpNum = 0;
      Value *Op;
      if (popValue(Record, &OpNum, NextValueNo, &Op) ||
          OpNum+2 != Record.size())
        return Error("Invalid LOAD record");

      I = new LoadInst(Op, "", Record[OpNum+1], (1 << Record[OpNum]) >> 1);
      InstructionList.push_back(I);
      break;
    }
    case naclbitc::FUNC_CODE_INST_LOADATOMIC: {
       // LOADATOMIC: [op, align, vol, ordering, synchscope]
      unsigned OpNum = 0;
      Value *Op;
      if (popValue(Record, &OpNum, NextValueNo, &Op) ||
          OpNum+4 != Record.size())
        return Error("Invalid LOADATOMIC record");


      AtomicOrdering Ordering = GetDecodedOrdering(Record[OpNum+2]);
      if (Ordering == NotAtomic || Ordering == Release ||
          Ordering == AcquireRelease)
        return Error("Invalid LOADATOMIC record");
      if (Ordering != NotAtomic && Record[OpNum] == 0)
        return Error("Invalid LOADATOMIC record");
      SynchronizationScope SynchScope = GetDecodedSynchScope(Record[OpNum+3]);

      I = new LoadInst(Op, "", Record[OpNum+1], (1 << Record[OpNum]) >> 1,
                       Ordering, SynchScope);
      InstructionList.push_back(I);
      break;
    }
    case naclbitc::FUNC_CODE_INST_STORE: { // STORE2:[ptr, val, align, vol]
      unsigned OpNum = 0;
      Value *Val, *Ptr;
      if (popValue(Record, &OpNum, NextValueNo, &Ptr) ||
          popValue(Record, &OpNum, NextValueNo, &Val) ||
          OpNum+2 != Record.size())
        return Error("Invalid STORE record");

      I = new StoreInst(Val, Ptr, Record[OpNum+1], (1 << Record[OpNum]) >> 1);
      InstructionList.push_back(I);
      break;
    }
    case naclbitc::FUNC_CODE_INST_STOREATOMIC: {
      // STOREATOMIC: [ptr, val, align, vol, ordering, synchscope]
      unsigned OpNum = 0;
      Value *Val, *Ptr;
      if (popValue(Record, &OpNum, NextValueNo, &Ptr) ||
          popValue(Record, &OpNum, NextValueNo, &Val) ||
          OpNum+4 != Record.size())
        return Error("Invalid STOREATOMIC record");

      AtomicOrdering Ordering = GetDecodedOrdering(Record[OpNum+2]);
      if (Ordering == NotAtomic || Ordering == Acquire ||
          Ordering == AcquireRelease)
        return Error("Invalid STOREATOMIC record");
      SynchronizationScope SynchScope = GetDecodedSynchScope(Record[OpNum+3]);
      if (Ordering != NotAtomic && Record[OpNum] == 0)
        return Error("Invalid STOREATOMIC record");

      I = new StoreInst(Val, Ptr, Record[OpNum+1], (1 << Record[OpNum]) >> 1,
                        Ordering, SynchScope);
      InstructionList.push_back(I);
      break;
    }
    case naclbitc::FUNC_CODE_INST_CMPXCHG: {
      // CMPXCHG:[ptr, cmp, new, vol, ordering, synchscope]
      unsigned OpNum = 0;
      Value *Ptr, *Cmp, *New;
      if (popValue(Record, &OpNum, NextValueNo, &Ptr) ||
          popValue(Record, &OpNum, NextValueNo, &Cmp) ||
          popValue(Record, &OpNum, NextValueNo, &New) ||
          OpNum+3 != Record.size())
        return Error("Invalid CMPXCHG record");
      AtomicOrdering Ordering = GetDecodedOrdering(Record[OpNum+1]);
      if (Ordering == NotAtomic || Ordering == Unordered)
        return Error("Invalid CMPXCHG record");
      SynchronizationScope SynchScope = GetDecodedSynchScope(Record[OpNum+2]);
      I = new AtomicCmpXchgInst(Ptr, Cmp, New, Ordering, SynchScope);
      cast<AtomicCmpXchgInst>(I)->setVolatile(Record[OpNum]);
      InstructionList.push_back(I);
      break;
    }
    case naclbitc::FUNC_CODE_INST_ATOMICRMW: {
      // ATOMICRMW:[ptr, val, op, vol, ordering, synchscope]
      unsigned OpNum = 0;
      Value *Ptr, *Val;
      if (popValue(Record, &OpNum, NextValueNo, &Ptr) ||
          popValue(Record, &OpNum, NextValueNo, &Val) ||
          OpNum+4 != Record.size())
        return Error("Invalid ATOMICRMW record");
      AtomicRMWInst::BinOp Operation = GetDecodedRMWOperation(Record[OpNum]);
      if (Operation < AtomicRMWInst::FIRST_BINOP ||
          Operation > AtomicRMWInst::LAST_BINOP)
        return Error("Invalid ATOMICRMW record");
      AtomicOrdering Ordering = GetDecodedOrdering(Record[OpNum+2]);
      if (Ordering == NotAtomic || Ordering == Unordered)
        return Error("Invalid ATOMICRMW record");
      SynchronizationScope SynchScope = GetDecodedSynchScope(Record[OpNum+3]);
      I = new AtomicRMWInst(Operation, Ptr, Val, Ordering, SynchScope);
      cast<AtomicRMWInst>(I)->setVolatile(Record[OpNum+1]);
      InstructionList.push_back(I);
      break;
    }
    case naclbitc::FUNC_CODE_INST_FENCE: { // FENCE:[ordering, synchscope]
      if (2 != Record.size())
        return Error("Invalid FENCE record");
      AtomicOrdering Ordering = GetDecodedOrdering(Record[0]);
      if (Ordering == NotAtomic || Ordering == Unordered ||
          Ordering == Monotonic)
        return Error("Invalid FENCE record");
      SynchronizationScope SynchScope = GetDecodedSynchScope(Record[1]);
      I = new FenceInst(Context, Ordering, SynchScope);
      InstructionList.push_back(I);
      break;
    }
    case naclbitc::FUNC_CODE_INST_CALL: {
      // CALL: [cc, fnid, arg0, arg1...]
      if (Record.size() < 2)
        return Error("Invalid CALL record");

      unsigned CCInfo = Record[0];

      unsigned OpNum = 1;
      Value *Callee;
      if (popValue(Record, &OpNum, NextValueNo, &Callee))
        return Error("Invalid CALL record");

      PointerType *OpTy = dyn_cast<PointerType>(Callee->getType());
      FunctionType *FTy = 0;
      if (OpTy) FTy = dyn_cast<FunctionType>(OpTy->getElementType());
      if (!FTy || Record.size() < FTy->getNumParams()+OpNum)
        return Error("Invalid CALL record");

      SmallVector<Value*, 16> Args;
      // Read the fixed params.
      for (unsigned i = 0, e = FTy->getNumParams(); i != e; ++i, ++OpNum) {
        if (FTy->getParamType(i)->isLabelTy())
          Args.push_back(getBasicBlock(Record[OpNum]));
        else
          Args.push_back(getValue(Record, OpNum, NextValueNo));
        if (Args.back() == 0) return Error("Invalid CALL record");
      }

      // Read type/value pairs for varargs params.
      if (!FTy->isVarArg()) {
        if (OpNum != Record.size())
          return Error("Invalid CALL record");
      } else {
        while (OpNum != Record.size()) {
          Value *Op;
          if (popValue(Record, &OpNum, NextValueNo, &Op))
            return Error("Invalid CALL record");
          Args.push_back(Op);
        }
      }

      I = CallInst::Create(Callee, Args);
      InstructionList.push_back(I);
      cast<CallInst>(I)->setCallingConv(GetDecodedCallingConv(CCInfo>>1));
      cast<CallInst>(I)->setTailCall(CCInfo & 1);
      break;
    }
    case naclbitc::FUNC_CODE_INST_VAARG: { // VAARG: [valistty, valist, instty]
      if (Record.size() < 3)
        return Error("Invalid VAARG record");
      Type *OpTy = getTypeByID(Record[0]);
      Value *Op = getValue(Record, 1, NextValueNo);
      Type *ResTy = getTypeByID(Record[2]);
      if (!OpTy || !Op || !ResTy)
        return Error("Invalid VAARG record");
      I = new VAArgInst(Op, ResTy);
      InstructionList.push_back(I);
      break;
    }
    case naclbitc::FUNC_CODE_INST_FORWARDTYPEREF:
      // Build corresponding forward reference.
      if (Record.size() != 2 ||
          ValueList.createValueFwdRef(Record[0], getTypeByID(Record[1])))
        return Error("Invalid FORWARDTYPEREF record");
      continue;
    }

    // Add instruction to end of current BB.  If there is no current BB, reject
    // this file.
    if (CurBB == 0) {
      delete I;
      return Error("Invalid instruction with no BB");
    }
    CurBB->getInstList().push_back(I);

    // If this was a terminator instruction, move to the next block.
    if (isa<TerminatorInst>(I)) {
      ++CurBBNo;
      CurBB = CurBBNo < FunctionBBs.size() ? FunctionBBs[CurBBNo] : 0;
    }

    // Non-void values get registered in the value table for future use.
    if (I && !I->getType()->isVoidTy())
      ValueList.AssignValue(I, NextValueNo++);
  }

OutOfRecordLoop:

  // Check the function list for unresolved values.
  if (Argument *A = dyn_cast<Argument>(ValueList.back())) {
    if (A->getParent() == 0) {
      // We found at least one unresolved value.  Nuke them all to avoid leaks.
      for (unsigned i = ModuleValueListSize, e = ValueList.size(); i != e; ++i){
        if ((A = dyn_cast<Argument>(ValueList[i])) && A->getParent() == 0) {
          A->replaceAllUsesWith(UndefValue::get(A->getType()));
          delete A;
        }
      }
      return Error("Never resolved value found in function!");
    }
  }

  // See if anything took the address of blocks in this function.  If so,
  // resolve them now.
  DenseMap<Function*, std::vector<BlockAddrRefTy> >::iterator BAFRI =
    BlockAddrFwdRefs.find(F);
  if (BAFRI != BlockAddrFwdRefs.end()) {
    std::vector<BlockAddrRefTy> &RefList = BAFRI->second;
    for (unsigned i = 0, e = RefList.size(); i != e; ++i) {
      unsigned BlockIdx = RefList[i].first;
      if (BlockIdx >= FunctionBBs.size())
        return Error("Invalid blockaddress block #");

      GlobalVariable *FwdRef = RefList[i].second;
      FwdRef->replaceAllUsesWith(BlockAddress::get(F, FunctionBBs[BlockIdx]));
      FwdRef->eraseFromParent();
    }

    BlockAddrFwdRefs.erase(BAFRI);
  }

  // Trim the value list down to the size it was before we parsed this function.
  ValueList.shrinkTo(ModuleValueListSize);
  std::vector<BasicBlock*>().swap(FunctionBBs);
  DEBUG(dbgs() << "-> ParseFunctionBody\n");
  return false;
}

/// FindFunctionInStream - Find the function body in the bitcode stream
bool NaClBitcodeReader::FindFunctionInStream(Function *F,
       DenseMap<Function*, uint64_t>::iterator DeferredFunctionInfoIterator) {
  while (DeferredFunctionInfoIterator->second == 0) {
    if (Stream.AtEndOfStream())
      return Error("Could not find Function in stream");
    // ParseModule will parse the next body in the stream and set its
    // position in the DeferredFunctionInfo map.
    if (ParseModule(true)) return true;
  }
  return false;
}

//===----------------------------------------------------------------------===//
// GVMaterializer implementation
//===----------------------------------------------------------------------===//


bool NaClBitcodeReader::isMaterializable(const GlobalValue *GV) const {
  if (const Function *F = dyn_cast<Function>(GV)) {
    return F->isDeclaration() &&
      DeferredFunctionInfo.count(const_cast<Function*>(F));
  }
  return false;
}

bool NaClBitcodeReader::Materialize(GlobalValue *GV, std::string *ErrInfo) {
  Function *F = dyn_cast<Function>(GV);
  // If it's not a function or is already material, ignore the request.
  if (!F || !F->isMaterializable()) return false;

  DenseMap<Function*, uint64_t>::iterator DFII = DeferredFunctionInfo.find(F);
  assert(DFII != DeferredFunctionInfo.end() && "Deferred function not found!");
  // If its position is recorded as 0, its body is somewhere in the stream
  // but we haven't seen it yet.
  if (DFII->second == 0)
    if (LazyStreamer && FindFunctionInStream(F, DFII)) return true;

  // Move the bit stream to the saved position of the deferred function body.
  Stream.JumpToBit(DFII->second);

  if (ParseFunctionBody(F)) {
    if (ErrInfo) *ErrInfo = ErrorString;
    return true;
  }

  // Upgrade any old intrinsic calls in the function.
  for (UpgradedIntrinsicMap::iterator I = UpgradedIntrinsics.begin(),
       E = UpgradedIntrinsics.end(); I != E; ++I) {
    if (I->first != I->second) {
      for (Value::use_iterator UI = I->first->use_begin(),
           UE = I->first->use_end(); UI != UE; ) {
        if (CallInst* CI = dyn_cast<CallInst>(*UI++))
          UpgradeIntrinsicCall(CI, I->second);
      }
    }
  }

  return false;
}

bool NaClBitcodeReader::isDematerializable(const GlobalValue *GV) const {
  const Function *F = dyn_cast<Function>(GV);
  if (!F || F->isDeclaration())
    return false;
  // @LOCALMOD-START
  // Don't dematerialize functions with BBs which have their address taken;
  // it will cause any referencing blockAddress constants to also be destroyed,
  // but because they are GVs, they need to stay around until PassManager
  // finalization.
  for (Function::const_iterator BB = F->begin(); BB != F->end(); ++BB) {
    if (BB->hasAddressTaken())
      return false;
  }
  // @LOCALMOD-END
  return DeferredFunctionInfo.count(const_cast<Function*>(F));
}

void NaClBitcodeReader::Dematerialize(GlobalValue *GV) {
  Function *F = dyn_cast<Function>(GV);
  // If this function isn't dematerializable, this is a noop.
  if (!F || !isDematerializable(F))
    return;

  assert(DeferredFunctionInfo.count(F) && "No info to read function later?");

  // Just forget the function body, we can remat it later.
  F->deleteBody();
}


bool NaClBitcodeReader::MaterializeModule(Module *M, std::string *ErrInfo) {
  assert(M == TheModule &&
         "Can only Materialize the Module this NaClBitcodeReader is attached to.");
  // Iterate over the module, deserializing any functions that are still on
  // disk.
  for (Module::iterator F = TheModule->begin(), E = TheModule->end();
       F != E; ++F)
    if (F->isMaterializable() &&
        Materialize(F, ErrInfo))
      return true;

  // At this point, if there are any function bodies, the current bit is
  // pointing to the END_BLOCK record after them. Now make sure the rest
  // of the bits in the module have been read.
  if (NextUnreadBit)
    ParseModule(true);

  // Upgrade any intrinsic calls that slipped through (should not happen!) and
  // delete the old functions to clean up. We can't do this unless the entire
  // module is materialized because there could always be another function body
  // with calls to the old function.
  for (std::vector<std::pair<Function*, Function*> >::iterator I =
       UpgradedIntrinsics.begin(), E = UpgradedIntrinsics.end(); I != E; ++I) {
    if (I->first != I->second) {
      for (Value::use_iterator UI = I->first->use_begin(),
           UE = I->first->use_end(); UI != UE; ) {
        if (CallInst* CI = dyn_cast<CallInst>(*UI++))
          UpgradeIntrinsicCall(CI, I->second);
      }
      if (!I->first->use_empty())
        I->first->replaceAllUsesWith(I->second);
      I->first->eraseFromParent();
    }
  }
  std::vector<std::pair<Function*, Function*> >().swap(UpgradedIntrinsics);

  return false;
}

bool NaClBitcodeReader::InitStream() {
  if (LazyStreamer) return InitLazyStream();
  return InitStreamFromBuffer();
}

bool NaClBitcodeReader::InitStreamFromBuffer() {
  const unsigned char *BufPtr = (const unsigned char*)Buffer->getBufferStart();
  const unsigned char *BufEnd = BufPtr+Buffer->getBufferSize();

  if (Buffer->getBufferSize() & 3)
    return Error("Bitcode stream should be a multiple of 4 bytes in length");

  if (Header.Read(BufPtr, BufEnd))
    return Error("Invalid PNaCl bitcode header");

  StreamFile.reset(new NaClBitstreamReader(BufPtr, BufEnd));
  Stream.init(*StreamFile);

  return AcceptHeader();
}

bool NaClBitcodeReader::InitLazyStream() {
  StreamingMemoryObject *Bytes = new StreamingMemoryObject(LazyStreamer);
  if (Header.Read(Bytes))
    return Error("Invalid PNaCl bitcode header");

  StreamFile.reset(new NaClBitstreamReader(Bytes, Header.getHeaderSize()));
  Stream.init(*StreamFile);
  return AcceptHeader();
}

//===----------------------------------------------------------------------===//
// External interface
//===----------------------------------------------------------------------===//

/// getNaClLazyBitcodeModule - lazy function-at-a-time loading from a file.
///
Module *llvm::getNaClLazyBitcodeModule(MemoryBuffer *Buffer,
                                       LLVMContext& Context,
                                       std::string *ErrMsg,
                                       bool AcceptSupportedOnly) {
  Module *M = new Module(Buffer->getBufferIdentifier(), Context);
  NaClBitcodeReader *R =
      new NaClBitcodeReader(Buffer, Context, AcceptSupportedOnly);
  M->setMaterializer(R);
  if (R->ParseBitcodeInto(M)) {
    if (ErrMsg)
      *ErrMsg = R->getErrorString();

    delete M;  // Also deletes R.
    return 0;
  }
  // Have the NaClBitcodeReader dtor delete 'Buffer'.
  R->setBufferOwned(true);

  R->materializeForwardReferencedFunctions();

  return M;
}


Module *llvm::getNaClStreamedBitcodeModule(const std::string &name,
                                           DataStreamer *streamer,
                                           LLVMContext &Context,
                                           std::string *ErrMsg,
                                           bool AcceptSupportedOnly) {
  Module *M = new Module(name, Context);
  NaClBitcodeReader *R =
      new NaClBitcodeReader(streamer, Context, AcceptSupportedOnly);
  M->setMaterializer(R);
  if (R->ParseBitcodeInto(M)) {
    if (ErrMsg)
      *ErrMsg = R->getErrorString();
    delete M;  // Also deletes R.
    return 0;
  }
  R->setBufferOwned(false); // no buffer to delete

  R->materializeForwardReferencedFunctions();

  return M;
}

/// NaClParseBitcodeFile - Read the specified bitcode file, returning the module.
/// If an error occurs, return null and fill in *ErrMsg if non-null.
Module *llvm::NaClParseBitcodeFile(MemoryBuffer *Buffer, LLVMContext& Context,
                                   std::string *ErrMsg,
                                   bool AcceptSupportedOnly){
  Module *M = getNaClLazyBitcodeModule(Buffer, Context, ErrMsg,
                                       AcceptSupportedOnly);
  if (!M) return 0;

  // Don't let the NaClBitcodeReader dtor delete 'Buffer', regardless of whether
  // there was an error.
  static_cast<NaClBitcodeReader*>(M->getMaterializer())->setBufferOwned(false);

  // Read in the entire module, and destroy the NaClBitcodeReader.
  if (M->MaterializeAllPermanently(ErrMsg)) {
    delete M;
    return 0;
  }

  // TODO: Restore the use-lists to the in-memory state when the bitcode was
  // written.  We must defer until the Module has been fully materialized.

  return M;
}
