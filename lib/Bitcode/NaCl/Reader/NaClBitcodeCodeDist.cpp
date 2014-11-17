//===-- NaClBitcodeCodeDist.cpp -------------------------------------------===//
//      Implements distribution maps for record codes within a PNaCl bitcode
//      file.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/NaCl/NaClBitcodeCodeDist.h"
#include "llvm/Bitcode/NaCl/NaClLLVMBitCodes.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

/// GetCodeName - Return a symbolic code name if known, otherwise return
/// null.
static const char *GetCodeName(unsigned CodeID, unsigned BlockID) {
  // Standard blocks for all bitcode files.
  if (BlockID < naclbitc::FIRST_APPLICATION_BLOCKID) {
    if (BlockID == naclbitc::BLOCKINFO_BLOCK_ID) {
      switch (CodeID) {
      default: return 0;
      case naclbitc::BLOCKINFO_CODE_SETBID:        return "SETBID";
      }
    }
    return 0;
  }

  switch (BlockID) {
  default: return 0;
  case naclbitc::MODULE_BLOCK_ID:
    switch (CodeID) {
    default: return 0;
    case naclbitc::MODULE_CODE_VERSION:     return "VERSION";
    case naclbitc::MODULE_CODE_TRIPLE:      return "TRIPLE";
    case naclbitc::MODULE_CODE_DATALAYOUT:  return "DATALAYOUT";
    case naclbitc::MODULE_CODE_ASM:         return "ASM";
    case naclbitc::MODULE_CODE_SECTIONNAME: return "SECTIONNAME";
    case naclbitc::MODULE_CODE_DEPLIB:      return "DEPLIB"; // FIXME: Remove in 4.0
    case naclbitc::MODULE_CODE_GLOBALVAR:   return "GLOBALVAR";
    case naclbitc::MODULE_CODE_FUNCTION:    return "FUNCTION";
    case naclbitc::MODULE_CODE_ALIAS:       return "ALIAS";
    case naclbitc::MODULE_CODE_PURGEVALS:   return "PURGEVALS";
    case naclbitc::MODULE_CODE_GCNAME:      return "GCNAME";
    }
  case naclbitc::PARAMATTR_BLOCK_ID:
    switch (CodeID) {
    default: return 0;
    case naclbitc::PARAMATTR_CODE_ENTRY_OLD: return "ENTRY";
    case naclbitc::PARAMATTR_CODE_ENTRY:     return "ENTRY";
    case naclbitc::PARAMATTR_GRP_CODE_ENTRY: return "ENTRY";
    }
  case naclbitc::TYPE_BLOCK_ID_NEW:
    switch (CodeID) {
    default: return 0;
    case naclbitc::TYPE_CODE_NUMENTRY:     return "NUMENTRY";
    case naclbitc::TYPE_CODE_VOID:         return "VOID";
    case naclbitc::TYPE_CODE_FLOAT:        return "FLOAT";
    case naclbitc::TYPE_CODE_DOUBLE:       return "DOUBLE";
    case naclbitc::TYPE_CODE_LABEL:        return "LABEL";
    case naclbitc::TYPE_CODE_OPAQUE:       return "OPAQUE";
    case naclbitc::TYPE_CODE_INTEGER:      return "INTEGER";
    case naclbitc::TYPE_CODE_POINTER:      return "POINTER";
    case naclbitc::TYPE_CODE_ARRAY:        return "ARRAY";
    case naclbitc::TYPE_CODE_VECTOR:       return "VECTOR";
    case naclbitc::TYPE_CODE_X86_FP80:     return "X86_FP80";
    case naclbitc::TYPE_CODE_FP128:        return "FP128";
    case naclbitc::TYPE_CODE_PPC_FP128:    return "PPC_FP128";
    case naclbitc::TYPE_CODE_METADATA:     return "METADATA";
    case naclbitc::TYPE_CODE_STRUCT_ANON:  return "STRUCT_ANON";
    case naclbitc::TYPE_CODE_STRUCT_NAME:  return "STRUCT_NAME";
    case naclbitc::TYPE_CODE_STRUCT_NAMED: return "STRUCT_NAMED";
    case naclbitc::TYPE_CODE_FUNCTION:     return "FUNCTION";
    }

  case naclbitc::CONSTANTS_BLOCK_ID:
    switch (CodeID) {
    default: return 0;
    case naclbitc::CST_CODE_SETTYPE:         return "SETTYPE";
    case naclbitc::CST_CODE_NULL:            return "NULL";
    case naclbitc::CST_CODE_UNDEF:           return "UNDEF";
    case naclbitc::CST_CODE_INTEGER:         return "INTEGER";
    case naclbitc::CST_CODE_WIDE_INTEGER:    return "WIDE_INTEGER";
    case naclbitc::CST_CODE_FLOAT:           return "FLOAT";
    case naclbitc::CST_CODE_AGGREGATE:       return "AGGREGATE";
    case naclbitc::CST_CODE_STRING:          return "STRING";
    case naclbitc::CST_CODE_CSTRING:         return "CSTRING";
    case naclbitc::CST_CODE_CE_BINOP:        return "CE_BINOP";
    case naclbitc::CST_CODE_CE_CAST:         return "CE_CAST";
    case naclbitc::CST_CODE_CE_GEP:          return "CE_GEP";
    case naclbitc::CST_CODE_CE_INBOUNDS_GEP: return "CE_INBOUNDS_GEP";
    case naclbitc::CST_CODE_CE_SELECT:       return "CE_SELECT";
    case naclbitc::CST_CODE_CE_EXTRACTELT:   return "CE_EXTRACTELT";
    case naclbitc::CST_CODE_CE_INSERTELT:    return "CE_INSERTELT";
    case naclbitc::CST_CODE_CE_SHUFFLEVEC:   return "CE_SHUFFLEVEC";
    case naclbitc::CST_CODE_CE_CMP:          return "CE_CMP";
    case naclbitc::CST_CODE_INLINEASM:       return "INLINEASM";
    case naclbitc::CST_CODE_CE_SHUFVEC_EX:   return "CE_SHUFVEC_EX";
    case naclbitc::CST_CODE_BLOCKADDRESS:    return "CST_CODE_BLOCKADDRESS";
    case naclbitc::CST_CODE_DATA:            return "DATA";
    }
  case naclbitc::FUNCTION_BLOCK_ID:
    switch (CodeID) {
    default: return 0;
    case naclbitc::FUNC_CODE_DECLAREBLOCKS: return "DECLAREBLOCKS";

    case naclbitc::FUNC_CODE_INST_BINOP:        return "INST_BINOP";
    case naclbitc::FUNC_CODE_INST_CAST:         return "INST_CAST";
    case naclbitc::FUNC_CODE_INST_GEP:          return "INST_GEP";
    case naclbitc::FUNC_CODE_INST_INBOUNDS_GEP: return "INST_INBOUNDS_GEP";
    case naclbitc::FUNC_CODE_INST_SELECT:       return "INST_SELECT";
    case naclbitc::FUNC_CODE_INST_EXTRACTELT:   return "INST_EXTRACTELT";
    case naclbitc::FUNC_CODE_INST_INSERTELT:    return "INST_INSERTELT";
    case naclbitc::FUNC_CODE_INST_SHUFFLEVEC:   return "INST_SHUFFLEVEC";
    case naclbitc::FUNC_CODE_INST_CMP:          return "INST_CMP";

    case naclbitc::FUNC_CODE_INST_RET:          return "INST_RET";
    case naclbitc::FUNC_CODE_INST_BR:           return "INST_BR";
    case naclbitc::FUNC_CODE_INST_SWITCH:       return "INST_SWITCH";
    case naclbitc::FUNC_CODE_INST_INVOKE:       return "INST_INVOKE";
    case naclbitc::FUNC_CODE_INST_UNREACHABLE:  return "INST_UNREACHABLE";

    case naclbitc::FUNC_CODE_INST_PHI:          return "INST_PHI";
    case naclbitc::FUNC_CODE_INST_ALLOCA:       return "INST_ALLOCA";
    case naclbitc::FUNC_CODE_INST_LOAD:         return "INST_LOAD";
    case naclbitc::FUNC_CODE_INST_VAARG:        return "INST_VAARG";
    case naclbitc::FUNC_CODE_INST_STORE:        return "INST_STORE";
    case naclbitc::FUNC_CODE_INST_EXTRACTVAL:   return "INST_EXTRACTVAL";
    case naclbitc::FUNC_CODE_INST_INSERTVAL:    return "INST_INSERTVAL";
    case naclbitc::FUNC_CODE_INST_CMP2:         return "INST_CMP2";
    case naclbitc::FUNC_CODE_INST_VSELECT:      return "INST_VSELECT";
    case naclbitc::FUNC_CODE_DEBUG_LOC_AGAIN:   return "DEBUG_LOC_AGAIN";
    case naclbitc::FUNC_CODE_INST_CALL:         return "INST_CALL";
    case naclbitc::FUNC_CODE_INST_CALL_INDIRECT: return "INST_CALL_INDIRECT";
    case naclbitc::FUNC_CODE_DEBUG_LOC:         return "DEBUG_LOC";
    case naclbitc::FUNC_CODE_INST_FORWARDTYPEREF: return "FORWARDTYPEREF";
    }
  case naclbitc::VALUE_SYMTAB_BLOCK_ID:
    switch (CodeID) {
    default: return 0;
    case naclbitc::VST_CODE_ENTRY: return "ENTRY";
    case naclbitc::VST_CODE_BBENTRY: return "BBENTRY";
    }
  case naclbitc::METADATA_ATTACHMENT_ID:
    switch(CodeID) {
    default:return 0;
    case naclbitc::METADATA_ATTACHMENT: return "METADATA_ATTACHMENT";
    }
  case naclbitc::METADATA_BLOCK_ID:
    switch(CodeID) {
    default:return 0;
    case naclbitc::METADATA_STRING:      return "METADATA_STRING";
    case naclbitc::METADATA_NAME:        return "METADATA_NAME";
    case naclbitc::METADATA_KIND:        return "METADATA_KIND";
    case naclbitc::METADATA_NODE:        return "METADATA_NODE";
    case naclbitc::METADATA_FN_NODE:     return "METADATA_FN_NODE";
    case naclbitc::METADATA_NAMED_NODE:  return "METADATA_NAMED_NODE";
    }
  case naclbitc::GLOBALVAR_BLOCK_ID:
    switch (CodeID) {
    default: return 0;
    case naclbitc::GLOBALVAR_VAR:        return "VAR";
    case naclbitc::GLOBALVAR_COMPOUND:   return "COMPOUND";
    case naclbitc::GLOBALVAR_ZEROFILL:   return "ZEROFILL";
    case naclbitc::GLOBALVAR_DATA:       return "DATA";
    case naclbitc::GLOBALVAR_RELOC:      return "RELOC";
    case naclbitc::GLOBALVAR_COUNT:      return "COUNT";
    }
  }
}

