//===- NaClBitcodeBitsAndAbbrevsDist.h --------------------*- C++ -*-===//
//     Maps distributions of values with corresponding number of bits,
//     and percentage of abbreviations used in PNaCl bitcode records.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Creates a distribution map of values and the
// correspdonding bits and abbreviations in PNaCl bitcode records.

#ifndef LLVM_BITCODE_NACL_NACLBITCODEBITSANDABBREVSDIST_H
#define LLVM_BITCODE_NACL_NACLBITCODEBITSANDABBREVSDIST_H

#include "llvm/Bitcode/NaCl/NaClBitcodeBitsDist.h"

namespace llvm {

/// Defines the element type of a PNaCl bitcode distribution map when
/// we want to count both the number of instances, and the number of
/// bits used by each record. Also tracks the number to times an
/// abbreviation was used to parse the corresponding record.
class NaClBitcodeBitsAndAbbrevsDistElement : public NaClBitcodeBitsDistElement {
  NaClBitcodeBitsAndAbbrevsDistElement(
      const NaClBitcodeBitsAndAbbrevsDistElement&) = delete;
  void operator=(const NaClBitcodeBitsAndAbbrevsDistElement&) = delete;

public:
  static bool classof(const NaClBitcodeDistElement *Dist) {
    return Dist->getKind() >= RDE_BitsAndAbbrevsDist
        && Dist->getKind() < RDE_BitsAndAbbrevsDistLast;
  }

  // Create an element with no instances.
  explicit NaClBitcodeBitsAndAbbrevsDistElement(
      NaClBitcodeDistElementKind Kind=RDE_BitsAndAbbrevsDist)
      : NaClBitcodeBitsDistElement(Kind),
        NumAbbrevs(0)
  {}

  virtual ~NaClBitcodeBitsAndAbbrevsDistElement();

  virtual void AddRecord(const NaClBitcodeRecord &Record);

  // Note: No AddBlock method override because abbrevations only
  // apply to records.

  // Returns the number of times an abbreviation was used to represent
  // the value.
  unsigned GetNumAbbrevs() const {
    return NumAbbrevs;
  }

  virtual void PrintStatsHeader(raw_ostream &Stream) const;

  virtual void PrintRowStats(raw_ostream &Stream,
                             const NaClBitcodeDist *Distribution) const;

private:
  // Number of times an abbreviation is used for the value.
  unsigned NumAbbrevs;
};

}

#endif
