//===--- Bitcode/NaCl/Writer/NaClBitcodeWriter.cpp - Bitcode Writer -------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Bitcode writer implementation.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "NaClBitcodeWriter"

#include "llvm/Bitcode/NaCl/NaClBitcodeHeader.h"
#include "llvm/Bitcode/NaCl/NaClReaderWriter.h"
#include "NaClValueEnumerator.h"
#include "llvm/Bitcode/NaCl/NaClBitstreamWriter.h"
#include "llvm/Bitcode/NaCl/NaClLLVMBitCodes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/raw_ostream.h"
#include <cctype>
#include <map>
using namespace llvm;

static cl::opt<unsigned>
PNaClVersion("pnacl-version",
             cl::desc("Specify PNaCl bitcode version to write"),
             cl::init(2));

static cl::opt<bool>
AlignBitcodeRecords("align-bitcode-records",
    cl::desc("Align bitcode records in PNaCl bitcode files (experimental)"),
    cl::init(false));

/// These are manifest constants used by the bitcode writer. They do
/// not need to be kept in sync with the reader, but need to be
/// consistent within this file.
///
/// Note that for each block type GROUP, the last entry should be of
/// the form:
///
///    GROUP_MAX_ABBREV = GROUP_LAST_ABBREV,
///
/// where GROUP_LAST_ABBREV is the last defined abbreviation. See
/// include file "llvm/Bitcode/NaCl/NaClBitCodes.h" for more
/// information on how groups should be defined.
enum {
  // VALUE_SYMTAB_BLOCK abbrev id's.
  VST_ENTRY_8_ABBREV = naclbitc::FIRST_APPLICATION_ABBREV,
  VST_ENTRY_7_ABBREV,
  VST_ENTRY_6_ABBREV,
  VST_BBENTRY_6_ABBREV,
  VST_MAX_ABBREV = VST_BBENTRY_6_ABBREV,

  // CONSTANTS_BLOCK abbrev id's.
  CONSTANTS_SETTYPE_ABBREV = naclbitc::FIRST_APPLICATION_ABBREV,
  CONSTANTS_INTEGER_ABBREV,
  CONSTANTS_INTEGER_ZERO_ABBREV,
  CONSTANTS_FLOAT_ABBREV,
  CONSTANTS_MAX_ABBREV = CONSTANTS_FLOAT_ABBREV,

  // GLOBALVAR BLOCK abbrev id's.
  GLOBALVAR_VAR_ABBREV = naclbitc::FIRST_APPLICATION_ABBREV,
  GLOBALVAR_COMPOUND_ABBREV,
  GLOBALVAR_ZEROFILL_ABBREV,
  GLOBALVAR_DATA_ABBREV,
  GLOBALVAR_RELOC_ABBREV,
  GLOBALVAR_RELOC_WITH_ADDEND_ABBREV,
  GLOBALVAR_MAX_ABBREV = GLOBALVAR_RELOC_WITH_ADDEND_ABBREV,

  // FUNCTION_BLOCK abbrev id's.
  FUNCTION_INST_LOAD_ABBREV = naclbitc::FIRST_APPLICATION_ABBREV,
  FUNCTION_INST_BINOP_ABBREV,
  FUNCTION_INST_CAST_ABBREV,
  FUNCTION_INST_RET_VOID_ABBREV,
  FUNCTION_INST_RET_VAL_ABBREV,
  FUNCTION_INST_UNREACHABLE_ABBREV,
  FUNCTION_INST_FORWARDTYPEREF_ABBREV,
  FUNCTION_INST_STORE_ABBREV,
  FUNCTION_INST_MAX_ABBREV = FUNCTION_INST_STORE_ABBREV,

  // TYPE_BLOCK_ID_NEW abbrev id's.
  TYPE_FUNCTION_ABBREV = naclbitc::FIRST_APPLICATION_ABBREV,
  TYPE_MAX_ABBREV = TYPE_FUNCTION_ABBREV
};

LLVM_ATTRIBUTE_NORETURN
static void ReportIllegalValue(const char *ValueMessage,
                               const Value &Value) {
  std::string Message;
  raw_string_ostream StrM(Message);
  StrM << "NaCl Illegal ";
  if (ValueMessage != 0)
    StrM << ValueMessage << " ";
  StrM << ": " << Value;
  report_fatal_error(StrM.str());
}

static unsigned GetEncodedCastOpcode(unsigned Opcode, const Value &V) {
  switch (Opcode) {
  default: ReportIllegalValue("cast", V);
  case Instruction::Trunc   : return naclbitc::CAST_TRUNC;
  case Instruction::ZExt    : return naclbitc::CAST_ZEXT;
  case Instruction::SExt    : return naclbitc::CAST_SEXT;
  case Instruction::FPToUI  : return naclbitc::CAST_FPTOUI;
  case Instruction::FPToSI  : return naclbitc::CAST_FPTOSI;
  case Instruction::UIToFP  : return naclbitc::CAST_UITOFP;
  case Instruction::SIToFP  : return naclbitc::CAST_SITOFP;
  case Instruction::FPTrunc : return naclbitc::CAST_FPTRUNC;
  case Instruction::FPExt   : return naclbitc::CAST_FPEXT;
  case Instruction::BitCast : return naclbitc::CAST_BITCAST;
  }
}

static unsigned GetEncodedBinaryOpcode(unsigned Opcode, const Value &V) {
  switch (Opcode) {
  default: ReportIllegalValue("binary opcode", V);
  case Instruction::Add:
  case Instruction::FAdd: return naclbitc::BINOP_ADD;
  case Instruction::Sub:
  case Instruction::FSub: return naclbitc::BINOP_SUB;
  case Instruction::Mul:
  case Instruction::FMul: return naclbitc::BINOP_MUL;
  case Instruction::UDiv: return naclbitc::BINOP_UDIV;
  case Instruction::FDiv:
  case Instruction::SDiv: return naclbitc::BINOP_SDIV;
  case Instruction::URem: return naclbitc::BINOP_UREM;
  case Instruction::FRem:
  case Instruction::SRem: return naclbitc::BINOP_SREM;
  case Instruction::Shl:  return naclbitc::BINOP_SHL;
  case Instruction::LShr: return naclbitc::BINOP_LSHR;
  case Instruction::AShr: return naclbitc::BINOP_ASHR;
  case Instruction::And:  return naclbitc::BINOP_AND;
  case Instruction::Or:   return naclbitc::BINOP_OR;
  case Instruction::Xor:  return naclbitc::BINOP_XOR;
  }
}

static unsigned GetEncodedCallingConv(CallingConv::ID conv) {
  switch (conv) {
  default: report_fatal_error(
      "Calling convention not supported by PNaCL bitcode");
  case CallingConv::C: return naclbitc::C_CallingConv;
  }
}

