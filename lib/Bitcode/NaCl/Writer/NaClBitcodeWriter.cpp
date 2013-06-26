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
  CONSTANTS_CE_CAST_Abbrev,
  CONSTANTS_NULL_Abbrev,
  CONSTANTS_MAX_ABBREV = CONSTANTS_NULL_Abbrev,

  // CONSTANTS_BLOCK abbrev id's when global (extends list above).
  CST_CONSTANTS_AGGREGATE_ABBREV = CONSTANTS_MAX_ABBREV+1,
  CST_CONSTANTS_STRING_ABBREV,
  CST_CONSTANTS_CSTRING_7_ABBREV,
  CST_CONSTANTS_CSTRING_6_ABBREV,
  CST_CONSTANTS_MAX_ABBREV = CST_CONSTANTS_CSTRING_6_ABBREV,

  // FUNCTION_BLOCK abbrev id's.
  FUNCTION_INST_LOAD_ABBREV = naclbitc::FIRST_APPLICATION_ABBREV,
  FUNCTION_INST_BINOP_ABBREV,
  FUNCTION_INST_BINOP_FLAGS_ABBREV,
  FUNCTION_INST_CAST_ABBREV,
  FUNCTION_INST_RET_VOID_ABBREV,
  FUNCTION_INST_RET_VAL_ABBREV,
  FUNCTION_INST_UNREACHABLE_ABBREV,
  FUNCTION_INST_FORWARDTYPEREF_ABBREV,
  FUNCTION_INST_MAX_ABBREV = FUNCTION_INST_FORWARDTYPEREF_ABBREV,

  // TYPE_BLOCK_ID_NEW abbrev id's.
  TYPE_POINTER_ABBREV = naclbitc::FIRST_APPLICATION_ABBREV,
  TYPE_FUNCTION_ABBREV,
  TYPE_STRUCT_ANON_ABBREV,
  TYPE_STRUCT_NAME_ABBREV,
  TYPE_STRUCT_NAMED_ABBREV,
  TYPE_ARRAY_ABBREV,
  TYPE_MAX_ABBREV = TYPE_ARRAY_ABBREV,

  // META_DATA_BLOCK abbrev id's.
  METADATA_STRING_ABBREV = naclbitc::FIRST_APPLICATION_ABBREV,
  METADATA_MAX_ABBREV = METADATA_STRING_ABBREV,

  // MODULE_BLOCK abbrev id's.
  MODULE_GLOBALVAR_ABBREV = naclbitc::FIRST_APPLICATION_ABBREV,
  MODULE_MAX_ABBREV = MODULE_GLOBALVAR_ABBREV,

  // SwitchInst Magic
  SWITCH_INST_MAGIC = 0x4B5 // May 2012 => 1205 => Hex
};

static unsigned GetEncodedCastOpcode(unsigned Opcode) {
  switch (Opcode) {
  default: report_fatal_error("Unknown cast instruction!");
  case Instruction::Trunc   : return naclbitc::CAST_TRUNC;
  case Instruction::ZExt    : return naclbitc::CAST_ZEXT;
  case Instruction::SExt    : return naclbitc::CAST_SEXT;
  case Instruction::FPToUI  : return naclbitc::CAST_FPTOUI;
  case Instruction::FPToSI  : return naclbitc::CAST_FPTOSI;
  case Instruction::UIToFP  : return naclbitc::CAST_UITOFP;
  case Instruction::SIToFP  : return naclbitc::CAST_SITOFP;
  case Instruction::FPTrunc : return naclbitc::CAST_FPTRUNC;
  case Instruction::FPExt   : return naclbitc::CAST_FPEXT;
  case Instruction::PtrToInt: return naclbitc::CAST_PTRTOINT;
  case Instruction::IntToPtr: return naclbitc::CAST_INTTOPTR;
  case Instruction::BitCast : return naclbitc::CAST_BITCAST;
  }
}

