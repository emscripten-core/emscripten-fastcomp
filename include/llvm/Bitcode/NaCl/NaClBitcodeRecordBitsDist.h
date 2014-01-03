//===- NaClBitcodeRecordBitsDist.h -----------------------------*- C++ -*-===//
//     Maps distributions of values and corresponding number of
//     bits in PNaCl bitcode record distributions.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Creates a (nestable) distribution map of values, and the correspdonding
// bits, in PNaCl bitcode records. These distributions are built directly
// on top of NaClBitcodeRecordDist and NaClBitcodeRecordDistElement classes.
// See (included) file NaClBitcodeRecordDist.h for more details on these
// classes, and how you should use them.

#ifndef LLVM_BITCODE_NACL_NACLBITCODERECORDBITSDIST_H
#define LLVM_BITCODE_NACL_NACLBITCODERECORDBITSDIST_H

#include "llvm/Bitcode/NaCl/NaClBitcodeRecordDist.h"

namespace llvm {

/// Defines the element type of a PNaCl bitcode distribution map when
/// we want to count both the number of instances, and the number of
/// bits used by each record. Also tracks the number to times an
/// abbreviation was used to parse the corresponding record.
class NaClBitcodeRecordBitsDistElement : public NaClBitcodeRecordDistElement {
  NaClBitcodeRecordBitsDistElement(const NaClBitcodeRecordBitsDistElement&)
      LLVM_DELETED_FUNCTION;
  void operator=(const NaClBitcodeRecordBitsDistElement&)
      LLVM_DELETED_FUNCTION;

public:
  static bool classof(const NaClBitcodeRecordDistElement *Dist) {
    return Dist->getKind() >= RDE_BitsDist
        && Dist->getKind() < RDE_BitsDist_Last;
  }

  // Create an element with no instances.
  NaClBitcodeRecordBitsDistElement(
      NaClBitcodeRecordDist* NestedDist = 0,
      NaClBitcodeRecordDistElementKind Kind=RDE_BitsDist)
      : NaClBitcodeRecordDistElement(NestedDist, Kind),
        TotalBits(0), NumAbbrevs(0)
  {}

  virtual ~NaClBitcodeRecordBitsDistElement();

  virtual void Add(const NaClBitcodeRecord &Record);

  // Returns the total number of bits used to represent all instances
  // of this value.
  uint64_t GetTotalBits() const {
    return TotalBits;
  }

  // Returns the number of times an abbreviation was used to represent
  // the value.
  unsigned GetNumAbbrevs() const {
    return NumAbbrevs;
  }

private:
  // Number of bits used to represent all instances of the value.
  uint64_t TotalBits;
  // Number of times an abbreviation is used for the value.
  unsigned NumAbbrevs;
};

/// Defines a PNaCl bitcode distribution map when we want to count
/// both the number of instances, and the number of bits used by each
/// record. Assumes distribution elements are instances of
/// NaClBitcodeRecordBitsDistElement.
class NaClBitcodeRecordBitsDist : public NaClBitcodeRecordDist {
  NaClBitcodeRecordBitsDist(const NaClBitcodeRecordBitsDist&)
      LLVM_DELETED_FUNCTION;
  void operator=(const NaClBitcodeRecordBitsDist&)
      LLVM_DELETED_FUNCTION;

public:

  static bool classof(const NaClBitcodeRecordDist *Dist) {
    return Dist->getKind() >= RD_BitsDist
        && Dist->getKind() < RD_BitsDist_Last;
  }

  NaClBitcodeRecordBitsDist(NaClBitcodeRecordDistKind Kind=RD_BitsDist)
      : NaClBitcodeRecordDist(Kind)
  {}

  virtual ~NaClBitcodeRecordBitsDist();


protected:
  virtual NaClBitcodeRecordDistElement *
  CreateElement(NaClBitcodeRecordDistValue Value);

  virtual void PrintRowStats(raw_ostream &Stream,
                             const std::string &Indent,
                             NaClBitcodeRecordDistValue Value) const;

  virtual void PrintHeader(raw_ostream &Stream,
                           const std::string &Indent) const;
};

}

#endif