// Converts LLVM encoding of comparison predicates to the
// corresponding bitcode versions.
static unsigned GetEncodedCmpPredicate(const CmpInst &Cmp) {
  switch (Cmp.getPredicate()) {
  default: report_fatal_error(
      "Comparison predicate not supported by PNaCl bitcode");
  case CmpInst::FCMP_FALSE:
    return naclbitc::FCMP_FALSE;
  case CmpInst::FCMP_OEQ:
    return naclbitc::FCMP_OEQ;
  case CmpInst::FCMP_OGT:
    return naclbitc::FCMP_OGT;
  case CmpInst::FCMP_OGE:
    return naclbitc::FCMP_OGE;
  case CmpInst::FCMP_OLT:
    return naclbitc::FCMP_OLT;
  case CmpInst::FCMP_OLE:
    return naclbitc::FCMP_OLE;
  case CmpInst::FCMP_ONE:
    return naclbitc::FCMP_ONE;
  case CmpInst::FCMP_ORD:
    return naclbitc::FCMP_ORD;
  case CmpInst::FCMP_UNO:
    return naclbitc::FCMP_UNO;
  case CmpInst::FCMP_UEQ:
    return naclbitc::FCMP_UEQ;
  case CmpInst::FCMP_UGT:
    return naclbitc::FCMP_UGT;
  case CmpInst::FCMP_UGE:
    return naclbitc::FCMP_UGE;
  case CmpInst::FCMP_ULT:
    return naclbitc::FCMP_ULT;
  case CmpInst::FCMP_ULE:
    return naclbitc::FCMP_ULE;
  case CmpInst::FCMP_UNE:
    return naclbitc::FCMP_UNE;
  case CmpInst::FCMP_TRUE:
    return naclbitc::FCMP_TRUE;
  case CmpInst::ICMP_EQ:
    return naclbitc::ICMP_EQ;
  case CmpInst::ICMP_NE:
    return naclbitc::ICMP_NE;
  case CmpInst::ICMP_UGT:
    return naclbitc::ICMP_UGT;
  case CmpInst::ICMP_UGE:
    return naclbitc::ICMP_UGE;
  case CmpInst::ICMP_ULT:
    return naclbitc::ICMP_ULT;
  case CmpInst::ICMP_ULE:
    return naclbitc::ICMP_ULE;
  case CmpInst::ICMP_SGT:
    return naclbitc::ICMP_SGT;
  case CmpInst::ICMP_SGE:
    return naclbitc::ICMP_SGE;
  case CmpInst::ICMP_SLT:
    return naclbitc::ICMP_SLT;
  case CmpInst::ICMP_SLE:
    return naclbitc::ICMP_SLE;
  }
}

// The type of encoding to use for type ids.
static NaClBitCodeAbbrevOp::Encoding TypeIdEncoding = NaClBitCodeAbbrevOp::VBR;

// The cutoff (in number of bits) from Fixed to VBR.
static const unsigned TypeIdVBRCutoff = 6;

// The number of bits to use in the encoding of type ids.
static unsigned TypeIdNumBits = TypeIdVBRCutoff;

// Optimizes the value for TypeIdEncoding and TypeIdNumBits based
// the actual number of types.
static inline void OptimizeTypeIdEncoding(const NaClValueEnumerator &VE) {
  // Note: modify to use maximum number of bits if under cutoff. Otherwise,
  // use VBR to take advantage that frequently referenced types have
  // small IDs.
  unsigned NumBits = NaClBitsNeededForValue(VE.getTypes().size());
  TypeIdNumBits = (NumBits < TypeIdVBRCutoff ? NumBits : TypeIdVBRCutoff);
  TypeIdEncoding = NaClBitCodeAbbrevOp::Encoding(
      NumBits <= TypeIdVBRCutoff
      ? NaClBitCodeAbbrevOp::Fixed : NaClBitCodeAbbrevOp::VBR);
}

/// WriteTypeTable - Write out the type table for a module.
static void WriteTypeTable(const NaClValueEnumerator &VE,
                           NaClBitstreamWriter &Stream) {
  DEBUG(dbgs() << "-> WriteTypeTable\n");
  const NaClValueEnumerator::TypeList &TypeList = VE.getTypes();

  Stream.EnterSubblock(naclbitc::TYPE_BLOCK_ID_NEW, TYPE_MAX_ABBREV);

  SmallVector<uint64_t, 64> TypeVals;

  // Abbrev for TYPE_CODE_FUNCTION.
  NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
  Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::TYPE_CODE_FUNCTION));
  Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, 1));  // isvararg
  Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Array));
  Abbv->Add(NaClBitCodeAbbrevOp(TypeIdEncoding, TypeIdNumBits));
  if (TYPE_FUNCTION_ABBREV != Stream.EmitAbbrev(Abbv))
    llvm_unreachable("Unexpected abbrev ordering!");

  // Emit an entry count so the reader can reserve space.
  TypeVals.push_back(TypeList.size());
  Stream.EmitRecord(naclbitc::TYPE_CODE_NUMENTRY, TypeVals);
  TypeVals.clear();

  // Loop over all of the types, emitting each in turn.
  for (unsigned i = 0, e = TypeList.size(); i != e; ++i) {
    Type *T = TypeList[i];
    int AbbrevToUse = 0;
    unsigned Code = 0;

    switch (T->getTypeID()) {
    default: llvm_unreachable("Unknown type!");
    case Type::VoidTyID:      Code = naclbitc::TYPE_CODE_VOID;      break;
    case Type::FloatTyID:     Code = naclbitc::TYPE_CODE_FLOAT;     break;
    case Type::DoubleTyID:    Code = naclbitc::TYPE_CODE_DOUBLE;    break;
    case Type::IntegerTyID:
      // INTEGER: [width]
      Code = naclbitc::TYPE_CODE_INTEGER;
      TypeVals.push_back(cast<IntegerType>(T)->getBitWidth());
      break;
    case Type::VectorTyID: {
      VectorType *VT = cast<VectorType>(T);
      // VECTOR [numelts, eltty]
      Code = naclbitc::TYPE_CODE_VECTOR;
      TypeVals.push_back(VT->getNumElements());
      TypeVals.push_back(VE.getTypeID(VT->getElementType()));
      break;
    }
    case Type::FunctionTyID: {
      FunctionType *FT = cast<FunctionType>(T);
      // FUNCTION: [isvararg, retty, paramty x N]
      Code = naclbitc::TYPE_CODE_FUNCTION;
      TypeVals.push_back(FT->isVarArg());
      TypeVals.push_back(VE.getTypeID(FT->getReturnType()));
      for (unsigned i = 0, e = FT->getNumParams(); i != e; ++i)
        TypeVals.push_back(VE.getTypeID(FT->getParamType(i)));
      AbbrevToUse = TYPE_FUNCTION_ABBREV;
      break;
    }
    case Type::StructTyID:
      report_fatal_error("Struct types are not supported in PNaCl bitcode");
    case Type::ArrayTyID:
      report_fatal_error("Array types are not supported in PNaCl bitcode");
    }

    // Emit the finished record.
    Stream.EmitRecord(Code, TypeVals, AbbrevToUse);
    TypeVals.clear();
  }

  Stream.ExitBlock();
  DEBUG(dbgs() << "<- WriteTypeTable\n");
}