static unsigned GetEncodedBinaryOpcode(unsigned Opcode) {
  switch (Opcode) {
  default: report_fatal_error("Unknown binary instruction!");
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

static unsigned GetEncodedRMWOperation(AtomicRMWInst::BinOp Op) {
  switch (Op) {
  default: report_fatal_error("Unknown RMW operation!");
  case AtomicRMWInst::Xchg: return naclbitc::RMW_XCHG;
  case AtomicRMWInst::Add: return naclbitc::RMW_ADD;
  case AtomicRMWInst::Sub: return naclbitc::RMW_SUB;
  case AtomicRMWInst::And: return naclbitc::RMW_AND;
  case AtomicRMWInst::Nand: return naclbitc::RMW_NAND;
  case AtomicRMWInst::Or: return naclbitc::RMW_OR;
  case AtomicRMWInst::Xor: return naclbitc::RMW_XOR;
  case AtomicRMWInst::Max: return naclbitc::RMW_MAX;
  case AtomicRMWInst::Min: return naclbitc::RMW_MIN;
  case AtomicRMWInst::UMax: return naclbitc::RMW_UMAX;
  case AtomicRMWInst::UMin: return naclbitc::RMW_UMIN;
  }
}

static unsigned GetEncodedOrdering(AtomicOrdering Ordering) {
  switch (Ordering) {
  default: report_fatal_error("Invalid ordering");
  case NotAtomic: return naclbitc::ORDERING_NOTATOMIC;
  case Unordered: return naclbitc::ORDERING_UNORDERED;
  case Monotonic: return naclbitc::ORDERING_MONOTONIC;
  case Acquire: return naclbitc::ORDERING_ACQUIRE;
  case Release: return naclbitc::ORDERING_RELEASE;
  case AcquireRelease: return naclbitc::ORDERING_ACQREL;
  case SequentiallyConsistent: return naclbitc::ORDERING_SEQCST;
  }
}

static unsigned GetEncodedSynchScope(SynchronizationScope SynchScope) {
  switch (SynchScope) {
  default: report_fatal_error("Invalid synch scope");
  case SingleThread: return naclbitc::SYNCHSCOPE_SINGLETHREAD;
  case CrossThread: return naclbitc::SYNCHSCOPE_CROSSTHREAD;
  }
}

static unsigned GetEncodedCallingConv(CallingConv::ID conv) {
  switch (conv) {
  default: report_fatal_error(
      "Calling convention not supported by PNaCL bitcode");
  case CallingConv::C: return naclbitc::C_CallingConv;
  }
}

static void WriteStringRecord(unsigned Code, StringRef Str,
                              unsigned AbbrevToUse,
                              NaClBitstreamWriter &Stream) {
  SmallVector<unsigned, 64> Vals;

  // Code: [strchar x N]
  for (unsigned i = 0, e = Str.size(); i != e; ++i) {
    if (AbbrevToUse && !NaClBitCodeAbbrevOp::isChar6(Str[i]))
      AbbrevToUse = 0;
    Vals.push_back(Str[i]);
  }

  // Emit the finished record.
  Stream.EmitRecord(Code, Vals, AbbrevToUse);
}

/// WriteTypeTable - Write out the type table for a module.
static void WriteTypeTable(const NaClValueEnumerator &VE,
                           NaClBitstreamWriter &Stream) {
  DEBUG(dbgs() << "-> WriteTypeTable\n");
  const NaClValueEnumerator::TypeList &TypeList = VE.getTypes();

  Stream.EnterSubblock(naclbitc::TYPE_BLOCK_ID_NEW, TYPE_MAX_ABBREV);

  SmallVector<uint64_t, 64> TypeVals;


  // Note: modify to use maximum number of bits if under cutoff. Otherwise,
  // use VBR to take advantage that frequently referenced types have
  // small IDs.
  //
  // Note: Cutoff chosen based on experiments on pnacl-translate.pexe.
  uint64_t NumBits = NaClBitsNeededForValue(VE.getTypes().size());
  static const uint64_t TypeVBRCutoff = 6;
  uint64_t TypeIdNumBits = (NumBits <= TypeVBRCutoff ? NumBits : TypeVBRCutoff);
  NaClBitCodeAbbrevOp::Encoding TypeIdEncoding =
      (NumBits <= TypeVBRCutoff
       ? NaClBitCodeAbbrevOp::Fixed : NaClBitCodeAbbrevOp::VBR);

  // Abbrev for TYPE_CODE_POINTER.
  NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
  Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::TYPE_CODE_POINTER));
  Abbv->Add(NaClBitCodeAbbrevOp(TypeIdEncoding, TypeIdNumBits));
  Abbv->Add(NaClBitCodeAbbrevOp(0));  // Addrspace = 0
  if (TYPE_POINTER_ABBREV != Stream.EmitAbbrev(Abbv))
    llvm_unreachable("Unexpected abbrev ordering!");

  // Abbrev for TYPE_CODE_FUNCTION.
  Abbv = new NaClBitCodeAbbrev();
  Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::TYPE_CODE_FUNCTION));
  Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, 1));  // isvararg
  Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Array));
  Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, NumBits));
  if (TYPE_FUNCTION_ABBREV != Stream.EmitAbbrev(Abbv))
    llvm_unreachable("Unexpected abbrev ordering!");

  // Abbrev for TYPE_CODE_STRUCT_ANON.
  Abbv = new NaClBitCodeAbbrev();
  Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::TYPE_CODE_STRUCT_ANON));
  Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, 1));  // ispacked
  Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Array));
  Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, NumBits));
  if (TYPE_STRUCT_ANON_ABBREV != Stream.EmitAbbrev(Abbv))
    llvm_unreachable("Unexpected abbrev ordering!");

  // Abbrev for TYPE_CODE_STRUCT_NAME.
  Abbv = new NaClBitCodeAbbrev();
  Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::TYPE_CODE_STRUCT_NAME));
  Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Array));
  Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Char6));
  if (TYPE_STRUCT_NAME_ABBREV != Stream.EmitAbbrev(Abbv))
    llvm_unreachable("Unexpected abbrev ordering!");

  // Abbrev for TYPE_CODE_STRUCT_NAMED.
  Abbv = new NaClBitCodeAbbrev();
  Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::TYPE_CODE_STRUCT_NAMED));
  Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, 1));  // ispacked
  Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Array));
  Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, NumBits));
  if (TYPE_STRUCT_NAMED_ABBREV != Stream.EmitAbbrev(Abbv))
    llvm_unreachable("Unexpected abbrev ordering!");

  // Abbrev for TYPE_CODE_ARRAY.
  Abbv = new NaClBitCodeAbbrev();
  Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::TYPE_CODE_ARRAY));
  Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 8));   // size
  Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, NumBits));
  if (TYPE_ARRAY_ABBREV != Stream.EmitAbbrev(Abbv))
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
    case Type::HalfTyID:      Code = naclbitc::TYPE_CODE_HALF;      break;
    case Type::FloatTyID:     Code = naclbitc::TYPE_CODE_FLOAT;     break;
    case Type::DoubleTyID:    Code = naclbitc::TYPE_CODE_DOUBLE;    break;
    case Type::X86_FP80TyID:  Code = naclbitc::TYPE_CODE_X86_FP80;  break;
    case Type::FP128TyID:     Code = naclbitc::TYPE_CODE_FP128;     break;
    case Type::PPC_FP128TyID: Code = naclbitc::TYPE_CODE_PPC_FP128; break;
    case Type::LabelTyID:     Code = naclbitc::TYPE_CODE_LABEL;     break;
    case Type::MetadataTyID:  Code = naclbitc::TYPE_CODE_METADATA;  break;
    case Type::X86_MMXTyID:   Code = naclbitc::TYPE_CODE_X86_MMX;   break;
    case Type::IntegerTyID:
      // INTEGER: [width]
      Code = naclbitc::TYPE_CODE_INTEGER;
      TypeVals.push_back(cast<IntegerType>(T)->getBitWidth());
      break;
    case Type::PointerTyID: {
      PointerType *PTy = cast<PointerType>(T);
      // POINTER: [pointee type, address space]
      Code = naclbitc::TYPE_CODE_POINTER;
      TypeVals.push_back(VE.getTypeID(PTy->getElementType()));
      unsigned AddressSpace = PTy->getAddressSpace();
      TypeVals.push_back(AddressSpace);
      if (AddressSpace == 0) AbbrevToUse = TYPE_POINTER_ABBREV;
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
    case Type::StructTyID: {
      StructType *ST = cast<StructType>(T);
      // STRUCT: [ispacked, eltty x N]
      TypeVals.push_back(ST->isPacked());
      // Output all of the element types.
      for (StructType::element_iterator I = ST->element_begin(),
           E = ST->element_end(); I != E; ++I)
        TypeVals.push_back(VE.getTypeID(*I));

      if (ST->isLiteral()) {
        Code = naclbitc::TYPE_CODE_STRUCT_ANON;
        AbbrevToUse = TYPE_STRUCT_ANON_ABBREV;
      } else {
        if (ST->isOpaque()) {
          Code = naclbitc::TYPE_CODE_OPAQUE;
        } else {
          Code = naclbitc::TYPE_CODE_STRUCT_NAMED;
          AbbrevToUse = TYPE_STRUCT_NAMED_ABBREV;
        }

        // Emit the name if it is present.
        if (!ST->getName().empty())
          WriteStringRecord(naclbitc::TYPE_CODE_STRUCT_NAME, ST->getName(),
                            TYPE_STRUCT_NAME_ABBREV, Stream);
      }
      break;
    }
    case Type::ArrayTyID: {
      ArrayType *AT = cast<ArrayType>(T);
      // ARRAY: [numelts, eltty]
      Code = naclbitc::TYPE_CODE_ARRAY;
      TypeVals.push_back(AT->getNumElements());
      TypeVals.push_back(VE.getTypeID(AT->getElementType()));
      AbbrevToUse = TYPE_ARRAY_ABBREV;
      break;
    }
    case Type::VectorTyID: {
      VectorType *VT = cast<VectorType>(T);
      // VECTOR [numelts, eltty]
      Code = naclbitc::TYPE_CODE_VECTOR;
      TypeVals.push_back(VT->getNumElements());
      TypeVals.push_back(VE.getTypeID(VT->getElementType()));
      break;
    }
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
  case GlobalValue::WeakAnyLinkage:                  return 1;
  case GlobalValue::AppendingLinkage:                return 2;
  case GlobalValue::InternalLinkage:                 return 3;
  case GlobalValue::LinkOnceAnyLinkage:              return 4;
  case GlobalValue::DLLImportLinkage:                return 5;
  case GlobalValue::DLLExportLinkage:                return 6;
  case GlobalValue::ExternalWeakLinkage:             return 7;
  case GlobalValue::CommonLinkage:                   return 8;
  case GlobalValue::PrivateLinkage:                  return 9;
  case GlobalValue::WeakODRLinkage:                  return 10;
  case GlobalValue::LinkOnceODRLinkage:              return 11;
  case GlobalValue::AvailableExternallyLinkage:      return 12;
  case GlobalValue::LinkerPrivateLinkage:            return 13;
  case GlobalValue::LinkerPrivateWeakLinkage:        return 14;
  case GlobalValue::LinkOnceODRAutoHideLinkage:      return 15;
  }
  llvm_unreachable("Invalid linkage");
}

static unsigned getEncodedVisibility(const GlobalValue *GV) {
  switch (GV->getVisibility()) {
  case GlobalValue::DefaultVisibility:   return 0;
  case GlobalValue::HiddenVisibility:    return 1;
  case GlobalValue::ProtectedVisibility: return 2;
  }
  llvm_unreachable("Invalid visibility");
}

static unsigned getEncodedThreadLocalMode(const GlobalVariable *GV) {
  switch (GV->getThreadLocalMode()) {
    case GlobalVariable::NotThreadLocal:         return 0;
    case GlobalVariable::GeneralDynamicTLSModel: return 1;
    case GlobalVariable::LocalDynamicTLSModel:   return 2;
    case GlobalVariable::InitialExecTLSModel:    return 3;
    case GlobalVariable::LocalExecTLSModel:      return 4;
  }
  llvm_unreachable("Invalid TLS model");
}

