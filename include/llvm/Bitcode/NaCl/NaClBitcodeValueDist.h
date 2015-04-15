//===-- NaClBitcodeValueDist.h ---------------------------------------------===//
//      Defines distribution maps to separate out values at each index
//      in a bitcode record.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_BITCODE_NACL_NACLBITCODEVALUEDIST_H
#define LLVM_BITCODE_NACL_NACLBITCODEVALUEDIST_H

#include "llvm/Bitcode/NaCl/NaClBitcodeDist.h"

namespace llvm {

/// Defines the value index cutoff where we no longer track values associated
/// with specific value indices of bitcode records.
static const unsigned NaClValueIndexCutoff = 6;

/// Defines an (inclusive) range of values in a bitcode record,
/// defined by the given pair of values. Note that values are stored
/// as ranges. For small values, each value is in a separate range, so
/// that potential constants for abbreviations can be found. For
/// larger values, values are coalesced together into multiple element
/// ranges, since we don't look for constants and are only interested
/// in the overall distribution of values.
typedef std::pair<NaClBitcodeDistValue,
                  NaClBitcodeDistValue> NaClValueRangeType;

/// Models a range index. Ranges are encoded as a consecutive sequence
/// of indices, starting at zero. The actual ranges chosen to
/// represent bitcode records are internal, and is defined by function
/// GetValueRange index below.
typedef NaClBitcodeDistValue NaClValueRangeIndexType;

/// Converts a bitcode record value to the corresponding range index that
/// contains the value.
NaClValueRangeIndexType GetNaClValueRangeIndex(NaClBitcodeDistValue Value);

/// Converts a range index into the corresponding range of values.
NaClValueRangeType GetNaClValueRange(NaClValueRangeIndexType RangeIndex);

/// Defines the distribution of range indices.
class NaClBitcodeValueDistElement : public NaClBitcodeDistElement {
  NaClBitcodeValueDistElement(const NaClBitcodeValueDistElement&) = delete;
  void operator=(const NaClBitcodeValueDistElement&) = delete;

public:
  static bool classof(const NaClBitcodeDistElement *Element) {
    return Element->getKind() >= RDE_ValueDist &&
        Element->getKind() < RDE_ValueDistLast;
  }

  NaClBitcodeValueDistElement()
      : NaClBitcodeDistElement(RDE_ValueDist)
  {}

  static NaClBitcodeValueDistElement Sentinel;

  virtual ~NaClBitcodeValueDistElement();

  virtual NaClBitcodeDistElement *CreateElement(
      NaClBitcodeDistValue Value) const;

  /// Returns the number of instances, normalized over the
  /// range of values, using a uniform distribution.
  virtual double GetImportance(NaClBitcodeDistValue Value) const;

  virtual const char *GetTitle() const;

  virtual const char *GetValueHeader() const;

  virtual void PrintRowValue(raw_ostream &Stream,
                             NaClBitcodeDistValue Value,
                             const NaClBitcodeDist *Distribution) const;
};

/// Defines the distribution of values for a set of value indices for
/// bitcode records.
class NaClBitcodeValueDist : public NaClBitcodeDist {
  NaClBitcodeValueDist(const NaClBitcodeValueDist&) = delete;
  void operator=(const NaClBitcodeValueDist&) = delete;

public:
  static bool classof(const NaClBitcodeDist *Dist) {
    return Dist->getKind() >= RD_ValueDist &&
        Dist->getKind() < RD_ValueDistLast;
  }

  /// Builds a value distribution for the given set of value indices.
  /// If AllRemainingIndices is false, only value Index is considered.
  /// Otherwise, builds a value distribution for all values stored in
  /// record value indices >= Index.
  explicit NaClBitcodeValueDist(unsigned Index,
                                bool AllRemainingIndices=false);

  virtual ~NaClBitcodeValueDist();

  unsigned GetIndex() const {
    return Index;
  }

  bool HoldsAllRemainingIndices() const {
    return AllRemainingIndices;
  }

  virtual void GetValueList(const NaClBitcodeRecord &Record,
                            ValueListType &ValueList) const;

private:
  // The range index being tracked.
  unsigned Index;

  // If true, then tracks all indices >= Index. Otherwise only Index.
  bool AllRemainingIndices;
};

/// Defines the value distribution for each value index in corresponding
/// bitcode records. This is a helper class used to separate each element
/// in the bitcode record, so that we can determine the proper abbreviation
/// for each element.
class NaClBitcodeValueIndexDistElement : public NaClBitcodeDistElement {
  NaClBitcodeValueIndexDistElement(
      const NaClBitcodeValueIndexDistElement&) = delete;
  void operator=(
      const NaClBitcodeValueIndexDistElement&) = delete;

public:
  static bool classof(const NaClBitcodeDistElement *Element) {
    return Element->getKind() <= RDE_ValueIndexDist &&
        Element->getKind() < RDE_ValueIndexDistLast;
  }

  explicit NaClBitcodeValueIndexDistElement(unsigned Index=0)
      : NaClBitcodeDistElement(RDE_ValueIndexDist),
        ValueDist(Index) {
    NestedDists.push_back(&ValueDist);
  }

  static NaClBitcodeValueIndexDistElement Sentinel;

  virtual ~NaClBitcodeValueIndexDistElement();

  virtual NaClBitcodeDistElement *CreateElement(
      NaClBitcodeDistValue Value) const;

  virtual void GetValueList(const NaClBitcodeRecord &Record,
                            ValueListType &ValueList) const;

  virtual double GetImportance(NaClBitcodeDistValue Value) const;

  virtual void AddRecord(const NaClBitcodeRecord &Record);

  virtual const char *GetTitle() const;

  virtual const char *GetValueHeader() const;

  virtual void PrintRowValue(raw_ostream &Stream,
                             NaClBitcodeDistValue Value,
                             const NaClBitcodeDist *Distribution) const;

  virtual const SmallVectorImpl<NaClBitcodeDist*> *
  GetNestedDistributions() const;

  NaClBitcodeValueDist &GetValueDist() {
    return ValueDist;
  }

private:
  // Nested blocks used by GetNestedDistributions.
  SmallVector<NaClBitcodeDist*, 1> NestedDists;

  // The value distribution associated with the given index.
  NaClBitcodeValueDist ValueDist;
};

}

#endif