static unsigned getEncodedLinkage(const GlobalValue *GV) {
  switch (GV->getLinkage()) {
  case GlobalValue::ExternalLinkage:                 return 0;
  case GlobalValue::InternalLinkage:                 return 3;
  default:
    report_fatal_error("Invalid linkage");
  }
}

/// \brief Function to convert constant initializers for global
/// variables into corresponding bitcode. Takes advantage that these
/// global variable initializations are normalized (see
/// lib/Transforms/NaCl/FlattenGlobals.cpp).
void WriteGlobalInit(const Constant *C, unsigned GlobalVarID,
                     SmallVectorImpl<uint32_t> &Vals,
                     const NaClValueEnumerator &VE,
                     NaClBitstreamWriter &Stream) {
  if (ArrayType *Ty = dyn_cast<ArrayType>(C->getType())) {
    if (!Ty->getElementType()->isIntegerTy(8))
      report_fatal_error("Global array initializer not i8");
    uint32_t Size = Ty->getNumElements();
    if (isa<ConstantAggregateZero>(C)) {
      Vals.push_back(Size);
      Stream.EmitRecord(naclbitc::GLOBALVAR_ZEROFILL, Vals,
                        GLOBALVAR_ZEROFILL_ABBREV);
      Vals.clear();
    } else {
      const ConstantDataSequential *CD = cast<ConstantDataSequential>(C);
      StringRef Data = CD->getRawDataValues();
      for (size_t i = 0; i < Size; ++i) {
        Vals.push_back(Data[i] & 0xFF);
      }
      Stream.EmitRecord(naclbitc::GLOBALVAR_DATA, Vals,
                        GLOBALVAR_DATA_ABBREV);
      Vals.clear();
    }
    return;
  }
  if (VE.IsIntPtrType(C->getType())) {
    // This constant defines a relocation. Start by verifying the
    // relocation is of the right form.
    const ConstantExpr *CE = dyn_cast<ConstantExpr>(C);
    if (CE == 0)
      report_fatal_error("Global i32 initializer not constant");
    assert(CE);
    int32_t Addend = 0;
    if (CE->getOpcode() == Instruction::Add) {
      const ConstantInt *AddendConst = dyn_cast<ConstantInt>(CE->getOperand(1));
      if (AddendConst == 0)
        report_fatal_error("Malformed addend in global relocation initializer");
      Addend = AddendConst->getSExtValue();
      CE = dyn_cast<ConstantExpr>(CE->getOperand(0));
      if (CE == 0)
        report_fatal_error(
            "Base of global relocation initializer not constant");
    }
    if (CE->getOpcode() != Instruction::PtrToInt)
      report_fatal_error("Global relocation base doesn't contain ptrtoint");
    GlobalValue *GV = dyn_cast<GlobalValue>(CE->getOperand(0));
    if (GV == 0)
      report_fatal_error(
          "Argument of ptrtoint in global relocation no global value");

    // Now generate the corresponding relocation record.
    unsigned RelocID = VE.getValueID(GV);
    // This is a value index.
    unsigned AbbrevToUse = GLOBALVAR_RELOC_ABBREV;
    Vals.push_back(RelocID);
    if (Addend) {
      Vals.push_back(Addend);
      AbbrevToUse = GLOBALVAR_RELOC_WITH_ADDEND_ABBREV;
    }
    Stream.EmitRecord(naclbitc::GLOBALVAR_RELOC, Vals, AbbrevToUse);
    Vals.clear();
    return;
  }
  report_fatal_error("Global initializer is not a SimpleElement");
}

// Emit global variables.
static void WriteGlobalVars(const Module *M,
                            const NaClValueEnumerator &VE,
                            NaClBitstreamWriter &Stream) {
  Stream.EnterSubblock(naclbitc::GLOBALVAR_BLOCK_ID);
  SmallVector<uint32_t, 32> Vals;
  unsigned GlobalVarID = VE.getFirstGlobalVarID();

  // Emit the number of global variables.

  Vals.push_back(M->getGlobalList().size());
  Stream.EmitRecord(naclbitc::GLOBALVAR_COUNT, Vals);
  Vals.clear();

  // Now emit each global variable.
  for (Module::const_global_iterator
           GV = M->global_begin(), E = M->global_end();
       GV != E; ++GV, ++GlobalVarID) {
    // Define the global variable.
    Vals.push_back(Log2_32(GV->getAlignment()) + 1);
    Vals.push_back(GV->isConstant());
    Stream.EmitRecord(naclbitc::GLOBALVAR_VAR, Vals, GLOBALVAR_VAR_ABBREV);
    Vals.clear();

    // Add the field(s).
    const Constant *C = GV->getInitializer();
    if (C == 0)
      report_fatal_error("Global variable initializer not a constant");
    if (const ConstantStruct *CS = dyn_cast<ConstantStruct>(C)) {
      if (!CS->getType()->isPacked())
        report_fatal_error("Global variable type not packed");
      if (CS->getType()->hasName())
        report_fatal_error("Global variable type is named");
      Vals.push_back(CS->getNumOperands());
      Stream.EmitRecord(naclbitc::GLOBALVAR_COMPOUND, Vals,
                        GLOBALVAR_COMPOUND_ABBREV);
      Vals.clear();
      for (unsigned I = 0; I < CS->getNumOperands(); ++I) {
        WriteGlobalInit(dyn_cast<Constant>(CS->getOperand(I)), GlobalVarID,
                        Vals, VE, Stream);
      }
    } else {
      WriteGlobalInit(C, GlobalVarID, Vals, VE, Stream);
    }
  }

  assert(GlobalVarID == VE.getFirstGlobalVarID() + VE.getNumGlobalVarIDs());
  Stream.ExitBlock();
}

// Emit top-level description of module, including inline asm,
// descriptors for global variables, and function prototype info.
static void WriteModuleInfo(const Module *M, const NaClValueEnumerator &VE,
                            NaClBitstreamWriter &Stream) {
  DEBUG(dbgs() << "-> WriteModuleInfo\n");

  // Emit the function proto information. Note: We do this before
  // global variables, so that global variable initializations can
  // refer to the functions without a forward reference.
  SmallVector<unsigned, 64> Vals;
  for (Module::const_iterator F = M->begin(), E = M->end(); F != E; ++F) {
    // FUNCTION:  [type, callingconv, isproto, linkage]
    Type *Ty = F->getType()->getPointerElementType();
    Vals.push_back(VE.getTypeID(Ty));
    Vals.push_back(GetEncodedCallingConv(F->getCallingConv()));
    Vals.push_back(F->isDeclaration());
    Vals.push_back(getEncodedLinkage(F));

    unsigned AbbrevToUse = 0;
    Stream.EmitRecord(naclbitc::MODULE_CODE_FUNCTION, Vals, AbbrevToUse);
    Vals.clear();
  }

  // Emit the global variable information.
  WriteGlobalVars(M, VE, Stream);
  DEBUG(dbgs() << "<- WriteModuleInfo\n");
}