// Emit top-level description of module, including inline asm,
// descriptors for global variables, and function prototype info.
static void WriteModuleInfo(const Module *M, const NaClValueEnumerator &VE,
                            NaClBitstreamWriter &Stream) {
  DEBUG(dbgs() << "-> WriteModuleInfo\n");
  // Emit various pieces of data attached to a module.
  if (!M->getModuleInlineAsm().empty())
    WriteStringRecord(naclbitc::MODULE_CODE_ASM, M->getModuleInlineAsm(),
                      0/*TODO*/, Stream);

  // Emit information about sections and GC, computing how many there are. Also
  // compute the maximum alignment value.
  std::map<std::string, unsigned> SectionMap;
  std::map<std::string, unsigned> GCMap;
  unsigned MaxAlignment = 0;
  unsigned MaxGlobalType = 0;
  for (Module::const_global_iterator GV = M->global_begin(),E = M->global_end();
       GV != E; ++GV) {
    MaxAlignment = std::max(MaxAlignment, GV->getAlignment());
    MaxGlobalType = std::max(MaxGlobalType, VE.getTypeID(GV->getType()));
    if (GV->hasSection()) {
      // Give section names unique ID's.
      unsigned &Entry = SectionMap[GV->getSection()];
      if (!Entry) {
        WriteStringRecord(naclbitc::MODULE_CODE_SECTIONNAME, GV->getSection(),
                          0/*TODO*/, Stream);
        Entry = SectionMap.size();
      }
    }
  }
  for (Module::const_iterator F = M->begin(), E = M->end(); F != E; ++F) {
    MaxAlignment = std::max(MaxAlignment, F->getAlignment());
    if (F->hasSection()) {
      // Give section names unique ID's.
      unsigned &Entry = SectionMap[F->getSection()];
      if (!Entry) {
        WriteStringRecord(naclbitc::MODULE_CODE_SECTIONNAME, F->getSection(),
                          0/*TODO*/, Stream);
        Entry = SectionMap.size();
      }
    }
    if (F->hasGC()) {
      // Same for GC names.
      unsigned &Entry = GCMap[F->getGC()];
      if (!Entry) {
        WriteStringRecord(naclbitc::MODULE_CODE_GCNAME, F->getGC(),
                          0/*TODO*/, Stream);
        Entry = GCMap.size();
      }
    }
  }

  // Emit abbrev for globals, now that we know # sections and max alignment.
  // Add an abbrev for common globals with no visibility or thread localness.
  // Don't bother emitting vis + thread local abbreviation.
  {
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::MODULE_CODE_GLOBALVAR));
    Abbv->Add(NaClBitCodeAbbrevOp(
        NaClBitCodeAbbrevOp::Fixed, NaClBitsNeededForValue(MaxGlobalType)));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, 1)); // Constant.
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));   // Initializer.
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, 4)); // Linkage.
    if (MaxAlignment == 0)                                         // Alignment.
      Abbv->Add(NaClBitCodeAbbrevOp(0));
    else {
      unsigned MaxEncAlignment = Log2_32(MaxAlignment)+1;
      Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed,
                                    NaClBitsNeededForValue(MaxEncAlignment)));
    }
    if (SectionMap.empty())                                    // Section.
      Abbv->Add(NaClBitCodeAbbrevOp(0));
    else
      Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed,
                                    NaClBitsNeededForValue(SectionMap.size())));
    if (MODULE_GLOBALVAR_ABBREV != Stream.EmitAbbrev(Abbv))
      llvm_unreachable("Unexpected abbrev ordering!");
  }

  // Emit the global variable information.
  SmallVector<unsigned, 64> Vals;
  for (Module::const_global_iterator GV = M->global_begin(),E = M->global_end();
       GV != E; ++GV) {
    unsigned AbbrevToUse = 0;

    // GLOBALVAR: [type, isconst, initid,
    //             linkage, alignment, section, visibility, threadlocal,
    //             unnamed_addr]
    Vals.push_back(VE.getTypeID(GV->getType()));
    Vals.push_back(GV->isConstant());
    Vals.push_back(GV->isDeclaration() ? 0 :
                   (VE.getValueID(GV->getInitializer()) + 1));
    Vals.push_back(getEncodedLinkage(GV));
    Vals.push_back(Log2_32(GV->getAlignment())+1);
    Vals.push_back(GV->hasSection() ? SectionMap[GV->getSection()] : 0);
    if (GV->isThreadLocal() ||
        GV->getVisibility() != GlobalValue::DefaultVisibility ||
        GV->hasUnnamedAddr() || GV->isExternallyInitialized()) {
      Vals.push_back(getEncodedVisibility(GV));
      Vals.push_back(getEncodedThreadLocalMode(GV));
      Vals.push_back(GV->hasUnnamedAddr());
      Vals.push_back(GV->isExternallyInitialized());
    } else {
      AbbrevToUse = MODULE_GLOBALVAR_ABBREV;
    }

    Stream.EmitRecord(naclbitc::MODULE_CODE_GLOBALVAR, Vals, AbbrevToUse);
    Vals.clear();
  }

  // Emit the function proto information.
  for (Module::const_iterator F = M->begin(), E = M->end(); F != E; ++F) {
    // FUNCTION:  [type, callingconv, isproto, linkage]
    Vals.push_back(VE.getTypeID(F->getType()));
    Vals.push_back(GetEncodedCallingConv(F->getCallingConv()));
    Vals.push_back(F->isDeclaration());
    Vals.push_back(getEncodedLinkage(F));

    unsigned AbbrevToUse = 0;
    Stream.EmitRecord(naclbitc::MODULE_CODE_FUNCTION, Vals, AbbrevToUse);
    Vals.clear();
  }

  // Emit the alias information.
  for (Module::const_alias_iterator AI = M->alias_begin(), E = M->alias_end();
       AI != E; ++AI) {
    // ALIAS: [alias type, aliasee val#, linkage, visibility]
    Vals.push_back(VE.getTypeID(AI->getType()));
    Vals.push_back(VE.getValueID(AI->getAliasee()));
    Vals.push_back(getEncodedLinkage(AI));
    Vals.push_back(getEncodedVisibility(AI));
    unsigned AbbrevToUse = 0;
    Stream.EmitRecord(naclbitc::MODULE_CODE_ALIAS, Vals, AbbrevToUse);
    Vals.clear();
  }
  DEBUG(dbgs() << "<- WriteModuleInfo\n");
}

static uint64_t GetOptimizationFlags(const Value *V) {
  uint64_t Flags = 0;

  if (const OverflowingBinaryOperator *OBO =
        dyn_cast<OverflowingBinaryOperator>(V)) {
    if (OBO->hasNoSignedWrap())
      Flags |= 1 << naclbitc::OBO_NO_SIGNED_WRAP;
    if (OBO->hasNoUnsignedWrap())
      Flags |= 1 << naclbitc::OBO_NO_UNSIGNED_WRAP;
  } else if (const PossiblyExactOperator *PEO =
               dyn_cast<PossiblyExactOperator>(V)) {
    if (PEO->isExact())
      Flags |= 1 << naclbitc::PEO_EXACT;
  } else if (const FPMathOperator *FPMO =
             dyn_cast<const FPMathOperator>(V)) {
    if (FPMO->hasUnsafeAlgebra())
      Flags |= 1 << naclbitc::FPO_UNSAFE_ALGEBRA;
    if (FPMO->hasNoNaNs())
      Flags |= 1 << naclbitc::FPO_NO_NANS;
    if (FPMO->hasNoInfs())
      Flags |= 1 << naclbitc::FPO_NO_INFS;
    if (FPMO->hasNoSignedZeros())
      Flags |= 1 << naclbitc::FPO_NO_SIGNED_ZEROS;
    if (FPMO->hasAllowReciprocal())
      Flags |= 1 << naclbitc::FPO_ALLOW_RECIPROCAL;
  }

  return Flags;
}

static void WriteMDNode(const MDNode *N,
                        const NaClValueEnumerator &VE,
                        NaClBitstreamWriter &Stream,
                        SmallVector<uint64_t, 64> &Record) {
  for (unsigned i = 0, e = N->getNumOperands(); i != e; ++i) {
    if (N->getOperand(i)) {
      Record.push_back(VE.getTypeID(N->getOperand(i)->getType()));
      Record.push_back(VE.getValueID(N->getOperand(i)));
    } else {
      Record.push_back(VE.getTypeID(Type::getVoidTy(N->getContext())));
      Record.push_back(0);
    }
  }
  unsigned MDCode = N->isFunctionLocal() ? naclbitc::METADATA_FN_NODE :
                                           naclbitc::METADATA_NODE;
  Stream.EmitRecord(MDCode, Record, 0);
  Record.clear();
}

