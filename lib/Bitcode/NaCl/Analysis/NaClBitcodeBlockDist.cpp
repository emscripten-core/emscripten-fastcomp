//===-- NaClBitcodeBlockDist.cpp ---------------------------------------===//
//      implements distribution maps for blocks within PNaCl bitcode.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/NaCl/NaClBitcodeBlockDist.h"
#include "llvm/Bitcode/NaCl/NaClLLVMBitCodes.h"

using namespace llvm;

/// GetBlockName - Return a symbolic block name if known, otherwise return
/// null.
static const char *GetBlockName(unsigned BlockID) {
  // Standard blocks for all bitcode files.
  if (BlockID < naclbitc::FIRST_APPLICATION_BLOCKID) {
    if (BlockID == naclbitc::BLOCKINFO_BLOCK_ID)
      return "BLOCKINFO_BLOCK";
    return 0;
  }

  switch (BlockID) {
  default: return 0;
  case naclbitc::MODULE_BLOCK_ID:          return "MODULE_BLOCK";
  case naclbitc::PARAMATTR_BLOCK_ID:       return "PARAMATTR_BLOCK";
  case naclbitc::PARAMATTR_GROUP_BLOCK_ID: return "PARAMATTR_GROUP_BLOCK_ID";
  case naclbitc::TYPE_BLOCK_ID_NEW:        return "TYPE_BLOCK_ID";
  case naclbitc::CONSTANTS_BLOCK_ID:       return "CONSTANTS_BLOCK";
  case naclbitc::FUNCTION_BLOCK_ID:        return "FUNCTION_BLOCK";
  case naclbitc::VALUE_SYMTAB_BLOCK_ID:    return "VALUE_SYMTAB";
  case naclbitc::METADATA_BLOCK_ID:        return "METADATA_BLOCK";
  case naclbitc::METADATA_ATTACHMENT_ID:   return "METADATA_ATTACHMENT_BLOCK";
  case naclbitc::USELIST_BLOCK_ID:         return "USELIST_BLOCK_ID";
  case naclbitc::GLOBALVAR_BLOCK_ID:       return "GLOBALVAR_BLOCK";
  }
}

NaClBitcodeBlockDistElement NaClBitcodeBlockDist::DefaultSentinal;

NaClBitcodeBlockDistElement::~NaClBitcodeBlockDistElement() {}

NaClBitcodeDistElement *NaClBitcodeBlockDistElement::
CreateElement(NaClBitcodeDistValue Value) const {
  return new NaClBitcodeBlockDistElement();
}

double NaClBitcodeBlockDistElement::
GetImportance(NaClBitcodeDistValue Value) const {
  return static_cast<double>(GetTotalBits());
}

const char *NaClBitcodeBlockDistElement::GetTitle() const {
  return "Block Histogram";
}

const char *NaClBitcodeBlockDistElement::GetValueHeader() const {
  return "Block";
}

void NaClBitcodeBlockDistElement::PrintStatsHeader(raw_ostream &Stream) const {
  Stream << "  %File";
  NaClBitcodeBitsDistElement::PrintStatsHeader(Stream);
}

void NaClBitcodeBlockDistElement::
PrintRowStats(raw_ostream &Stream, const NaClBitcodeDist *Distribution) const {
  const NaClBitcodeBlockDist *BlockDist =
      cast<NaClBitcodeBlockDist>(Distribution);
  Stream << format(" %6.2f",
                   (double) GetTotalBits()/BlockDist->GetTotalBits()*100.00);
  NaClBitcodeBitsDistElement::PrintRowStats(Stream, Distribution);
}

void NaClBitcodeBlockDistElement::
PrintRowValue(raw_ostream &Stream, NaClBitcodeDistValue Value,
              const NaClBitcodeDist *Distribution) const {
  Stream << NaClBitcodeBlockDist::GetName(Value);
}

NaClBitcodeBlockDist::~NaClBitcodeBlockDist() {}

uint64_t NaClBitcodeBlockDist::GetTotalBits() const {
  uint64_t Total = 0;
  for (NaClBitcodeDist::const_iterator Iter = begin(), IterEnd = end();
       Iter != IterEnd; ++Iter) {
    const NaClBitcodeBlockDistElement *Element =
        cast<NaClBitcodeBlockDistElement>(Iter->second);
    Total += Element->GetTotalBits();
  }
  return Total;
}

std::string NaClBitcodeBlockDist::GetName(unsigned BlockID) {
  if (const char *BlockName = GetBlockName(BlockID)) {
    return BlockName;
  }

  std::string Str;
  raw_string_ostream StrStrm(Str);
  StrStrm << "UnknownBlock" << BlockID;
  return StrStrm.str();
}
