//===- NaClBitcodeBitsDist.cpp ----------------------------------*- C++ -*-===//
//     Implements distributions of values with the corresponding number
//     of bits in PNaCl bitcode.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/NaCl/NaClBitcodeBitsDist.h"

using namespace llvm;

NaClBitcodeBitsDistElement::~NaClBitcodeBitsDistElement() {}

void NaClBitcodeBitsDistElement::AddRecord(const NaClBitcodeRecord &Record) {
  NaClBitcodeDistElement::AddRecord(Record);
  TotalBits += Record.GetNumBits();
}

void NaClBitcodeBitsDistElement::AddBlock(const NaClBitcodeBlock &Block) {
  NaClBitcodeDistElement::AddBlock(Block);
  TotalBits += Block.GetLocalNumBits();
}

void NaClBitcodeBitsDistElement::PrintStatsHeader(raw_ostream &Stream) const {
  NaClBitcodeDistElement::PrintStatsHeader(Stream);
  Stream << "    # Bits    Bits/Elmt";
}

void NaClBitcodeBitsDistElement::
PrintRowStats(raw_ostream &Stream,
              const NaClBitcodeDist *Distribution) const {
  NaClBitcodeDistElement::PrintRowStats(Stream, Distribution);
  Stream << format(" %9lu %12.2f",
                   (unsigned long) GetTotalBits(),
                   (double) GetTotalBits()/GetNumInstances());
}