static void emitSignedInt64(SmallVectorImpl<uint64_t> &Vals, uint64_t V) {
  Vals.push_back(NaClEncodeSignRotatedValue((int64_t)V));
}

static void EmitAPInt(SmallVectorImpl<uint64_t> &Vals,
                      unsigned &Code, unsigned &AbbrevToUse, const APInt &Val) {
  if (Val.getBitWidth() <= 64) {
    uint64_t V = Val.getSExtValue();
    emitSignedInt64(Vals, V);
    Code = naclbitc::CST_CODE_INTEGER;
    AbbrevToUse =
        Val == 0 ? CONSTANTS_INTEGER_ZERO_ABBREV : CONSTANTS_INTEGER_ABBREV;
  } else {
    report_fatal_error("Wide integers are not supported");
  }
}

static void WriteConstants(unsigned FirstVal, unsigned LastVal,
                           const NaClValueEnumerator &VE,
                           NaClBitstreamWriter &Stream) {
  if (FirstVal == LastVal) return;

  Stream.EnterSubblock(naclbitc::CONSTANTS_BLOCK_ID, CONSTANTS_MAX_ABBREV);

  SmallVector<uint64_t, 64> Record;

  const NaClValueEnumerator::ValueList &Vals = VE.getValues();
  Type *LastTy = 0;
  for (unsigned i = FirstVal; i != LastVal; ++i) {
    const Value *V = Vals[i].first;
    // If we need to switch types, do so now.
    if (V->getType() != LastTy) {
      LastTy = V->getType();
      Record.push_back(VE.getTypeID(LastTy));
      Stream.EmitRecord(naclbitc::CST_CODE_SETTYPE, Record,
                        CONSTANTS_SETTYPE_ABBREV);
      Record.clear();
    }

    if (isa<InlineAsm>(V)) {
      ReportIllegalValue("inline assembly", *V);
    }
    const Constant *C = cast<Constant>(V);
    unsigned Code = -1U;
    unsigned AbbrevToUse = 0;
    if (isa<UndefValue>(C)) {
      Code = naclbitc::CST_CODE_UNDEF;
    } else if (const ConstantInt *IV = dyn_cast<ConstantInt>(C)) {
      EmitAPInt(Record, Code, AbbrevToUse, IV->getValue());
    } else if (const ConstantFP *CFP = dyn_cast<ConstantFP>(C)) {
      Code = naclbitc::CST_CODE_FLOAT;
      AbbrevToUse = CONSTANTS_FLOAT_ABBREV;
      Type *Ty = CFP->getType();
      if (Ty->isFloatTy() || Ty->isDoubleTy()) {
        Record.push_back(CFP->getValueAPF().bitcastToAPInt().getZExtValue());
      } else {
        report_fatal_error("Unknown FP type");
      }
    } else {
#ifndef NDEBUG
      C->dump();
#endif
      ReportIllegalValue("constant", *C);
    }
    Stream.EmitRecord(Code, Record, AbbrevToUse);
    Record.clear();
  }

  Stream.ExitBlock();
  DEBUG(dbgs() << "<- WriteConstants\n");
}

/// \brief Emits a type for the forward value reference. That is, if
/// the ID for the given value is larger than or equal to the BaseID,
/// the corresponding forward reference is generated.
static void EmitFnForwardTypeRef(const Value *V,
                                 unsigned BaseID,
                                 NaClValueEnumerator &VE,
                                 NaClBitstreamWriter &Stream) {
  unsigned ValID = VE.getValueID(V);
  if (ValID >= BaseID &&
      VE.InsertFnForwardTypeRef(ValID)) {
    SmallVector<unsigned, 2> Vals;
    Vals.push_back(ValID);
    Vals.push_back(VE.getTypeID(VE.NormalizeType(V->getType())));
    Stream.EmitRecord(naclbitc::FUNC_CODE_INST_FORWARDTYPEREF, Vals,
                      FUNCTION_INST_FORWARDTYPEREF_ABBREV);
  }
}

/// pushValue - The file has to encode both the value and type id for
/// many values, because we need to know what type to create for forward
/// references.  However, most operands are not forward references, so this type
/// field is not needed.
///
/// This function adds V's value ID to Vals.  If the value ID is higher than the
/// instruction ID, then it is a forward reference, and it also includes the
/// type ID.  The value ID that is written is encoded relative to the InstID.
static void pushValue(const Value *V, unsigned InstID,
                      SmallVector<unsigned, 64> &Vals,
                      NaClValueEnumerator &VE,
                      NaClBitstreamWriter &Stream) {
  const Value *VElided = VE.ElideCasts(V);
  EmitFnForwardTypeRef(VElided, InstID, VE, Stream);
  unsigned ValID = VE.getValueID(VElided);
  // Make encoding relative to the InstID.
  Vals.push_back(InstID - ValID);
}

static void pushValue64(const Value *V, unsigned InstID,
                        SmallVector<uint64_t, 128> &Vals,
                        NaClValueEnumerator &VE,
                        NaClBitstreamWriter &Stream) {
  const Value *VElided = VE.ElideCasts(V);
  EmitFnForwardTypeRef(VElided, InstID, VE, Stream);
  uint64_t ValID = VE.getValueID(VElided);
  Vals.push_back(InstID - ValID);
}

static void pushValueSigned(const Value *V, unsigned InstID,
                            SmallVector<uint64_t, 128> &Vals,
                            NaClValueEnumerator &VE,
                            NaClBitstreamWriter &Stream) {
  const Value *VElided = VE.ElideCasts(V);
  EmitFnForwardTypeRef(VElided, InstID, VE, Stream);
  unsigned ValID = VE.getValueID(VElided);
  int64_t diff = ((int32_t)InstID - (int32_t)ValID);
  emitSignedInt64(Vals, diff);
}

