//===- NaClBitcodeRecordDist.cpp --------------------------------*- C++ -*-===//
//     Internal implementation of PNaCl bitcode record distributions.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/NaCl/NaClBitcodeRecordDist.h"

using namespace llvm;

NaClBitcodeRecordDist::~NaClBitcodeRecordDist() {
  RemoveCachedDistribution();
  for (const_iterator Iter = begin(), IterEnd = end();
       Iter != IterEnd; ++Iter) {
    delete Iter->second;
  }
}

void NaClBitcodeRecordDist::Add(const NaClBitcodeRecord &Record) {
  ValueListType ValueList;
  this->GetValueList(Record, ValueList);
  if (!ValueList.empty()) {
    RemoveCachedDistribution();
    ++Total;
    for (ValueListIterator
             Iter = ValueList.begin(),
             IterEnd = ValueList.end();
         Iter != IterEnd; ++Iter) {
      GetElement(*Iter)->Add(Record);
    }
  }
}

void NaClBitcodeRecordDist::Print(raw_ostream &Stream,
                                  const std::string &Indent) const {
  Distribution *Dist = this->GetDistribution();
  PrintTitle(Stream, Indent);
  PrintHeader(Stream, Indent);
  for (size_t I = 0, E = Dist->size(); I != E; ++I) {
    const DistPair &Pair = Dist->at(I);
    PrintRow(Stream, Indent, Pair.second);
  }
}

NaClBitcodeRecordDistElement *NaClBitcodeRecordDist::
CreateElement(NaClBitcodeRecordDistValue Value) {
  return new NaClBitcodeRecordDistElement(CreateNestedDistributionMap());
}

NaClBitcodeRecordDist* NaClBitcodeRecordDist::CreateNestedDistributionMap() {
  return 0;
}

const char *NaClBitcodeRecordDist::GetTitle() const {
  return "Distribution";
}

const char *NaClBitcodeRecordDist::GetValueHeader() const {
  return "Value";
}

void NaClBitcodeRecordDist::PrintTitle(raw_ostream &Stream,
                                       const std::string &Indent) const {
  Stream << Indent << GetTitle() << " (" << size() << " elements):\n\n";
}

void NaClBitcodeRecordDist::
PrintRowStats(raw_ostream &Stream,
              const std::string &Indent,
              NaClBitcodeRecordDistValue Value) const {
  Stream << Indent << format("%7d ", at(Value)->GetNumInstances()) << "    ";
}

void NaClBitcodeRecordDist::
PrintRowValue(raw_ostream &Stream,
              const std::string &Indent,
              NaClBitcodeRecordDistValue Value) const {
  std::string ValueFormat;
  raw_string_ostream StrStream(ValueFormat);
  StrStream << "%" << strlen(GetValueHeader()) << "d";
  StrStream.flush();
  Stream << format(ValueFormat.c_str(), (int) Value);
  // TODO(kschimpf): Print nested distribution here if applicable.
  // Note: Indent would be used in this context.
}

void NaClBitcodeRecordDist::
PrintHeader(raw_ostream &Stream, const std::string &Indent) const {
  Stream << Indent << "  Count     " << GetValueHeader() << "\n";
}

void NaClBitcodeRecordDist::
PrintRow(raw_ostream &Stream,
         const std::string &Indent,
         NaClBitcodeRecordDistValue Value) const {
  PrintRowStats(Stream, Indent, Value);
  PrintRowValue(Stream, Indent, Value);
  Stream << "\n";
}

void NaClBitcodeRecordDist::Sort() const {
  RemoveCachedDistribution();
  CachedDistribution = new Distribution();
  for (const_iterator Iter = begin(), IterEnd = end();
       Iter != IterEnd; ++Iter) {
    const NaClBitcodeRecordDistElement *Elmt = Iter->second;
    if (double Importance = Elmt->GetImportance())
      CachedDistribution->push_back(std::make_pair(Importance, Iter->first));
  }
  // Sort in ascending order, based on importance.
  std::stable_sort(CachedDistribution->begin(),
                   CachedDistribution->end());
  // Reverse so most important appear first.
  std::reverse(CachedDistribution->begin(),
               CachedDistribution->end());
}

NaClBitcodeRecordDistElement::~NaClBitcodeRecordDistElement() {
  delete NestedDist;
}

void NaClBitcodeRecordDistElement::Add(const NaClBitcodeRecord &Record) {
  if (NestedDist) NestedDist->Add(Record);
  ++NumInstances;
}

double NaClBitcodeRecordDistElement::GetImportance() const {
  return static_cast<double>(NumInstances);
}
