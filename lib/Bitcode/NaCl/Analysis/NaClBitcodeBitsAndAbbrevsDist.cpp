//===- NaClBitcodeBitsAndAbbrevsDist.cpp ------------------*- C++ -*-===//
//     Implements distributions of values with the corresponding
//     number of bits and percentage of abbreviations used in PNaCl
//     bitcode records.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/NaCl/NaClBitcodeBitsAndAbbrevsDist.h"

using namespace llvm;

NaClBitcodeBitsAndAbbrevsDistElement::~NaClBitcodeBitsAndAbbrevsDistElement() {}

void NaClBitcodeBitsAndAbbrevsDistElement::
AddRecord(const NaClBitcodeRecord &Record) {
  NaClBitcodeBitsDistElement::AddRecord(Record);
  if (Record.UsedAnAbbreviation()) {
    ++NumAbbrevs;
  }
}

void NaClBitcodeBitsAndAbbrevsDistElement::
PrintStatsHeader(raw_ostream &Stream) const {
  NaClBitcodeBitsDistElement::PrintStatsHeader(Stream);
  Stream << "   % Abv";
}

void NaClBitcodeBitsAndAbbrevsDistElement::
PrintRowStats(raw_ostream &Stream,
              const NaClBitcodeDist *Distribution) const {
  NaClBitcodeBitsDistElement::PrintRowStats(Stream, Distribution);
  if (GetNumAbbrevs())
    Stream << format(" %7.2f",
                     (double) GetNumAbbrevs()/GetNumInstances()*100.0);
  else
    Stream << "        ";
}