/// WriteInstruction - Emit an instruction to the specified stream.
/// Returns true if instruction actually emitted.
static bool WriteInstruction(const Instruction &I, unsigned InstID,
                             NaClValueEnumerator &VE,
                             NaClBitstreamWriter &Stream,
                             SmallVector<unsigned, 64> &Vals) {
  unsigned Code = 0;
  unsigned AbbrevToUse = 0;
  VE.setInstructionID(&I);
  switch (I.getOpcode()) {
  default:
    if (Instruction::isCast(I.getOpcode())) {
      // CAST:       [opval, destty, castopc]
      if (VE.IsElidedCast(&I))
        return false;
      Code = naclbitc::FUNC_CODE_INST_CAST;
      AbbrevToUse = FUNCTION_INST_CAST_ABBREV;
      pushValue(I.getOperand(0), InstID, Vals, VE, Stream);
      Vals.push_back(VE.getTypeID(I.getType()));
      unsigned Opcode = I.getOpcode();
      Vals.push_back(GetEncodedCastOpcode(Opcode, I));
      if (Opcode == Instruction::PtrToInt ||
          Opcode == Instruction::IntToPtr ||
          (Opcode == Instruction::BitCast &&
           (I.getOperand(0)->getType()->isPointerTy() ||
            I.getType()->isPointerTy()))) {
        ReportIllegalValue("(PNaCl ABI) pointer cast", I);
      }
    } else if (isa<BinaryOperator>(I)) {
      // BINOP:      [opval, opval, opcode]
      Code = naclbitc::FUNC_CODE_INST_BINOP;
      AbbrevToUse = FUNCTION_INST_BINOP_ABBREV;
      pushValue(I.getOperand(0), InstID, Vals, VE, Stream);
      pushValue(I.getOperand(1), InstID, Vals, VE, Stream);
      Vals.push_back(GetEncodedBinaryOpcode(I.getOpcode(), I));
    } else {
      ReportIllegalValue("instruction", I);
    }
    break;
  case Instruction::Select:
    Code = naclbitc::FUNC_CODE_INST_VSELECT;
    pushValue(I.getOperand(1), InstID, Vals, VE, Stream);
    pushValue(I.getOperand(2), InstID, Vals, VE, Stream);
    pushValue(I.getOperand(0), InstID, Vals, VE, Stream);
    break;
  case Instruction::ExtractElement:
    Code = naclbitc::FUNC_CODE_INST_EXTRACTELT;
    pushValue(I.getOperand(0), InstID, Vals, VE, Stream);
    pushValue(I.getOperand(1), InstID, Vals, VE, Stream);
    break;
  case Instruction::InsertElement:
    Code = naclbitc::FUNC_CODE_INST_INSERTELT;
    pushValue(I.getOperand(0), InstID, Vals, VE, Stream);
    pushValue(I.getOperand(1), InstID, Vals, VE, Stream);
    pushValue(I.getOperand(2), InstID, Vals, VE, Stream);
    break;
  case Instruction::ICmp:
  case Instruction::FCmp:
    // compare returning Int1Ty or vector of Int1Ty
    Code = naclbitc::FUNC_CODE_INST_CMP2;
    pushValue(I.getOperand(0), InstID, Vals, VE, Stream);
    pushValue(I.getOperand(1), InstID, Vals, VE, Stream);
    Vals.push_back(GetEncodedCmpPredicate(cast<CmpInst>(I)));
    break;

  case Instruction::Ret:
    {
      Code = naclbitc::FUNC_CODE_INST_RET;
      unsigned NumOperands = I.getNumOperands();
      if (NumOperands == 0)
        AbbrevToUse = FUNCTION_INST_RET_VOID_ABBREV;
      else if (NumOperands == 1) {
        pushValue(I.getOperand(0), InstID, Vals, VE, Stream);
        AbbrevToUse = FUNCTION_INST_RET_VAL_ABBREV;
      } else {
        for (unsigned i = 0, e = NumOperands; i != e; ++i)
          pushValue(I.getOperand(i), InstID, Vals, VE, Stream);
      }
    }
    break;
  case Instruction::Br:
    {
      Code = naclbitc::FUNC_CODE_INST_BR;
      const BranchInst &II = cast<BranchInst>(I);
      Vals.push_back(VE.getValueID(II.getSuccessor(0)));
      if (II.isConditional()) {
        Vals.push_back(VE.getValueID(II.getSuccessor(1)));
        pushValue(II.getCondition(), InstID, Vals, VE, Stream);
      }
    }
    break;
  case Instruction::Switch:
    {
      // Redefine Vals, since here we need to use 64 bit values
      // explicitly to store large APInt numbers.
      SmallVector<uint64_t, 128> Vals64;

      Code = naclbitc::FUNC_CODE_INST_SWITCH;
      const SwitchInst &SI = cast<SwitchInst>(I);

      Vals64.push_back(VE.getTypeID(SI.getCondition()->getType()));
      pushValue64(SI.getCondition(), InstID, Vals64, VE, Stream);
      Vals64.push_back(VE.getValueID(SI.getDefaultDest()));
      Vals64.push_back(SI.getNumCases());
      for (SwitchInst::ConstCaseIt i = SI.case_begin(), e = SI.case_end();
           i != e; ++i) {
        // The PNaCl bitcode format has vestigial support for case
        // ranges, but we no longer support reading or writing them,
        // so the next two fields always have the same values.
        // See https://code.google.com/p/nativeclient/issues/detail?id=3758
        Vals64.push_back(1/*NumItems = 1*/);
        Vals64.push_back(true/*IsSingleNumber = true*/);

        emitSignedInt64(Vals64, i.getCaseValue()->getSExtValue());
        Vals64.push_back(VE.getValueID(i.getCaseSuccessor()));
      }

      Stream.EmitRecord(Code, Vals64, AbbrevToUse);

      // Also do expected action - clear external Vals collection:
      Vals.clear();
      return true;
    }
    break;
  case Instruction::Unreachable:
    Code = naclbitc::FUNC_CODE_INST_UNREACHABLE;
    AbbrevToUse = FUNCTION_INST_UNREACHABLE_ABBREV;
    break;

  case Instruction::PHI: {
    const PHINode &PN = cast<PHINode>(I);
    Code = naclbitc::FUNC_CODE_INST_PHI;
    // With the newer instruction encoding, forward references could give
    // negative valued IDs.  This is most common for PHIs, so we use
    // signed VBRs.
    SmallVector<uint64_t, 128> Vals64;
    Vals64.push_back(VE.getTypeID(PN.getType()));
    for (unsigned i = 0, e = PN.getNumIncomingValues(); i != e; ++i) {
      pushValueSigned(PN.getIncomingValue(i), InstID, Vals64, VE, Stream);
      Vals64.push_back(VE.getValueID(PN.getIncomingBlock(i)));
    }
    // Emit a Vals64 vector and exit.
    Stream.EmitRecord(Code, Vals64, AbbrevToUse);
    Vals64.clear();
    return true;
  }

  case Instruction::Alloca:
    if (!cast<AllocaInst>(&I)->getAllocatedType()->isIntegerTy(8))
      report_fatal_error("Type of alloca instruction is not i8");
    Code = naclbitc::FUNC_CODE_INST_ALLOCA;
    pushValue(I.getOperand(0), InstID, Vals, VE, Stream); // size.
    Vals.push_back(Log2_32(cast<AllocaInst>(I).getAlignment())+1);
    break;
  case Instruction::Load:
    // LOAD: [op, align, ty]
    Code = naclbitc::FUNC_CODE_INST_LOAD;
    pushValue(I.getOperand(0), InstID, Vals, VE, Stream);
    AbbrevToUse = FUNCTION_INST_LOAD_ABBREV;
    Vals.push_back(Log2_32(cast<LoadInst>(I).getAlignment())+1);
    Vals.push_back(VE.getTypeID(I.getType()));
    break;
  case Instruction::Store:
    // STORE: [ptr, val, align]
    Code = naclbitc::FUNC_CODE_INST_STORE;
    AbbrevToUse = FUNCTION_INST_STORE_ABBREV;
    pushValue(I.getOperand(1), InstID, Vals, VE, Stream);
    pushValue(I.getOperand(0), InstID, Vals, VE, Stream);
    Vals.push_back(Log2_32(cast<StoreInst>(I).getAlignment())+1);
    break;
  case Instruction::Call: {
    // CALL: [cc, fnid, args...]
    // CALL_INDIRECT: [cc, fnid, fnty, args...]

    const CallInst &Call = cast<CallInst>(I);
    const Value* Callee = Call.getCalledValue();
    Vals.push_back((GetEncodedCallingConv(Call.getCallingConv()) << 1)
                   | unsigned(Call.isTailCall()));

    pushValue(Callee, InstID, Vals, VE, Stream);

    if (Callee == VE.ElideCasts(Callee)) {
      // Since the call pointer has not been elided, we know that
      // the call pointer has the type signature of the called
      // function.  This implies that the reader can use the type
      // signature of the callee to figure out how to add casts to
      // the arguments.
      Code = naclbitc::FUNC_CODE_INST_CALL;
    } else {
      // If the cast was elided, a pointer conversion to a pointer
      // was applied, meaning that this is an indirect call. For the
      // reader, this implies that we can't use the type signature
      // of the callee to resolve elided call arguments, since it is
      // not known. Hence, we must send the type signature to the
      // reader.
      Code = naclbitc::FUNC_CODE_INST_CALL_INDIRECT;
      Vals.push_back(VE.getTypeID(I.getType()));
    }

    for (unsigned I = 0, E = Call.getNumArgOperands(); I < E; ++I) {
      pushValue(Call.getArgOperand(I), InstID, Vals, VE, Stream);
    }
    break;
  }
  }

  Stream.EmitRecord(Code, Vals, AbbrevToUse);
  Vals.clear();
  return true;
}

