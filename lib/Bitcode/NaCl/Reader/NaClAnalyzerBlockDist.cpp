//===-- NaClAnalBlockDist.cpp ---------------------------------------===//
//      implements distribution maps used to collect block and record
//      distributions for tools pnacl-bcanalyzer and pnacl-benchmark.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/NaCl/NaClAnalyzerBlockDist.h"

using namespace llvm;

NaClAnalyzerBlockDistElement::~NaClAnalyzerBlockDistElement() {}

NaClBitcodeDistElement* NaClAnalyzerBlockDistElement::
CreateElement(NaClBitcodeDistValue Value) const {
  return new NaClAnalyzerBlockDistElement(Value, OrderBlocksByID);
}

double NaClAnalyzerBlockDistElement::
GetImportance(NaClBitcodeDistValue Value) const {
  if (OrderBlocksByID)
    // Negate importance to "undo" reverse ordering of sort.
    return -static_cast<double>(BlockID);
  else
    return NaClBitcodeBlockDistElement::GetImportance(Value);
}

const SmallVectorImpl<NaClBitcodeDist*> *NaClAnalyzerBlockDistElement::
GetNestedDistributions() const {
  return &NestedDists;
}

void NaClAnalyzerBlockDist::AddRecord(const NaClBitcodeRecord &Record) {
  cast<NaClAnalyzerBlockDistElement>(GetElement(Record.GetBlockID()))
      ->GetRecordDist().AddRecord(Record);
}

void NaClAnalyzerBlockDist::AddBlock(const NaClBitcodeBlock &Block) {
  NaClBitcodeBlockDist::AddBlock(Block);
  if (const NaClBitcodeBlock *EncBlock = Block.GetEnclosingBlock()) {
    cast<NaClAnalyzerBlockDistElement>(GetElement(EncBlock->GetBlockID()))
        ->GetSubblockDist().AddBlock(Block);
  }
}
