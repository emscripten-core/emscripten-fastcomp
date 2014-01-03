//===- NaClBitcodeRecordBitsDist.cpp ---------------------------*- C++ -*-===//
//     Implements distributions of values with the corresponding number
//     of bits in PNaCl bitcode records.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/NaCl/NaClBitcodeRecordBitsDist.h"

using namespace llvm;

NaClBitcodeRecordBitsDistElement::~NaClBitcodeRecordBitsDistElement() {}

void NaClBitcodeRecordBitsDistElement::Add(const NaClBitcodeRecord &Record) {
  NaClBitcodeRecordDistElement::Add(Record);
  TotalBits += Record.GetNumBits();
  if (Record.UsedAnAbbreviation()) {
    ++NumAbbrevs;
  }
}

NaClBitcodeRecordBitsDist::~NaClBitcodeRecordBitsDist() {}

NaClBitcodeRecordDistElement *NaClBitcodeRecordBitsDist::
CreateElement(NaClBitcodeRecordDistValue Value) {
  return new NaClBitcodeRecordBitsDistElement();
}


void NaClBitcodeRecordBitsDist::
PrintRowStats(raw_ostream &Stream,
              const std::string &Indent,
              NaClBitcodeRecordDistValue Value) const {
  NaClBitcodeRecordBitsDistElement *Element =
      cast<NaClBitcodeRecordBitsDistElement>(this->at(Value));
  Stream << Indent
         << format("%7d %6.2f %9lu ",
                   Element->GetNumInstances(),
                   (double) Element->GetNumInstances()/
                   this->GetTotal()*100.0,
                   (unsigned long) Element->GetTotalBits())
         << format("%9.2f",
                   (double) Element->GetTotalBits()/
                   Element->GetNumInstances());
  if (Element->GetNumAbbrevs())
    Stream << format(" %7.2f  ",
                     (double) Element->GetNumAbbrevs()/
                     Element->GetNumInstances()*100.0);
  else
    Stream << "          ";
}

void NaClBitcodeRecordBitsDist::
PrintHeader(raw_ostream &Stream, const std::string &Indent) const {
  Stream << Indent
         << "  Count %Total    # Bits Bits/Elmt   % Abv  "
         << this->GetValueHeader() << "\n";
}
