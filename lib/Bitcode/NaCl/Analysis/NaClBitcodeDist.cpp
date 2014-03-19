//===- NaClBitcodeDist.cpp --------------------------------------*- C++ -*-===//
//     Internal implementation of PNaCl bitcode distributions.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/NaCl/NaClBitcodeDist.h"

using namespace llvm;

NaClBitcodeDist::~NaClBitcodeDist() {
  RemoveCachedDistribution();
  for (const_iterator Iter = begin(), IterEnd = end();
       Iter != IterEnd; ++Iter) {
    delete Iter->second;
  }
}

NaClBitcodeDistElement *NaClBitcodeDist::CreateElement(
    NaClBitcodeDistValue Value) const {
  return Sentinel->CreateElement(Value);
}

void NaClBitcodeDist::GetValueList(const NaClBitcodeRecord &Record,
                                   ValueListType &ValueList) const {
  Sentinel->GetValueList(Record, ValueList);
}

void NaClBitcodeDist::AddRecord(const NaClBitcodeRecord &Record) {
  if (StorageKind != NaClBitcodeDist::RecordStorage)
    return;
  ValueListType ValueList;
  GetValueList(Record, ValueList);
  if (!ValueList.empty()) {
    RemoveCachedDistribution();
    for (ValueListIterator
             Iter = ValueList.begin(),
             IterEnd = ValueList.end();
         Iter != IterEnd; ++Iter) {
      NaClBitcodeDistElement *Element = GetElement(*Iter);
      Element->AddRecord(Record);
      ++Total;
    }
  }
}

void NaClBitcodeDist::AddBlock(const NaClBitcodeBlock &Block) {
  if (StorageKind != NaClBitcodeDist::BlockStorage)
    return;
  RemoveCachedDistribution();
  ++Total;
  unsigned BlockID = Block.GetBlockID();
  NaClBitcodeDistElement *Element = GetElement(BlockID);
  Element->AddBlock(Block);
}

void NaClBitcodeDist::Print(raw_ostream &Stream,
                            const std::string &Indent) const {
  const Distribution *Dist = GetDistribution();
  Stream << Indent;
  Sentinel->PrintTitle(Stream, this);
  Stream << Indent;
  Sentinel->PrintHeader(Stream);
  Stream << "\n";
  bool NeedsHeader = false;
  for (size_t I = 0, E = Dist->size(); I < E; ++I) {
    if (NeedsHeader) {
      // Reprint the header so that rows are more readable.
      Stream << Indent << "  " << Sentinel->GetTitle() << " (continued)\n";
      Stream << Indent;
      Sentinel->PrintHeader(Stream);
      Stream << "\n";
    }
    const DistPair &Pair = Dist->at(I);
    Stream << Indent;
    NaClBitcodeDistElement *Element = at(Pair.second);
    Element->PrintRow(Stream, Pair.second, this);
    NeedsHeader = Element->PrintNestedDistIfApplicable(Stream, Indent);
  }
}

void NaClBitcodeDist::Sort() const {
  RemoveCachedDistribution();
  CachedDistribution = new Distribution();
  for (const_iterator Iter = begin(), IterEnd = end();
       Iter != IterEnd; ++Iter) {
    const NaClBitcodeDistElement *Elmt = Iter->second;
    // Only add if histogram element is non-empty.
    if (Elmt->GetNumInstances()) {
      double Importance = Elmt->GetImportance(Iter->first);
      CachedDistribution->push_back(std::make_pair(Importance, Iter->first));
    }
  }
  // Sort in ascending order, based on importance.
  std::stable_sort(CachedDistribution->begin(),
                   CachedDistribution->end());
  // Reverse so most important appear first.
  std::reverse(CachedDistribution->begin(),
               CachedDistribution->end());
}

NaClBitcodeDistElement::~NaClBitcodeDistElement() {}

void NaClBitcodeDistElement::AddRecord(const NaClBitcodeRecord &Record) {
  ++NumInstances;
}

void NaClBitcodeDistElement::AddBlock(const NaClBitcodeBlock &Block) {
  ++NumInstances;
}

void NaClBitcodeDistElement::GetValueList(const NaClBitcodeRecord &Record,
                                          ValueListType &ValueList) const {
  // By default, assume no record values are defined.
}

double NaClBitcodeDistElement::GetImportance(NaClBitcodeDistValue Value) const {
  return static_cast<double>(NumInstances);
}

const char *NaClBitcodeDistElement::GetTitle() const {
  return "Distribution";
}

void NaClBitcodeDistElement::
PrintTitle(raw_ostream &Stream, const NaClBitcodeDist *Distribution) const {
  Stream << GetTitle() << " (" << Distribution->size() << " elements):\n\n";
}

const char *NaClBitcodeDistElement::GetValueHeader() const {
  return "Value";
}

void NaClBitcodeDistElement::PrintStatsHeader(raw_ostream &Stream) const {
  Stream << "   Count %Count";
}

void NaClBitcodeDistElement::
PrintHeader(raw_ostream &Stream) const {
  PrintStatsHeader(Stream);
  Stream << " " << GetValueHeader();
}

void NaClBitcodeDistElement::
PrintRowStats(raw_ostream &Stream,
              const NaClBitcodeDist *Distribution) const {
  unsigned Count = GetNumInstances();
  Stream << format("%8d %6.2f",
                   Count,
                   (double) Count/Distribution->GetTotal()*100.0);
}

void NaClBitcodeDistElement::
PrintRowValue(raw_ostream &Stream,
              NaClBitcodeDistValue Value,
              const NaClBitcodeDist *Distribution) const {
  std::string ValueFormat;
  raw_string_ostream StrStream(ValueFormat);
  StrStream << "%" << strlen(GetValueHeader()) << "d";
  StrStream.flush();
  Stream << format(ValueFormat.c_str(), (int) Value);
}

void NaClBitcodeDistElement::
PrintRow(raw_ostream &Stream,
         NaClBitcodeDistValue Value,
         const NaClBitcodeDist *Distribution) const {
  PrintRowStats(Stream, Distribution);
  Stream << " ";
  PrintRowValue(Stream, Value, Distribution);
  Stream << "\n";
}

const SmallVectorImpl<NaClBitcodeDist*> *NaClBitcodeDistElement::
GetNestedDistributions() const {
  return 0;
}

bool NaClBitcodeDistElement::
PrintNestedDistIfApplicable(raw_ostream &Stream,
                            const std::string &Indent) const {
  bool PrintedNestedDists = false;
  if (const SmallVectorImpl<NaClBitcodeDist*> *Dists =
      GetNestedDistributions()) {
    for (SmallVectorImpl<NaClBitcodeDist*>::const_iterator
             Iter = Dists->begin(), IterEnd = Dists->end();
         Iter != IterEnd; ++Iter) {
      NaClBitcodeDist *Dist = *Iter;
      if (!Dist->empty()) {
        if (!PrintedNestedDists) {
          PrintedNestedDists = true;
          Stream << "\n";
        }
        Dist->Print(Stream, Indent + "    ");
        Stream << "\n";
      }
    }
  }
  return PrintedNestedDists;
}
