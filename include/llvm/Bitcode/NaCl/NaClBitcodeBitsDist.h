//===- NaClBitcodeBitsDist.h ------------------------------------*- C++ -*-===//
//     Maps distributions of values and corresponding number of
//     bits in PNaCl bitcode records.
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
// on top of the NaClBitcodeDistElement class.

#ifndef LLVM_BITCODE_NACL_NACLBITCODEBITSDIST_H
#define LLVM_BITCODE_NACL_NACLBITCODEBITSDIST_H

#include "llvm/Bitcode/NaCl/NaClBitcodeDist.h"

namespace llvm {

/// Defines the element type of a PNaCl bitcode distribution map when
/// we want to count both the number of instances, and the number of
/// bits used by each record. Also tracks the number to times an
/// abbreviation was used to parse the corresponding record.
class NaClBitcodeBitsDistElement : public NaClBitcodeDistElement {
  NaClBitcodeBitsDistElement(const NaClBitcodeBitsDistElement&) = delete;
  void operator=(const NaClBitcodeBitsDistElement&) = delete;

public:
  static bool classof(const NaClBitcodeDistElement *Dist) {
    return Dist->getKind() >= RDE_BitsDist
        && Dist->getKind() < RDE_BitsDistLast;
  }

  // Create an element with no instances.
  explicit NaClBitcodeBitsDistElement(
      NaClBitcodeDistElementKind Kind=RDE_BitsDist)
      : NaClBitcodeDistElement(Kind),
        TotalBits(0)
  {}

  virtual ~NaClBitcodeBitsDistElement();

  virtual void AddRecord(const NaClBitcodeRecord &Record);

  virtual void AddBlock(const NaClBitcodeBlock &Block);

  // Returns the total number of bits used to represent all instances
  // of this value.
  uint64_t GetTotalBits() const {
    return TotalBits;
  }

  virtual void PrintStatsHeader(raw_ostream &Stream) const;

  virtual void PrintRowStats(raw_ostream &Stream,
                             const NaClBitcodeDist *Distribution) const;

private:
  // Number of bits used to represent all instances of the value.
  uint64_t TotalBits;
};

}

#endif
