//===-- NaClBitcodeSizeDist.cpp --------------------------------------------===//
//      Implements distribution maps for value record sizes associated with
//      bitcode records.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/NaCl/NaClBitcodeSizeDist.h"

using namespace llvm;

NaClBitcodeSizeDistElement::~NaClBitcodeSizeDistElement() {}

NaClBitcodeDistElement *NaClBitcodeSizeDistElement::CreateElement(
    NaClBitcodeDistValue Value) const {
  return new NaClBitcodeSizeDistElement();
}

void NaClBitcodeSizeDistElement::
GetValueList(const NaClBitcodeRecord &Record,
             ValueListType &ValueList) const {
  unsigned Size = Record.GetValues().size();
  // Map all sizes greater than the max value index into the same bucket.
  if (Size > NaClValueIndexCutoff) Size = NaClValueIndexCutoff;
  ValueList.push_back(Size);
}

void NaClBitcodeSizeDistElement::AddRecord(const NaClBitcodeRecord &Record) {
  NaClBitcodeDistElement::AddRecord(Record);
  ValueIndexDist.AddRecord(Record);
}

const SmallVectorImpl<NaClBitcodeDist*> *NaClBitcodeSizeDistElement::
GetNestedDistributions() const {
  return &NestedDists;
}

const char *NaClBitcodeSizeDistElement::GetTitle() const {
  return "Record sizes";
}

const char *NaClBitcodeSizeDistElement::GetValueHeader() const {
  return "   Size";
}

void NaClBitcodeSizeDistElement::
PrintRowValue(raw_ostream &Stream,
              NaClBitcodeDistValue Value,
              const NaClBitcodeDist *Distribution) const {
  Stream << format("%7u", Value);
  // Report if we merged in GetValueList.
  if (Value >= NaClValueIndexCutoff) Stream << "+";
}

NaClBitcodeSizeDistElement NaClBitcodeSizeDistElement::Sentinel;