static void WriteModuleMetadata(const Module *M,
                                const NaClValueEnumerator &VE,
                                NaClBitstreamWriter &Stream) {
  DEBUG(dbgs() << "-> WriteModuleMetadata\n");
  const NaClValueEnumerator::ValueList &Vals = VE.getMDValues();
  bool StartedMetadataBlock = false;
  SmallVector<uint64_t, 64> Record;
  for (unsigned i = 0, e = Vals.size(); i != e; ++i) {

    if (const MDNode *N = dyn_cast<MDNode>(Vals[i].first)) {
      if (!N->isFunctionLocal() || !N->getFunction()) {
        if (!StartedMetadataBlock) {
          Stream.EnterSubblock(naclbitc::METADATA_BLOCK_ID);
          StartedMetadataBlock = true;
        }
        WriteMDNode(N, VE, Stream, Record);
      }
    } else if (const MDString *MDS = dyn_cast<MDString>(Vals[i].first)) {
      if (!StartedMetadataBlock)  {
        Stream.EnterSubblock(naclbitc::METADATA_BLOCK_ID);
        StartedMetadataBlock = true;
      }

      // Code: [strchar x N]
      Record.append(MDS->begin(), MDS->end());

      // Emit the finished record.
      Stream.EmitRecord(naclbitc::METADATA_STRING, Record,
                        METADATA_STRING_ABBREV);
      Record.clear();
    }
  }

  // Write named metadata.
  for (Module::const_named_metadata_iterator I = M->named_metadata_begin(),
       E = M->named_metadata_end(); I != E; ++I) {
    const NamedMDNode *NMD = I;
    if (!StartedMetadataBlock)  {
      Stream.EnterSubblock(naclbitc::METADATA_BLOCK_ID);
      StartedMetadataBlock = true;
    }

    // Write name.
    StringRef Str = NMD->getName();
    for (unsigned i = 0, e = Str.size(); i != e; ++i)
      Record.push_back(Str[i]);
    Stream.EmitRecord(naclbitc::METADATA_NAME, Record, 0/*TODO*/);
    Record.clear();

    // Write named metadata operands.
    for (unsigned i = 0, e = NMD->getNumOperands(); i != e; ++i)
      Record.push_back(VE.getValueID(NMD->getOperand(i)));
    Stream.EmitRecord(naclbitc::METADATA_NAMED_NODE, Record, 0);
    Record.clear();
  }

  if (StartedMetadataBlock)
    Stream.ExitBlock();

  DEBUG(dbgs() << "<- WriteModuleMetadata\n");
}

static void WriteFunctionLocalMetadata(const Function &F,
                                       const NaClValueEnumerator &VE,
                                       NaClBitstreamWriter &Stream) {
  DEBUG(dbgs() << "-> WriteFunctionLocalMetadata\n");
  bool StartedMetadataBlock = false;
  SmallVector<uint64_t, 64> Record;
  const SmallVector<const MDNode *, 8> &Vals = VE.getFunctionLocalMDValues();
  for (unsigned i = 0, e = Vals.size(); i != e; ++i)
    if (const MDNode *N = Vals[i])
      if (N->isFunctionLocal() && N->getFunction() == &F) {
        if (!StartedMetadataBlock) {
          Stream.EnterSubblock(naclbitc::METADATA_BLOCK_ID);
          StartedMetadataBlock = true;
        }
        WriteMDNode(N, VE, Stream, Record);
      }

  if (StartedMetadataBlock)
    Stream.ExitBlock();
  DEBUG(dbgs() << "<- WriteFunctionLocalMetadata\n");
}

static void WriteMetadataAttachment(const Function &F,
                                    const NaClValueEnumerator &VE,
                                    NaClBitstreamWriter &Stream) {
  Stream.EnterSubblock(naclbitc::METADATA_ATTACHMENT_ID);

  SmallVector<uint64_t, 64> Record;

  // Write metadata attachments
  // METADATA_ATTACHMENT - [m x [value, [n x [id, mdnode]]]
  SmallVector<std::pair<unsigned, MDNode*>, 4> MDs;

  for (Function::const_iterator BB = F.begin(), E = F.end(); BB != E; ++BB)
    for (BasicBlock::const_iterator I = BB->begin(), E = BB->end();
         I != E; ++I) {
      MDs.clear();
      I->getAllMetadataOtherThanDebugLoc(MDs);

      // If no metadata, ignore instruction.
      if (MDs.empty()) continue;

      Record.push_back(VE.getInstructionID(I));

      for (unsigned i = 0, e = MDs.size(); i != e; ++i) {
        Record.push_back(MDs[i].first);
        Record.push_back(VE.getValueID(MDs[i].second));
      }
      Stream.EmitRecord(naclbitc::METADATA_ATTACHMENT, Record, 0);
      Record.clear();
    }

  Stream.ExitBlock();
}

static void WriteModuleMetadataStore(const Module *M,
                                     NaClBitstreamWriter &Stream) {
  SmallVector<uint64_t, 64> Record;

  // Write metadata kinds
  // METADATA_KIND - [n x [id, name]]
  SmallVector<StringRef, 8> Names;
  M->getMDKindNames(Names);

  if (Names.empty()) return;

  Stream.EnterSubblock(naclbitc::METADATA_BLOCK_ID);

  for (unsigned MDKindID = 0, e = Names.size(); MDKindID != e; ++MDKindID) {
    Record.push_back(MDKindID);
    StringRef KName = Names[MDKindID];
    Record.append(KName.begin(), KName.end());

    Stream.EmitRecord(naclbitc::METADATA_KIND, Record, 0);
    Record.clear();
  }

  Stream.ExitBlock();
}

static void emitSignedInt64(SmallVectorImpl<uint64_t> &Vals, uint64_t V) {
  Vals.push_back(NaClEncodeSignRotatedValue((int64_t)V));
}

static void EmitAPInt(SmallVectorImpl<uint64_t> &Vals,
                      unsigned &Code, unsigned &AbbrevToUse, const APInt &Val,
                      bool EmitSizeForWideNumbers = false
                      ) {
  if (Val.getBitWidth() <= 64) {
    uint64_t V = Val.getSExtValue();
    emitSignedInt64(Vals, V);
    Code = naclbitc::CST_CODE_INTEGER;
    AbbrevToUse = CONSTANTS_INTEGER_ABBREV;
  } else {
    // Wide integers, > 64 bits in size.
    // We have an arbitrary precision integer value to write whose
    // bit width is > 64. However, in canonical unsigned integer
    // format it is likely that the high bits are going to be zero.
    // So, we only write the number of active words.
    unsigned NWords = Val.getActiveWords();

    if (EmitSizeForWideNumbers)
      Vals.push_back(NWords);

    const uint64_t *RawWords = Val.getRawData();
    for (unsigned i = 0; i != NWords; ++i) {
      emitSignedInt64(Vals, RawWords[i]);
    }
    Code = naclbitc::CST_CODE_WIDE_INTEGER;
  }
}

