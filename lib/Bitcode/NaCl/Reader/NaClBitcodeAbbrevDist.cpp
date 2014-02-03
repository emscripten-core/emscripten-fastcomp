//===-- NaClBitcodeAbbrevDist.cpp -------------------------------------------===//
//      Implements distribution maps for abbreviations associated with
//      bitcode records.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/NaCl/NaClBitcodeAbbrevDist.h"

using namespace llvm;

NaClBitcodeAbbrevDistElement::~NaClBitcodeAbbrevDistElement() {}

NaClBitcodeDistElement *NaClBitcodeAbbrevDistElement::CreateElement(
    NaClBitcodeDistValue Value) const {
  return new NaClBitcodeAbbrevDistElement();
}

void NaClBitcodeAbbrevDistElement::
GetValueList(const NaClBitcodeRecord &Record,
             ValueListType &ValueList) const {
  ValueList.push_back(Record.GetAbbreviationIndex());
}

void NaClBitcodeAbbrevDistElement::AddRecord(const NaClBitcodeRecord &Record) {
  NaClBitcodeDistElement::AddRecord(Record);
  CodeDist.AddRecord(Record);
}

const char *NaClBitcodeAbbrevDistElement::GetTitle() const {
  return "Abbreviation Indices";
}

const char *NaClBitcodeAbbrevDistElement::GetValueHeader() const {
  return "  Index";
}

void NaClBitcodeAbbrevDistElement::
PrintRowValue(raw_ostream &Stream,
              NaClBitcodeDistValue Value,
              const NaClBitcodeDist *Distribution) const {
  Stream << format("%7u", Value);
}

const SmallVectorImpl<NaClBitcodeDist*> *NaClBitcodeAbbrevDistElement::
GetNestedDistributions() const {
  return &NestedDists;
}

NaClBitcodeAbbrevDistElement NaClBitcodeAbbrevDistElement::Sentinel;

NaClBitcodeAbbrevDist::~NaClBitcodeAbbrevDist() {}

NaClBitcodeDistElement* NaClBitcodeAbbrevDist::
CreateElement(NaClBitcodeDistValue Value) const {
  return new NaClBitcodeAbbrevDistElement(BlockID);
}