// Emit names for globals/functions etc.
static void WriteValueSymbolTable(const ValueSymbolTable &VST,
                                  const NaClValueEnumerator &VE,
                                  NaClBitstreamWriter &Stream) {
  if (VST.empty()) return;
  Stream.EnterSubblock(naclbitc::VALUE_SYMTAB_BLOCK_ID);

  // FIXME: Set up the abbrev, we know how many values there are!
  // FIXME: We know if the type names can use 7-bit ascii.
  SmallVector<unsigned, 64> NameVals;

  for (ValueSymbolTable::const_iterator SI = VST.begin(), SE = VST.end();
       SI != SE; ++SI) {
    if (VE.IsElidedCast(SI->getValue())) continue;

    const ValueName &Name = *SI;

    // Figure out the encoding to use for the name.
    bool is7Bit = true;
    bool isChar6 = true;
    for (const char *C = Name.getKeyData(), *E = C+Name.getKeyLength();
         C != E; ++C) {
      if (isChar6)
        isChar6 = NaClBitCodeAbbrevOp::isChar6(*C);
      if ((unsigned char)*C & 128) {
        is7Bit = false;
        break;  // don't bother scanning the rest.
      }
    }

    unsigned AbbrevToUse = VST_ENTRY_8_ABBREV;

    // VST_ENTRY:   [valueid, namechar x N]
    // VST_BBENTRY: [bbid, namechar x N]
    unsigned Code;
    if (isa<BasicBlock>(SI->getValue())) {
      Code = naclbitc::VST_CODE_BBENTRY;
      if (isChar6)
        AbbrevToUse = VST_BBENTRY_6_ABBREV;
    } else {
      Code = naclbitc::VST_CODE_ENTRY;
      if (isChar6)
        AbbrevToUse = VST_ENTRY_6_ABBREV;
      else if (is7Bit)
        AbbrevToUse = VST_ENTRY_7_ABBREV;
    }

    NameVals.push_back(VE.getValueID(SI->getValue()));
    for (const char *P = Name.getKeyData(),
         *E = Name.getKeyData()+Name.getKeyLength(); P != E; ++P)
      NameVals.push_back((unsigned char)*P);

    // Emit the finished record.
    Stream.EmitRecord(Code, NameVals, AbbrevToUse);
    NameVals.clear();
  }
  Stream.ExitBlock();
}

/// WriteFunction - Emit a function body to the module stream.
static void WriteFunction(const Function &F, NaClValueEnumerator &VE,
                          NaClBitstreamWriter &Stream) {
  Stream.EnterSubblock(naclbitc::FUNCTION_BLOCK_ID);
  VE.incorporateFunction(F);

  SmallVector<unsigned, 64> Vals;

  // Emit the number of basic blocks, so the reader can create them ahead of
  // time.
  Vals.push_back(VE.getBasicBlocks().size());
  Stream.EmitRecord(naclbitc::FUNC_CODE_DECLAREBLOCKS, Vals);
  Vals.clear();

  // If there are function-local constants, emit them now.
  unsigned CstStart, CstEnd;
  VE.getFunctionConstantRange(CstStart, CstEnd);
  WriteConstants(CstStart, CstEnd, VE, Stream);

  // Keep a running idea of what the instruction ID is.
  unsigned InstID = CstEnd;

  // Finally, emit all the instructions, in order.
  for (Function::const_iterator BB = F.begin(), E = F.end(); BB != E; ++BB)
    for (BasicBlock::const_iterator I = BB->begin(), E = BB->end();
         I != E; ++I) {
      if (WriteInstruction(*I, InstID, VE, Stream, Vals) &&
          !I->getType()->isVoidTy())
        ++InstID;
    }

  // Emit names for instructions etc.
  if (PNaClAllowLocalSymbolTables)
    WriteValueSymbolTable(F.getValueSymbolTable(), VE, Stream);

  VE.purgeFunction();
  Stream.ExitBlock();
}