static void WriteConstants(unsigned FirstVal, unsigned LastVal,
                           const NaClValueEnumerator &VE,
                           NaClBitstreamWriter &Stream, bool isGlobal) {
  if (FirstVal == LastVal) return;

  Stream.EnterSubblock(naclbitc::CONSTANTS_BLOCK_ID,
                       (isGlobal
                        ? CST_CONSTANTS_MAX_ABBREV
                        : CONSTANTS_MAX_ABBREV));

  unsigned AggregateAbbrev = 0;
  unsigned String8Abbrev = 0;
  unsigned CString7Abbrev = 0;
  unsigned CString6Abbrev = 0;
  // If this is a constant pool for the module, emit module-specific abbrevs.
  // Note: These abbreviations are size specific (to LastVal), and hence,
  // can be more efficient if LastVal is known (rather then generating
  // up-front for all constant sections).
  if (isGlobal) {
    // Abbrev for CST_CODE_AGGREGATE.
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::CST_CODE_AGGREGATE));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Array));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed,
                                  NaClBitsNeededForValue(LastVal)));
    AggregateAbbrev = Stream.EmitAbbrev(Abbv);
    if (CST_CONSTANTS_AGGREGATE_ABBREV != AggregateAbbrev)
      llvm_unreachable("Unexpected abbrev ordering!");

    // Abbrev for CST_CODE_STRING.
    Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::CST_CODE_STRING));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Array));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, 8));
    String8Abbrev = Stream.EmitAbbrev(Abbv);
    if (CST_CONSTANTS_STRING_ABBREV != String8Abbrev)
      llvm_unreachable("Unexpected abbrev ordering!");

    // Abbrev for CST_CODE_CSTRING.
    Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::CST_CODE_CSTRING));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Array));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, 7));
    CString7Abbrev = Stream.EmitAbbrev(Abbv);
    if (CST_CONSTANTS_CSTRING_7_ABBREV != CString7Abbrev)
      llvm_unreachable("Unexpected abbrev ordering!");

    // Abbrev for CST_CODE_CSTRING.
    Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::CST_CODE_CSTRING));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Array));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Char6));
    CString6Abbrev = Stream.EmitAbbrev(Abbv);
    if (CST_CONSTANTS_CSTRING_6_ABBREV != CString6Abbrev)
      llvm_unreachable("Unexpected abbrev ordering!");

    DEBUG(dbgs() << "-- emitted abbreviations\n");
  }


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

    if (const InlineAsm *IA = dyn_cast<InlineAsm>(V)) {
      Record.push_back(unsigned(IA->hasSideEffects()) |
                       unsigned(IA->isAlignStack()) << 1 |
                       unsigned(IA->getDialect()&1) << 2);

      // Add the asm string.
      const std::string &AsmStr = IA->getAsmString();
      Record.push_back(AsmStr.size());
      for (unsigned i = 0, e = AsmStr.size(); i != e; ++i)
        Record.push_back(AsmStr[i]);

      // Add the constraint string.
      const std::string &ConstraintStr = IA->getConstraintString();
      Record.push_back(ConstraintStr.size());
      for (unsigned i = 0, e = ConstraintStr.size(); i != e; ++i)
        Record.push_back(ConstraintStr[i]);
      Stream.EmitRecord(naclbitc::CST_CODE_INLINEASM, Record);
      Record.clear();
      continue;
    }
    const Constant *C = cast<Constant>(V);
    unsigned Code = -1U;
    unsigned AbbrevToUse = 0;
    if (C->isNullValue()) {
      Code = naclbitc::CST_CODE_NULL;
    } else if (isa<UndefValue>(C)) {
      Code = naclbitc::CST_CODE_UNDEF;
    } else if (const ConstantInt *IV = dyn_cast<ConstantInt>(C)) {
      EmitAPInt(Record, Code, AbbrevToUse, IV->getValue());
    } else if (const ConstantFP *CFP = dyn_cast<ConstantFP>(C)) {
      Code = naclbitc::CST_CODE_FLOAT;
      Type *Ty = CFP->getType();
      if (Ty->isHalfTy() || Ty->isFloatTy() || Ty->isDoubleTy()) {
        Record.push_back(CFP->getValueAPF().bitcastToAPInt().getZExtValue());
      } else if (Ty->isX86_FP80Ty()) {
        // api needed to prevent premature destruction
        // bits are not in the same order as a normal i80 APInt, compensate.
        APInt api = CFP->getValueAPF().bitcastToAPInt();
        const uint64_t *p = api.getRawData();
        Record.push_back((p[1] << 48) | (p[0] >> 16));
        Record.push_back(p[0] & 0xffffLL);
      } else if (Ty->isFP128Ty() || Ty->isPPC_FP128Ty()) {
        APInt api = CFP->getValueAPF().bitcastToAPInt();
        const uint64_t *p = api.getRawData();
        Record.push_back(p[0]);
        Record.push_back(p[1]);
      } else {
        assert (0 && "Unknown FP type!");
      }
    } else if (isa<ConstantDataSequential>(C) &&
               cast<ConstantDataSequential>(C)->isString()) {
      const ConstantDataSequential *Str = cast<ConstantDataSequential>(C);
      // Emit constant strings specially.
      unsigned NumElts = Str->getNumElements();
      // If this is a null-terminated string, use the denser CSTRING encoding.
      if (Str->isCString()) {
        Code = naclbitc::CST_CODE_CSTRING;
        --NumElts;  // Don't encode the null, which isn't allowed by char6.
      } else {
        Code = naclbitc::CST_CODE_STRING;
        AbbrevToUse = String8Abbrev;
      }
      bool isCStr7 = Code == naclbitc::CST_CODE_CSTRING;
      bool isCStrChar6 = Code == naclbitc::CST_CODE_CSTRING;
      for (unsigned i = 0; i != NumElts; ++i) {
        unsigned char V = Str->getElementAsInteger(i);
        Record.push_back(V);
        isCStr7 &= (V & 128) == 0;
        if (isCStrChar6)
          isCStrChar6 = NaClBitCodeAbbrevOp::isChar6(V);
      }

      if (isCStrChar6)
        AbbrevToUse = CString6Abbrev;
      else if (isCStr7)
        AbbrevToUse = CString7Abbrev;
    } else if (const ConstantDataSequential *CDS =
                  dyn_cast<ConstantDataSequential>(C)) {
      Code = naclbitc::CST_CODE_DATA;
      Type *EltTy = CDS->getType()->getElementType();
      if (isa<IntegerType>(EltTy)) {
        for (unsigned i = 0, e = CDS->getNumElements(); i != e; ++i)
          Record.push_back(CDS->getElementAsInteger(i));
      } else if (EltTy->isFloatTy()) {
        for (unsigned i = 0, e = CDS->getNumElements(); i != e; ++i) {
          union { float F; uint32_t I; };
          F = CDS->getElementAsFloat(i);
          Record.push_back(I);
        }
      } else {
        assert(EltTy->isDoubleTy() && "Unknown ConstantData element type");
        for (unsigned i = 0, e = CDS->getNumElements(); i != e; ++i) {
          union { double F; uint64_t I; };
          F = CDS->getElementAsDouble(i);
          Record.push_back(I);
        }
      }
    } else if (isa<ConstantArray>(C) || isa<ConstantStruct>(C) ||
               isa<ConstantVector>(C)) {
      Code = naclbitc::CST_CODE_AGGREGATE;
      for (unsigned i = 0, e = C->getNumOperands(); i != e; ++i)
        Record.push_back(VE.getValueID(C->getOperand(i)));
      AbbrevToUse = AggregateAbbrev;
    } else if (const ConstantExpr *CE = dyn_cast<ConstantExpr>(C)) {
      switch (CE->getOpcode()) {
      default:
        if (Instruction::isCast(CE->getOpcode())) {
          Code = naclbitc::CST_CODE_CE_CAST;
          Record.push_back(GetEncodedCastOpcode(CE->getOpcode()));
          Record.push_back(VE.getTypeID(C->getOperand(0)->getType()));
          Record.push_back(VE.getValueID(C->getOperand(0)));
          AbbrevToUse = CONSTANTS_CE_CAST_Abbrev;
        } else {
          assert(CE->getNumOperands() == 2 && "Unknown constant expr!");
          Code = naclbitc::CST_CODE_CE_BINOP;
          Record.push_back(GetEncodedBinaryOpcode(CE->getOpcode()));
          Record.push_back(VE.getValueID(C->getOperand(0)));
          Record.push_back(VE.getValueID(C->getOperand(1)));
          uint64_t Flags = GetOptimizationFlags(CE);
          if (Flags != 0)
            Record.push_back(Flags);
        }
        break;
      case Instruction::GetElementPtr:
        Code = naclbitc::CST_CODE_CE_GEP;
        if (cast<GEPOperator>(C)->isInBounds())
          Code = naclbitc::CST_CODE_CE_INBOUNDS_GEP;
        for (unsigned i = 0, e = CE->getNumOperands(); i != e; ++i) {
          Record.push_back(VE.getTypeID(C->getOperand(i)->getType()));
          Record.push_back(VE.getValueID(C->getOperand(i)));
        }
        break;
      case Instruction::Select:
        Code = naclbitc::CST_CODE_CE_SELECT;
        Record.push_back(VE.getValueID(C->getOperand(0)));
        Record.push_back(VE.getValueID(C->getOperand(1)));
        Record.push_back(VE.getValueID(C->getOperand(2)));
        break;
      case Instruction::ExtractElement:
        Code = naclbitc::CST_CODE_CE_EXTRACTELT;
        Record.push_back(VE.getTypeID(C->getOperand(0)->getType()));
        Record.push_back(VE.getValueID(C->getOperand(0)));
        Record.push_back(VE.getValueID(C->getOperand(1)));
        break;
      case Instruction::InsertElement:
        Code = naclbitc::CST_CODE_CE_INSERTELT;
        Record.push_back(VE.getValueID(C->getOperand(0)));
        Record.push_back(VE.getValueID(C->getOperand(1)));
        Record.push_back(VE.getValueID(C->getOperand(2)));
        break;
      case Instruction::ShuffleVector:
        // If the return type and argument types are the same, this is a
        // standard shufflevector instruction.  If the types are different,
        // then the shuffle is widening or truncating the input vectors, and
        // the argument type must also be encoded.
        if (C->getType() == C->getOperand(0)->getType()) {
          Code = naclbitc::CST_CODE_CE_SHUFFLEVEC;
        } else {
          Code = naclbitc::CST_CODE_CE_SHUFVEC_EX;
          Record.push_back(VE.getTypeID(C->getOperand(0)->getType()));
        }
        Record.push_back(VE.getValueID(C->getOperand(0)));
        Record.push_back(VE.getValueID(C->getOperand(1)));
        Record.push_back(VE.getValueID(C->getOperand(2)));
        break;
      case Instruction::ICmp:
      case Instruction::FCmp:
        Code = naclbitc::CST_CODE_CE_CMP;
        Record.push_back(VE.getTypeID(C->getOperand(0)->getType()));
        Record.push_back(VE.getValueID(C->getOperand(0)));
        Record.push_back(VE.getValueID(C->getOperand(1)));
        Record.push_back(CE->getPredicate());
        break;
      }
    } else if (const BlockAddress *BA = dyn_cast<BlockAddress>(C)) {
      Code = naclbitc::CST_CODE_BLOCKADDRESS;
      Record.push_back(VE.getTypeID(BA->getFunction()->getType()));
      Record.push_back(VE.getValueID(BA->getFunction()));
      Record.push_back(VE.getGlobalBasicBlockID(BA->getBasicBlock()));
    } else {
#ifndef NDEBUG
      C->dump();
#endif
      llvm_unreachable("Unknown constant!");
    }
    Stream.EmitRecord(Code, Record, AbbrevToUse);
    Record.clear();
  }

  Stream.ExitBlock();
  DEBUG(dbgs() << "<- WriteConstants\n");
}