NaClBitcodeCodeDistElement::~NaClBitcodeCodeDistElement() {}

NaClBitcodeDistElement *NaClBitcodeCodeDistElement::CreateElement(
    NaClBitcodeDistValue Value) const {
  return new NaClBitcodeCodeDistElement();
}

void NaClBitcodeCodeDistElement::
GetValueList(const NaClBitcodeRecord &Record,
             ValueListType &ValueList) const {
  if (Record.GetEntryKind() == NaClBitstreamEntry::Record) {
    ValueList.push_back(Record.GetCode());
  }
}

const char *NaClBitcodeCodeDistElement::GetTitle() const {
  return "Record Histogram:";
}

const char *NaClBitcodeCodeDistElement::GetValueHeader() const {
  return "Record Kind";
}

void NaClBitcodeCodeDistElement::
PrintRowValue(raw_ostream &Stream,
              NaClBitcodeDistValue Value,
              const NaClBitcodeDist *Distribution) const {
  Stream <<
      NaClBitcodeCodeDist::
      GetCodeName(Value,
                  cast<NaClBitcodeCodeDist>(Distribution)->GetBlockID());
}

NaClBitcodeCodeDistElement NaClBitcodeCodeDist::DefaultSentinel;

NaClBitcodeCodeDist::~NaClBitcodeCodeDist() {}

std::string NaClBitcodeCodeDist::GetCodeName(unsigned CodeID,
                                             unsigned BlockID) {
  if (const char *CodeName = ::GetCodeName(CodeID, BlockID))
    return CodeName;

  std::string Str;
  raw_string_ostream StrStrm(Str);
  StrStrm << "UnknownCode" << CodeID;
  return StrStrm.str();
}
