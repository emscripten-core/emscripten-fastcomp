//===-- NaClBitcodeValueDist.cpp ------------------------------------------===//
//      Implements (nested) distribution maps to separate out values at each
//      index in a bitcode record.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/NaCl/NaClBitcodeValueDist.h"

using namespace llvm;

NaClBitcodeValueDistElement NaClBitcodeValueDistElement::Sentinel;

NaClBitcodeValueDistElement::~NaClBitcodeValueDistElement() {}

NaClBitcodeDistElement *NaClBitcodeValueDistElement::CreateElement(
      NaClBitcodeDistValue Value) const {
  return new NaClBitcodeValueDistElement();
}

const char *NaClBitcodeValueDistElement::GetTitle() const {
  return "Values";
}

const char *NaClBitcodeValueDistElement::GetValueHeader() const {
  return "       Value";
}

void NaClBitcodeValueDistElement::
PrintRowValue(raw_ostream &Stream,
              NaClBitcodeDistValue Value,
              const NaClBitcodeDist *Distribution) const {
  Stream << format("%12u", Value);
}

NaClBitcodeValueDist::~NaClBitcodeValueDist() {}

void NaClBitcodeValueDist::GetValueList(const NaClBitcodeRecord &Record,
                                        ValueListType &ValueList) const {
  ValueList.push_back(Record.GetValues()[Index]);
}


NaClBitcodeValueIndexDistElement NaClBitcodeValueIndexDistElement::Sentinel;

NaClBitcodeValueIndexDistElement::~NaClBitcodeValueIndexDistElement() {}

NaClBitcodeDistElement *NaClBitcodeValueIndexDistElement::CreateElement(
    NaClBitcodeDistValue Value) const {
  return new NaClBitcodeValueIndexDistElement(Value);
}

void NaClBitcodeValueIndexDistElement::
GetValueList(const NaClBitcodeRecord &Record, ValueListType &ValueList) const {
  for (size_t i = 0, i_limit = Record.GetValues().size(); i < i_limit; ++i) {
    ValueList.push_back(i);
  }
}

double NaClBitcodeValueIndexDistElement::GetImportance() const {
  // Since all indices (usually) have the same number of instances,
  // that is a bad measure of importance. Rather, we will base
  // importance in terms of the value distribution for the value
  // index. We would like the indicies with a few, large instance
  // counts, to appear before value indices with a uniform value
  // distribution. To do this, we will use the sum of the squares of
  // the number of instances for each value (i.e. sort by standard
  // deviation).
  double Sum = 0.0;
  for (NaClBitcodeDist::const_iterator
           Iter = ValueDist.begin(), IterEnd = ValueDist.end();
       Iter != IterEnd; ++Iter) {
    double Count = static_cast<double>(Iter->second->GetNumInstances());
    Sum += Count * Count;
  }
  return Sum;
}

void NaClBitcodeValueIndexDistElement::
AddRecord(const NaClBitcodeRecord &Record) {
  NaClBitcodeDistElement::AddRecord(Record);
  ValueDist.AddRecord(Record);
}

const char *NaClBitcodeValueIndexDistElement::GetTitle() const {
  return "Value indices";
}

const char *NaClBitcodeValueIndexDistElement::GetValueHeader() const {
  return "  Index";
}

void NaClBitcodeValueIndexDistElement::
PrintRowValue(raw_ostream &Stream,
              NaClBitcodeDistValue Value,
              const NaClBitcodeDist *Distribution) const {
  Stream << format("%7u", Value);
}

const SmallVectorImpl<NaClBitcodeDist*> *NaClBitcodeValueIndexDistElement::
GetNestedDistributions() const {
  return &NestedDists;
}