static void WriteModuleConstants(const NaClValueEnumerator &VE,
                                 NaClBitstreamWriter &Stream) {
  const NaClValueEnumerator::ValueList &Vals = VE.getValues();

  // Find the first constant to emit, which is the first non-globalvalue value.
  // We know globalvalues have been emitted by WriteModuleInfo.
  for (unsigned i = 0, e = Vals.size(); i != e; ++i) {
    if (!isa<GlobalValue>(Vals[i].first)) {
      WriteConstants(i, Vals.size(), VE, Stream, true);
      return;
    }
  }
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
    Vals.push_back(VE.getTypeID(V->getType()));
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
  EmitFnForwardTypeRef(V, InstID, VE, Stream);
  unsigned ValID = VE.getValueID(V);
  // Make encoding relative to the InstID.
  Vals.push_back(InstID - ValID);
}

static void pushValue64(const Value *V, unsigned InstID,
                        SmallVector<uint64_t, 128> &Vals,
                        NaClValueEnumerator &VE,
                        NaClBitstreamWriter &Stream) {
  EmitFnForwardTypeRef(V, InstID, VE, Stream);
  uint64_t ValID = VE.getValueID(V);
  Vals.push_back(InstID - ValID);
}

static void pushValueSigned(const Value *V, unsigned InstID,
                            SmallVector<uint64_t, 128> &Vals,
                            NaClValueEnumerator &VE,
                            NaClBitstreamWriter &Stream) {
  EmitFnForwardTypeRef(V, InstID, VE, Stream);
  unsigned ValID = VE.getValueID(V);
  int64_t diff = ((int32_t)InstID - (int32_t)ValID);
  emitSignedInt64(Vals, diff);
}

