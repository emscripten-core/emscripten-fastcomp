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
#include "llvm/ADT/STLExtras.h"

#ifndef _MSC_VER // XXX Emscripten: This header doesn't exist on MSVC, not needed anyways, as builds without.
#include <inttypes.h>
#endif

using namespace llvm;

/// Constant defining cutoff for value distributions. All values less than
/// this constant will be stored as a singleton range.
static NaClBitcodeDistValue ValueDistSingletonCutoff =
    (NaClBitcodeDistValue)1 << 6;

/// Largest value possible.
static NaClBitcodeDistValue MaxValue = ~((NaClBitcodeDistValue) 0);

/// Cutoffs for value ranges, starting the first range at
/// ValueDistSingetonCutoff, and ending the last range with MaxValue.
static NaClBitcodeDistValue ValueCutoffs[] = {
  (NaClBitcodeDistValue)1 << 8,
  (NaClBitcodeDistValue)1 << 12,
  (NaClBitcodeDistValue)1 << 16,
  (NaClBitcodeDistValue)1 << 24,
  (NaClBitcodeDistValue)1 << 32
};

// Converts the value to the corresponding range (value) that will
// be stored in a value distribution map.
NaClValueRangeIndexType
llvm::GetNaClValueRangeIndex(NaClBitcodeDistValue Value) {
  if (Value < ValueDistSingletonCutoff)
    return Value;

  size_t ValueCutoffsSize = array_lengthof(ValueCutoffs);
  for (size_t i = 0; i < ValueCutoffsSize; ++i) {
    if (Value < ValueCutoffs[i]) {
      return ValueDistSingletonCutoff + i;
    }
  }

  return ValueDistSingletonCutoff + ValueCutoffsSize;
}

NaClValueRangeType llvm::GetNaClValueRange(NaClValueRangeIndexType RangeIndex) {
  if (RangeIndex < ValueDistSingletonCutoff)
    return NaClValueRangeType(RangeIndex, RangeIndex);

  size_t Index = RangeIndex - ValueDistSingletonCutoff;
  size_t ValueCutoffsSize = array_lengthof(ValueCutoffs);
  if (Index >= ValueCutoffsSize)
    return NaClValueRangeType(ValueCutoffs[ValueCutoffsSize-1], MaxValue);
  else if (Index == 0)
    return NaClValueRangeType(ValueDistSingletonCutoff, ValueCutoffs[0]);
  else
    return NaClValueRangeType(ValueCutoffs[Index-1], ValueCutoffs[Index]-1);
}

NaClBitcodeValueDistElement NaClBitcodeValueDistElement::Sentinel;

NaClBitcodeValueDistElement::~NaClBitcodeValueDistElement() {}

NaClBitcodeDistElement *NaClBitcodeValueDistElement::CreateElement(
      NaClBitcodeDistValue Value) const {
  return new NaClBitcodeValueDistElement();
}

double NaClBitcodeValueDistElement::
GetImportance(NaClBitcodeDistValue Value) const {
  NaClValueRangeType Pair = GetNaClValueRange(Value);
  return GetNumInstances() / static_cast<double>((Pair.second-Pair.first) + 1);
}

const char *NaClBitcodeValueDistElement::GetTitle() const {
  return "Values";
}

const char *NaClBitcodeValueDistElement::GetValueHeader() const {
  return "Value / Range";
}

void NaClBitcodeValueDistElement::
PrintRowValue(raw_ostream &Stream,
              NaClBitcodeDistValue Value,
              const NaClBitcodeDist *Distribution) const {
  NaClValueRangeType Range = GetNaClValueRange(Value);
  Stream << format("%" PRIu64, Range.first);
  if (Range.first != Range.second) {
    Stream << " .. " << format("%" PRIu64, Range.second);
  }
}

/// Defines the sentinel distribution element of range indices for untracked
/// indices in the bitcode records, which need a clarifying print title.
class NaClBitcodeUntrackedValueDistElement
    : public NaClBitcodeValueDistElement {
public:
  static NaClBitcodeUntrackedValueDistElement Sentinel;

  virtual ~NaClBitcodeUntrackedValueDistElement();

  virtual const char *GetTitle() const;

private:
  NaClBitcodeUntrackedValueDistElement()
      : NaClBitcodeValueDistElement()
  {}
};

NaClBitcodeUntrackedValueDistElement
NaClBitcodeUntrackedValueDistElement::Sentinel;

NaClBitcodeUntrackedValueDistElement::~NaClBitcodeUntrackedValueDistElement() {}

const char *NaClBitcodeUntrackedValueDistElement::GetTitle() const {
  return "Values for untracked indices";
}

NaClBitcodeValueDist::
NaClBitcodeValueDist(unsigned Index, bool AllRemainingIndices)
    : NaClBitcodeDist(RecordStorage,
                      (AllRemainingIndices
                       ? &NaClBitcodeUntrackedValueDistElement::Sentinel
                       : &NaClBitcodeValueDistElement::Sentinel),
                      RD_ValueDist),
      Index(Index),
      AllRemainingIndices(AllRemainingIndices) {
}

NaClBitcodeValueDist::~NaClBitcodeValueDist() {}

void NaClBitcodeValueDist::GetValueList(const NaClBitcodeRecord &Record,
                                        ValueListType &ValueList) const {
  const NaClBitcodeRecord::RecordVector &Values = Record.GetValues();
  if (AllRemainingIndices) {
    for (size_t i = Index, i_limit = Values.size(); i < i_limit; ++i) {
      ValueList.push_back(GetNaClValueRangeIndex(Values[i]));
    }
  } else {
    ValueList.push_back(GetNaClValueRangeIndex(Values[Index]));
  }
}

NaClBitcodeValueIndexDistElement NaClBitcodeValueIndexDistElement::Sentinel;

NaClBitcodeValueIndexDistElement::~NaClBitcodeValueIndexDistElement() {}

NaClBitcodeDistElement *NaClBitcodeValueIndexDistElement::CreateElement(
    NaClBitcodeDistValue Value) const {
  return new NaClBitcodeValueIndexDistElement(Value);
}

void NaClBitcodeValueIndexDistElement::
GetValueList(const NaClBitcodeRecord &Record, ValueListType &ValueList) const {
  unsigned i_limit = Record.GetValues().size();
  if (i_limit > NaClValueIndexCutoff) i_limit = NaClValueIndexCutoff;
  for (size_t i = 0; i < i_limit; ++i) {
    ValueList.push_back(i);
  }
}

double NaClBitcodeValueIndexDistElement::
GetImportance(NaClBitcodeDistValue Value) const {
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
    const NaClBitcodeValueDistElement *Elmt =
        cast<NaClBitcodeValueDistElement>(Iter->second);
    double Count = static_cast<double>(Elmt->GetImportance(Iter->first));
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