// Emit blockinfo, which defines the standard abbreviations etc.
static void WriteBlockInfo(const NaClValueEnumerator &VE,
                           NaClBitstreamWriter &Stream) {
  // We only want to emit block info records for blocks that have multiple
  // instances: CONSTANTS_BLOCK, FUNCTION_BLOCK and VALUE_SYMTAB_BLOCK.
  // Other blocks can define their abbrevs inline.
  Stream.EnterBlockInfoBlock();

  { // 8-bit fixed-width VST_ENTRY/VST_BBENTRY strings.
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, 3));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 8));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Array));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, 8));
    if (Stream.EmitBlockInfoAbbrev(naclbitc::VALUE_SYMTAB_BLOCK_ID,
                                   Abbv) != VST_ENTRY_8_ABBREV)
      llvm_unreachable("Unexpected abbrev ordering!");
  }

  { // 7-bit fixed width VST_ENTRY strings.
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::VST_CODE_ENTRY));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 8));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Array));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, 7));
    if (Stream.EmitBlockInfoAbbrev(naclbitc::VALUE_SYMTAB_BLOCK_ID,
                                   Abbv) != VST_ENTRY_7_ABBREV)
      llvm_unreachable("Unexpected abbrev ordering!");
  }
  { // 6-bit char6 VST_ENTRY strings.
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::VST_CODE_ENTRY));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 8));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Array));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Char6));
    if (Stream.EmitBlockInfoAbbrev(naclbitc::VALUE_SYMTAB_BLOCK_ID,
                                   Abbv) != VST_ENTRY_6_ABBREV)
      llvm_unreachable("Unexpected abbrev ordering!");
  }
  { // 6-bit char6 VST_BBENTRY strings.
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::VST_CODE_BBENTRY));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 8));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Array));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Char6));
    if (Stream.EmitBlockInfoAbbrev(naclbitc::VALUE_SYMTAB_BLOCK_ID,
                                   Abbv) != VST_BBENTRY_6_ABBREV)
      llvm_unreachable("Unexpected abbrev ordering!");
  }



  { // SETTYPE abbrev for CONSTANTS_BLOCK.
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::CST_CODE_SETTYPE));
    Abbv->Add(NaClBitCodeAbbrevOp(TypeIdEncoding, TypeIdNumBits));
    if (Stream.EmitBlockInfoAbbrev(naclbitc::CONSTANTS_BLOCK_ID,
                                   Abbv) != CONSTANTS_SETTYPE_ABBREV)
      llvm_unreachable("Unexpected abbrev ordering!");
  }

  { // INTEGER abbrev for CONSTANTS_BLOCK.
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::CST_CODE_INTEGER));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 8));
    if (Stream.EmitBlockInfoAbbrev(naclbitc::CONSTANTS_BLOCK_ID,
                                   Abbv) != CONSTANTS_INTEGER_ABBREV)
      llvm_unreachable("Unexpected abbrev ordering!");
  }
  { // INTEGER_ZERO abbrev for CONSTANTS_BLOCK.
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::CST_CODE_INTEGER));
    Abbv->Add(NaClBitCodeAbbrevOp(0));
    if (Stream.EmitBlockInfoAbbrev(naclbitc::CONSTANTS_BLOCK_ID,
                                   Abbv) != CONSTANTS_INTEGER_ZERO_ABBREV)
      llvm_unreachable("Unexpected abbrev ordering!");
  }
  { // FLOAT abbrev for CONSTANTS_BLOCK.
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::CST_CODE_FLOAT));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 8));
    if (Stream.EmitBlockInfoAbbrev(naclbitc::CONSTANTS_BLOCK_ID,
                                   Abbv) != CONSTANTS_FLOAT_ABBREV)
      llvm_unreachable("Unexpected abbrev ordering!");
  }

  // FIXME: This should only use space for first class types!

  { // INST_LOAD abbrev for FUNCTION_BLOCK.
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::FUNC_CODE_INST_LOAD));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6)); // Ptr
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 4)); // Align
    // Note: The vast majority of load operations are only on integers
    // and floats. In addition, no function types are allowed. In
    // addition, the type IDs have been sorted based on usage, moving
    // type IDs associated integers and floats to very low
    // indices. Hence, we assume that we can use a smaller width for
    // the typecast.
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 4)); // TypeCast
    if (Stream.EmitBlockInfoAbbrev(naclbitc::FUNCTION_BLOCK_ID,
                                   Abbv) != FUNCTION_INST_LOAD_ABBREV)
      llvm_unreachable("Unexpected abbrev ordering!");
  }
  { // INST_BINOP abbrev for FUNCTION_BLOCK.
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::FUNC_CODE_INST_BINOP));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6)); // LHS
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6)); // RHS
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, 4)); // opc
    if (Stream.EmitBlockInfoAbbrev(naclbitc::FUNCTION_BLOCK_ID,
                                   Abbv) != FUNCTION_INST_BINOP_ABBREV)
      llvm_unreachable("Unexpected abbrev ordering!");
  }
  { // INST_CAST abbrev for FUNCTION_BLOCK.
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::FUNC_CODE_INST_CAST));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));    // OpVal
    Abbv->Add(NaClBitCodeAbbrevOp(TypeIdEncoding, TypeIdNumBits));  // dest ty
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, 4));  // opc
    if (Stream.EmitBlockInfoAbbrev(naclbitc::FUNCTION_BLOCK_ID,
                                   Abbv) != FUNCTION_INST_CAST_ABBREV)
      llvm_unreachable("Unexpected abbrev ordering!");
  }

  { // INST_RET abbrev for FUNCTION_BLOCK.
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::FUNC_CODE_INST_RET));
    if (Stream.EmitBlockInfoAbbrev(naclbitc::FUNCTION_BLOCK_ID,
                                   Abbv) != FUNCTION_INST_RET_VOID_ABBREV)
      llvm_unreachable("Unexpected abbrev ordering!");
  }
  { // INST_RET abbrev for FUNCTION_BLOCK.
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::FUNC_CODE_INST_RET));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6)); // ValID
    if (Stream.EmitBlockInfoAbbrev(naclbitc::FUNCTION_BLOCK_ID,
                                   Abbv) != FUNCTION_INST_RET_VAL_ABBREV)
      llvm_unreachable("Unexpected abbrev ordering!");
  }
  { // INST_UNREACHABLE abbrev for FUNCTION_BLOCK.
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::FUNC_CODE_INST_UNREACHABLE));
    if (Stream.EmitBlockInfoAbbrev(naclbitc::FUNCTION_BLOCK_ID,
                                   Abbv) != FUNCTION_INST_UNREACHABLE_ABBREV)
      llvm_unreachable("Unexpected abbrev ordering!");
  }
  { // INST_FORWARDTYPEREF abbrev for FUNCTION_BLOCK.
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::FUNC_CODE_INST_FORWARDTYPEREF));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));
    Abbv->Add(NaClBitCodeAbbrevOp(TypeIdEncoding, TypeIdNumBits));
    if (Stream.EmitBlockInfoAbbrev(naclbitc::FUNCTION_BLOCK_ID,
                                   Abbv) != FUNCTION_INST_FORWARDTYPEREF_ABBREV)
      llvm_unreachable("Unexpected abbrev ordering!");
  }
  { // INST_STORE abbrev for FUNCTION_BLOCK.
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::FUNC_CODE_INST_STORE));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6)); // Ptr
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6)); // Value
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 4)); // Align
    if (Stream.EmitBlockInfoAbbrev(naclbitc::FUNCTION_BLOCK_ID,
                                   Abbv) != FUNCTION_INST_STORE_ABBREV)
      llvm_unreachable("Unexpected abbrev ordering!");
  }

  { // VAR abbrev for GLOBALVAR_BLOCK.
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::GLOBALVAR_VAR));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, 1));
    if (Stream.EmitBlockInfoAbbrev(naclbitc::GLOBALVAR_BLOCK_ID,
                                   Abbv) != GLOBALVAR_VAR_ABBREV)
      llvm_unreachable("Unexpected abbrev ordering!");
  }
  { // COMPOUND abbrev for GLOBALVAR_BLOCK.
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::GLOBALVAR_COMPOUND));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 8));
    if (Stream.EmitBlockInfoAbbrev(naclbitc::GLOBALVAR_BLOCK_ID,
                                   Abbv) != GLOBALVAR_COMPOUND_ABBREV)
      llvm_unreachable("Unexpected abbrev ordering!");
  }
  { // ZEROFILL abbrev for GLOBALVAR_BLOCK.
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::GLOBALVAR_ZEROFILL));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 8));
    if (Stream.EmitBlockInfoAbbrev(naclbitc::GLOBALVAR_BLOCK_ID,
                                   Abbv) != GLOBALVAR_ZEROFILL_ABBREV)
      llvm_unreachable("Unexpected abbrev ordering!");
  }
  { // DATA abbrev for GLOBALVAR_BLOCK.
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::GLOBALVAR_DATA));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Array));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, 8));
    if (Stream.EmitBlockInfoAbbrev(naclbitc::GLOBALVAR_BLOCK_ID,
                                   Abbv) != GLOBALVAR_DATA_ABBREV)
      llvm_unreachable("Unexpected abbrev ordering!");
  }
  { // RELOC abbrev for GLOBALVAR_BLOCK.
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::GLOBALVAR_RELOC));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));
    if (Stream.EmitBlockInfoAbbrev(naclbitc::GLOBALVAR_BLOCK_ID,
                                   Abbv) != GLOBALVAR_RELOC_ABBREV)
      llvm_unreachable("Unexpected abbrev ordering!");
  }
  { // RELOC_WITH_ADDEND_ABBREV abbrev for GLOBALVAR_BLOCK.
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::GLOBALVAR_RELOC));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));
    if (Stream.EmitBlockInfoAbbrev(
            naclbitc::GLOBALVAR_BLOCK_ID,
            Abbv) != GLOBALVAR_RELOC_WITH_ADDEND_ABBREV)
      llvm_unreachable("Unexpected abbrev ordering!");
  }

  Stream.ExitBlock();
}