/// WriteInstruction - Emit an instruction to the specified stream.
static void WriteInstruction(const Instruction &I, unsigned InstID,
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
      Code = naclbitc::FUNC_CODE_INST_CAST;
      AbbrevToUse = FUNCTION_INST_CAST_ABBREV;
      pushValue(I.getOperand(0), InstID, Vals, VE, Stream);
      Vals.push_back(VE.getTypeID(I.getType()));
      Vals.push_back(GetEncodedCastOpcode(I.getOpcode()));
    } else {
      // BINOP:      [opval, opval, opcode[, flags]]
      assert(isa<BinaryOperator>(I) && "Unknown instruction!");
      Code = naclbitc::FUNC_CODE_INST_BINOP;
      AbbrevToUse = FUNCTION_INST_BINOP_ABBREV;
      pushValue(I.getOperand(0), InstID, Vals, VE, Stream);
      pushValue(I.getOperand(1), InstID, Vals, VE, Stream);
      Vals.push_back(GetEncodedBinaryOpcode(I.getOpcode()));
      uint64_t Flags = GetOptimizationFlags(&I);
      if (Flags != 0) {
        AbbrevToUse = FUNCTION_INST_BINOP_FLAGS_ABBREV;
        Vals.push_back(Flags);
      }
    }
    break;

  case Instruction::GetElementPtr:
    Code = naclbitc::FUNC_CODE_INST_GEP;
    if (cast<GEPOperator>(&I)->isInBounds())
      Code = naclbitc::FUNC_CODE_INST_INBOUNDS_GEP;
    for (unsigned i = 0, e = I.getNumOperands(); i != e; ++i)
      pushValue(I.getOperand(i), InstID, Vals, VE, Stream);
    break;
  case Instruction::ExtractValue: {
    Code = naclbitc::FUNC_CODE_INST_EXTRACTVAL;
    pushValue(I.getOperand(0), InstID, Vals, VE, Stream);
    const ExtractValueInst *EVI = cast<ExtractValueInst>(&I);
    for (const unsigned *i = EVI->idx_begin(), *e = EVI->idx_end(); i != e; ++i)
      Vals.push_back(*i);
    break;
  }
  case Instruction::InsertValue: {
    Code = naclbitc::FUNC_CODE_INST_INSERTVAL;
    pushValue(I.getOperand(0), InstID, Vals, VE, Stream);
    pushValue(I.getOperand(1), InstID, Vals, VE, Stream);
    const InsertValueInst *IVI = cast<InsertValueInst>(&I);
    for (const unsigned *i = IVI->idx_begin(), *e = IVI->idx_end(); i != e; ++i)
      Vals.push_back(*i);
    break;
  }
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
  case Instruction::ShuffleVector:
    Code = naclbitc::FUNC_CODE_INST_SHUFFLEVEC;
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
    Vals.push_back(cast<CmpInst>(I).getPredicate());
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
        const IntegersSubset& CaseRanges = i.getCaseValueEx();
        unsigned Code, Abbrev; // will unused.

        if (CaseRanges.isSingleNumber()) {
          Vals64.push_back(1/*NumItems = 1*/);
          Vals64.push_back(true/*IsSingleNumber = true*/);
          EmitAPInt(Vals64, Code, Abbrev, CaseRanges.getSingleNumber(0), true);
        } else {

          Vals64.push_back(CaseRanges.getNumItems());

          if (CaseRanges.isSingleNumbersOnly()) {
            for (unsigned ri = 0, rn = CaseRanges.getNumItems();
                 ri != rn; ++ri) {

              Vals64.push_back(true/*IsSingleNumber = true*/);

              EmitAPInt(Vals64, Code, Abbrev,
                        CaseRanges.getSingleNumber(ri), true);
            }
          } else
            for (unsigned ri = 0, rn = CaseRanges.getNumItems();
                 ri != rn; ++ri) {
              IntegersSubset::Range r = CaseRanges.getItem(ri);
              bool IsSingleNumber = CaseRanges.isSingleNumber(ri);

              Vals64.push_back(IsSingleNumber);

              EmitAPInt(Vals64, Code, Abbrev, r.getLow(), true);
              if (!IsSingleNumber)
                EmitAPInt(Vals64, Code, Abbrev, r.getHigh(), true);
            }
        }
        Vals64.push_back(VE.getValueID(i.getCaseSuccessor()));
      }

      Stream.EmitRecord(Code, Vals64, AbbrevToUse);

      // Also do expected action - clear external Vals collection:
      Vals.clear();
      return;
    }
    break;
  case Instruction::IndirectBr:
    Code = naclbitc::FUNC_CODE_INST_INDIRECTBR;
    Vals.push_back(VE.getTypeID(I.getOperand(0)->getType()));
    // Encode the address operand as relative, but not the basic blocks.
    pushValue(I.getOperand(0), InstID, Vals, VE, Stream);
    for (unsigned i = 1, e = I.getNumOperands(); i != e; ++i)
      Vals.push_back(VE.getValueID(I.getOperand(i)));
    break;

  case Instruction::Invoke:
    report_fatal_error("Invoke is not allowed in PNaCl bitcode");
    break;
  case Instruction::Resume:
    Code = naclbitc::FUNC_CODE_INST_RESUME;
    pushValue(I.getOperand(0), InstID, Vals, VE, Stream);
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
    return;
  }

  case Instruction::LandingPad: {
    const LandingPadInst &LP = cast<LandingPadInst>(I);
    Code = naclbitc::FUNC_CODE_INST_LANDINGPAD;
    Vals.push_back(VE.getTypeID(LP.getType()));
    pushValue(LP.getPersonalityFn(), InstID, Vals, VE, Stream);
    Vals.push_back(LP.isCleanup());
    Vals.push_back(LP.getNumClauses());
    for (unsigned I = 0, E = LP.getNumClauses(); I != E; ++I) {
      if (LP.isCatch(I))
        Vals.push_back(LandingPadInst::Catch);
      else
        Vals.push_back(LandingPadInst::Filter);
      pushValue(LP.getClause(I), InstID, Vals, VE, Stream);
    }
    break;
  }

  case Instruction::Alloca:
    if (!cast<AllocaInst>(&I)->getAllocatedType()->isIntegerTy(8))
      report_fatal_error("Type of alloca instruction is not i8");
    Code = naclbitc::FUNC_CODE_INST_ALLOCA;
    pushValue(I.getOperand(0), InstID, Vals, VE, Stream); // size.
    Vals.push_back(Log2_32(cast<AllocaInst>(I).getAlignment())+1);
    break;

  case Instruction::Load:
    if (cast<LoadInst>(I).isAtomic()) {
      Code = naclbitc::FUNC_CODE_INST_LOADATOMIC;
      pushValue(I.getOperand(0), InstID, Vals, VE, Stream);
    } else {
      Code = naclbitc::FUNC_CODE_INST_LOAD;
      pushValue(I.getOperand(0), InstID, Vals, VE, Stream);  // ptr
      AbbrevToUse = FUNCTION_INST_LOAD_ABBREV;
    }
    Vals.push_back(Log2_32(cast<LoadInst>(I).getAlignment())+1);
    Vals.push_back(cast<LoadInst>(I).isVolatile());
    if (cast<LoadInst>(I).isAtomic()) {
      Vals.push_back(GetEncodedOrdering(cast<LoadInst>(I).getOrdering()));
      Vals.push_back(GetEncodedSynchScope(cast<LoadInst>(I).getSynchScope()));
    }
    break;
  case Instruction::Store:
    if (cast<StoreInst>(I).isAtomic())
      Code = naclbitc::FUNC_CODE_INST_STOREATOMIC;
    else
      Code = naclbitc::FUNC_CODE_INST_STORE;
    pushValue(I.getOperand(1), InstID, Vals, VE, Stream);  // ptrty + ptr
    pushValue(I.getOperand(0), InstID, Vals, VE, Stream);  // val.
    Vals.push_back(Log2_32(cast<StoreInst>(I).getAlignment())+1);
    Vals.push_back(cast<StoreInst>(I).isVolatile());
    if (cast<StoreInst>(I).isAtomic()) {
      Vals.push_back(GetEncodedOrdering(cast<StoreInst>(I).getOrdering()));
      Vals.push_back(GetEncodedSynchScope(cast<StoreInst>(I).getSynchScope()));
    }
    break;
  case Instruction::AtomicCmpXchg:
    Code = naclbitc::FUNC_CODE_INST_CMPXCHG;
    pushValue(I.getOperand(0), InstID, Vals, VE, Stream);  // ptrty + ptr
    pushValue(I.getOperand(1), InstID, Vals, VE, Stream);  // cmp.
    pushValue(I.getOperand(2), InstID, Vals, VE, Stream);  // newval.
    Vals.push_back(cast<AtomicCmpXchgInst>(I).isVolatile());
    Vals.push_back(GetEncodedOrdering(
                     cast<AtomicCmpXchgInst>(I).getOrdering()));
    Vals.push_back(GetEncodedSynchScope(
                     cast<AtomicCmpXchgInst>(I).getSynchScope()));
    break;
  case Instruction::AtomicRMW:
    Code = naclbitc::FUNC_CODE_INST_ATOMICRMW;
    pushValue(I.getOperand(0), InstID, Vals, VE, Stream);  // ptrty + ptr
    pushValue(I.getOperand(1), InstID, Vals, VE, Stream);  // val.
    Vals.push_back(GetEncodedRMWOperation(
                     cast<AtomicRMWInst>(I).getOperation()));
    Vals.push_back(cast<AtomicRMWInst>(I).isVolatile());
    Vals.push_back(GetEncodedOrdering(cast<AtomicRMWInst>(I).getOrdering()));
    Vals.push_back(GetEncodedSynchScope(
                     cast<AtomicRMWInst>(I).getSynchScope()));
    break;
  case Instruction::Fence:
    Code = naclbitc::FUNC_CODE_INST_FENCE;
    Vals.push_back(GetEncodedOrdering(cast<FenceInst>(I).getOrdering()));
    Vals.push_back(GetEncodedSynchScope(cast<FenceInst>(I).getSynchScope()));
    break;
  case Instruction::Call: {
    const CallInst &CI = cast<CallInst>(I);
    PointerType *PTy = cast<PointerType>(CI.getCalledValue()->getType());
    FunctionType *FTy = cast<FunctionType>(PTy->getElementType());

    Code = naclbitc::FUNC_CODE_INST_CALL;

    Vals.push_back((GetEncodedCallingConv(CI.getCallingConv()) << 1)
                   | unsigned(CI.isTailCall()));
    pushValue(CI.getCalledValue(), InstID, Vals, VE, Stream);  // Callee

    // Emit value #'s for the fixed parameters.
    for (unsigned i = 0, e = FTy->getNumParams(); i != e; ++i) {
      // Check for labels (can happen with asm labels).
      if (FTy->getParamType(i)->isLabelTy())
        Vals.push_back(VE.getValueID(CI.getArgOperand(i)));
      else
        // fixed param.
        pushValue(CI.getArgOperand(i), InstID, Vals, VE, Stream);
    }

    // Emit type/value pairs for varargs params.
    if (FTy->isVarArg()) {
      for (unsigned i = FTy->getNumParams(), e = CI.getNumArgOperands();
           i != e; ++i)
        // varargs
        pushValue(CI.getArgOperand(i), InstID, Vals, VE, Stream);
    }
    break;
  }
  case Instruction::VAArg:
    Code = naclbitc::FUNC_CODE_INST_VAARG;
    Vals.push_back(VE.getTypeID(I.getOperand(0)->getType()));   // valistty
    pushValue(I.getOperand(0), InstID, Vals, VE, Stream); // valist.
    Vals.push_back(VE.getTypeID(I.getType())); // restype.
    break;
  }

  Stream.EmitRecord(Code, Vals, AbbrevToUse);
  Vals.clear();
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
  WriteConstants(CstStart, CstEnd, VE, Stream, false);

  // If there is function-local metadata, emit it now.
  WriteFunctionLocalMetadata(F, VE, Stream);

  // Keep a running idea of what the instruction ID is.
  unsigned InstID = CstEnd;

  bool NeedsMetadataAttachment = false;

  DebugLoc LastDL;

  // Finally, emit all the instructions, in order.
  for (Function::const_iterator BB = F.begin(), E = F.end(); BB != E; ++BB)
    for (BasicBlock::const_iterator I = BB->begin(), E = BB->end();
         I != E; ++I) {
      WriteInstruction(*I, InstID, VE, Stream, Vals);

      if (!I->getType()->isVoidTy())
        ++InstID;

      // If the instruction has metadata, write a metadata attachment later.
      NeedsMetadataAttachment |= I->hasMetadataOtherThanDebugLoc();

      // If the instruction has a debug location, emit it.
      DebugLoc DL = I->getDebugLoc();
      if (DL.isUnknown()) {
        // nothing todo.
      } else if (DL == LastDL) {
        // Just repeat the same debug loc as last time.
        Stream.EmitRecord(naclbitc::FUNC_CODE_DEBUG_LOC_AGAIN, Vals);
      } else {
        MDNode *Scope, *IA;
        DL.getScopeAndInlinedAt(Scope, IA, I->getContext());

        Vals.push_back(DL.getLine());
        Vals.push_back(DL.getCol());
        Vals.push_back(Scope ? VE.getValueID(Scope)+1 : 0);
        Vals.push_back(IA ? VE.getValueID(IA)+1 : 0);
        Stream.EmitRecord(naclbitc::FUNC_CODE_DEBUG_LOC, Vals);
        Vals.clear();

        LastDL = DL;
      }
    }

  // Emit names for all the instructions etc.
  WriteValueSymbolTable(F.getValueSymbolTable(), VE, Stream);

  if (NeedsMetadataAttachment)
    WriteMetadataAttachment(F, VE, Stream);
  VE.purgeFunction();
  Stream.ExitBlock();
}

// Emit blockinfo, which defines the standard abbreviations etc.
static void WriteBlockInfo(const NaClValueEnumerator &VE,
                           NaClBitstreamWriter &Stream) {
  // We only want to emit block info records for blocks that have multiple
  // instances: CONSTANTS_BLOCK, FUNCTION_BLOCK and VALUE_SYMTAB_BLOCK,
  // and METADATA_BLOCK_ID.
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
    Abbv->Add(NaClBitCodeAbbrevOp(
        NaClBitCodeAbbrevOp::Fixed,
        NaClBitsNeededForValue(VE.getTypes().size())));
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

  { // CE_CAST abbrev for CONSTANTS_BLOCK.
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::CST_CODE_CE_CAST));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, 4));  // cast opc
    Abbv->Add(NaClBitCodeAbbrevOp(
        NaClBitCodeAbbrevOp::Fixed,                                 // typeid
        NaClBitsNeededForValue(VE.getTypes().size())));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 8));    // value id

    if (Stream.EmitBlockInfoAbbrev(naclbitc::CONSTANTS_BLOCK_ID,
                                   Abbv) != CONSTANTS_CE_CAST_Abbrev)
      llvm_unreachable("Unexpected abbrev ordering!");
  }
  { // NULL abbrev for CONSTANTS_BLOCK.
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::CST_CODE_NULL));
    if (Stream.EmitBlockInfoAbbrev(naclbitc::CONSTANTS_BLOCK_ID,
                                   Abbv) != CONSTANTS_NULL_Abbrev)
      llvm_unreachable("Unexpected abbrev ordering!");
  }

  // FIXME: This should only use space for first class types!

  { // INST_LOAD abbrev for FUNCTION_BLOCK.
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::FUNC_CODE_INST_LOAD));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6)); // Ptr
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 4)); // Align
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, 1)); // volatile
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
  { // INST_BINOP_FLAGS abbrev for FUNCTION_BLOCK.
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::FUNC_CODE_INST_BINOP));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6)); // LHS
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6)); // RHS
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, 4)); // opc
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, 7)); // flags
    if (Stream.EmitBlockInfoAbbrev(naclbitc::FUNCTION_BLOCK_ID,
                                   Abbv) != FUNCTION_INST_BINOP_FLAGS_ABBREV)
      llvm_unreachable("Unexpected abbrev ordering!");
  }
  { // INST_CAST abbrev for FUNCTION_BLOCK.
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::FUNC_CODE_INST_CAST));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));    // OpVal
    Abbv->Add(NaClBitCodeAbbrevOp(
        NaClBitCodeAbbrevOp::Fixed,                                 // dest ty
        NaClBitsNeededForValue(VE.getTypes().size())));
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
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));
    if (Stream.EmitBlockInfoAbbrev(naclbitc::FUNCTION_BLOCK_ID,
                                   Abbv) != FUNCTION_INST_FORWARDTYPEREF_ABBREV)
      llvm_unreachable("Unexpected abbrev ordering!");
  }

  { // Abbrev for METADATA_STRING.
    NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
    Abbv->Add(NaClBitCodeAbbrevOp(naclbitc::METADATA_STRING));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Array));
    Abbv->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, 8));
    if (Stream.EmitBlockInfoAbbrev(naclbitc::METADATA_BLOCK_ID,
                                   Abbv) != METADATA_STRING_ABBREV)
      llvm_unreachable("Unexpected abbrev ordering!");
  }

  Stream.ExitBlock();
}

/// WriteModule - Emit the specified module to the bitstream.
static void WriteModule(const Module *M, NaClBitstreamWriter &Stream) {
  DEBUG(dbgs() << "-> WriteModule\n");
  Stream.EnterSubblock(naclbitc::MODULE_BLOCK_ID, MODULE_MAX_ABBREV);

  SmallVector<unsigned, 1> Vals;
  unsigned CurVersion = 1;
  Vals.push_back(CurVersion);
  Stream.EmitRecord(naclbitc::MODULE_CODE_VERSION, Vals);

  // Analyze the module, enumerating globals, functions, etc.
  NaClValueEnumerator VE(M);

  // Emit blockinfo, which defines the standard abbreviations etc.
  WriteBlockInfo(VE, Stream);

  // Emit information describing all of the types in the module.
  WriteTypeTable(VE, Stream);

  // Emit top-level description of module, including inline asm,
  // descriptors for global variables, and function prototype info.
  WriteModuleInfo(M, VE, Stream);

  // Emit constants.
  WriteModuleConstants(VE, Stream);

  // Emit metadata.
  WriteModuleMetadata(M, VE, Stream);

  // Emit metadata.
  WriteModuleMetadataStore(M, Stream);

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

// Write out the given fields to the bitstream.
static void WriteHeaderFields(
    const std::vector<NaClBitcodeHeaderField*> &Fields,
    NaClBitstreamWriter& Stream) {
  // Emit placeholder for number of bytes used to hold header fields.
  // This value is necessary so that the streamable reader can preallocate
  // a buffer to read the fields.
  Stream.Emit(0, naclbitc::BlockSizeWidth);
  unsigned BytesForHeader = 0;

  unsigned NumberFields = Fields.size();
  if (NumberFields > 0xFFFF)
    report_fatal_error("Too many header fields");

  uint8_t Buffer[kMaxVariableFieldSize];
  for (std::vector<NaClBitcodeHeaderField*>::const_iterator
           Iter = Fields.begin(), IterEnd = Fields.end();
       Iter != IterEnd; ++Iter) {
    if (!(*Iter)->Write(Buffer, kMaxVariableFieldSize))
      report_fatal_error("Header field too big to generate");
    size_t limit = (*Iter)->GetTotalSize();
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

// Define the version of PNaCl bitcode we are generating.
static const uint16_t kPNaClVersion = 1;

/// WriteBitcodeToFile - Write the specified module to the specified output
/// stream.
void llvm::NaClWriteBitcodeToFile(const Module *M, raw_ostream &Out) {
  SmallVector<char, 0> Buffer;
  Buffer.reserve(256*1024);

  // Convert Deplib info to metadata
  M->convertLibraryListToMetadata(); // @LOCALMOD

  // Emit the module into the buffer.
  {
    NaClBitstreamWriter Stream(Buffer);

    // Emit the file header.
    Stream.Emit((unsigned)'P', 8);
    Stream.Emit((unsigned)'E', 8);
    Stream.Emit((unsigned)'X', 8);
    Stream.Emit((unsigned)'E', 8);

    // Collect header fields to add.
    {
      std::vector<NaClBitcodeHeaderField*> HeaderFields;
      HeaderFields.push_back(
          new NaClBitcodeHeaderField(NaClBitcodeHeaderField::kPNaClVersion,
                                     kPNaClVersion));
      WriteHeaderFields(HeaderFields, Stream);
    }

    // Emit the module.
    WriteModule(M, Stream);
  }

  // Write the generated bitstream to "Out".
  Out.write((char*)&Buffer.front(), Buffer.size());
}