/// WriteModule - Emit the specified module to the bitstream.
static void WriteModule(const Module *M, NaClBitstreamWriter &Stream) {
  DEBUG(dbgs() << "-> WriteModule\n");
  Stream.EnterSubblock(naclbitc::MODULE_BLOCK_ID);

  SmallVector<unsigned, 1> Vals;
  unsigned CurVersion = 1;
  Vals.push_back(CurVersion);
  Stream.EmitRecord(naclbitc::MODULE_CODE_VERSION, Vals);

  // Analyze the module, enumerating globals, functions, etc.
  NaClValueEnumerator VE(M);
  OptimizeTypeIdEncoding(VE);

  // Emit blockinfo, which defines the standard abbreviations etc.
  WriteBlockInfo(VE, Stream);

  // Emit information describing all of the types in the module.
  WriteTypeTable(VE, Stream);

  // Emit top-level description of module, including inline asm,
  // descriptors for global variables, and function prototype info.
  WriteModuleInfo(M, VE, Stream);

  // Emit names for globals/functions etc.
  WriteValueSymbolTable(M->getValueSymbolTable(), VE, Stream);

  // Emit function bodies.
  for (Module::const_iterator F = M->begin(), E = M->end(); F != E; ++F)
    if (!F->isDeclaration())
      WriteFunction(*F, VE, Stream);

  Stream.ExitBlock();
  DEBUG(dbgs() << "<- WriteModule\n");
}

// Max size for variable fields. Currently only used for writing them
// out to files (the parsing works for arbitrary sizes).
static const size_t kMaxVariableFieldSize = 256;

void llvm::NaClWriteHeader(NaClBitstreamWriter &Stream,
                           bool AcceptSupportedOnly) {
  NaClBitcodeHeader Header;
  Header.push_back(
      new NaClBitcodeHeaderField(NaClBitcodeHeaderField::kPNaClVersion,
                                 PNaClVersion));
  if (AlignBitcodeRecords)
    Header.push_back(new NaClBitcodeHeaderField(
        NaClBitcodeHeaderField::kAlignBitcodeRecords));

  Header.InstallFields();
  if (!(Header.IsSupported() ||
        (!AcceptSupportedOnly && Header.IsReadable()))) {
    report_fatal_error(Header.Unsupported());
  }
  NaClWriteHeader(Header, Stream);
}

// Write out the given Header to the bitstream.
void llvm::NaClWriteHeader(const NaClBitcodeHeader &Header,
                           NaClBitstreamWriter &Stream) {
  Stream.initFromHeader(Header);

  // Emit the file magic number;
  Stream.Emit((unsigned)'P', 8);
  Stream.Emit((unsigned)'E', 8);
  Stream.Emit((unsigned)'X', 8);
  Stream.Emit((unsigned)'E', 8);

  // Emit placeholder for number of bytes used to hold header fields.
  // This value is necessary so that the streamable reader can preallocate
  // a buffer to read the fields.
  Stream.Emit(0, naclbitc::BlockSizeWidth);
  unsigned BytesForHeader = 0;

  unsigned NumberFields = Header.NumberFields();
  if (NumberFields > 0xFFFF)
    report_fatal_error("Too many header fields");

  uint8_t Buffer[kMaxVariableFieldSize];
  for (unsigned F = 0; F < NumberFields; ++F) {
    NaClBitcodeHeaderField *Field = Header.GetField(F);
    if (!Field->Write(Buffer, kMaxVariableFieldSize))
      report_fatal_error("Header field too big to generate");
    size_t limit = Field->GetTotalSize();
    for (size_t i = 0; i < limit; i++) {
      Stream.Emit(Buffer[i], 8);
    }
    BytesForHeader += limit;
  }

  if (BytesForHeader > 0xFFFF)
    report_fatal_error("Header fields to big to save");

  // Encode #fields in top two bytes, and #bytes to hold fields in
  // bottom two bytes. Then backpatch into second word.
  unsigned Value = NumberFields | (BytesForHeader << 16);
  Stream.BackpatchWord(NaClBitcodeHeader::WordSize, Value);
}

/// WriteBitcodeToFile - Write the specified module to the specified output
/// stream.
void llvm::NaClWriteBitcodeToFile(const Module *M, raw_ostream &Out,
                                  bool AcceptSupportedOnly) {
  SmallVector<char, 0> Buffer;
  Buffer.reserve(256*1024);

  // Emit the module into the buffer.
  {
    NaClBitstreamWriter Stream(Buffer);
    NaClWriteHeader(Stream, AcceptSupportedOnly);
    WriteModule(M, Stream);
  }

  // Write the generated bitstream to "Out".
  Out.write((char*)&Buffer.front(), Buffer.size());
}
