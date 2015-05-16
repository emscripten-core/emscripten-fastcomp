//===-- NaClBitcodeBlockDist.h -----------------------------------------===//
//      Defines distribution maps for blocks within PNaCl bitcode.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Defines a distribution map for blocks which tracks the number of bits
// in each block, as well as the percentage of the file each bitcode block
// ID holds.


#ifndef LLVM_BITCODE_NACL_NACLBITCODEBLOCKDIST_H
#define LLVM_BITCODE_NACL_NACLBITCODEBLOCKDIST_H

#include "llvm/Bitcode/NaCl/NaClBitcodeBitsDist.h"

namespace llvm {

class NaClBitcodeBlockDistElement : public NaClBitcodeBitsDistElement {
  NaClBitcodeBlockDistElement(const NaClBitcodeBlockDistElement&) = delete;
  void operator=(const NaClBitcodeBlockDistElement&);

public:
  static bool classof(const NaClBitcodeDistElement *Element) {
    return Element->getKind() >= RDE_BlockDist &&
        Element->getKind() < RDE_BlockDistLast;
  }

  // Top-level constructor to create instances of this class.
  explicit NaClBitcodeBlockDistElement(
      NaClBitcodeDistElementKind Kind=RDE_BlockDist)
      : NaClBitcodeBitsDistElement(Kind) {}

  virtual ~NaClBitcodeBlockDistElement();

  virtual NaClBitcodeDistElement *CreateElement(
      NaClBitcodeDistValue Value) const;

  // Sorts by %file, rather than number of instances.
  virtual double GetImportance(NaClBitcodeDistValue value) const;

  virtual const char *GetTitle() const;

  virtual const char *GetValueHeader() const;

  /// Prints out header for row of statistics associated with instances
  /// of this distribution element.
  virtual void PrintStatsHeader(raw_ostream &Stream) const;

  /// Prints out statistics for the row with the given value.
  virtual void PrintRowStats(raw_ostream &Stream,
                             const NaClBitcodeDist *Distribution) const;

  virtual void PrintRowValue(raw_ostream &Stream,
                             NaClBitcodeDistValue Value,
                             const NaClBitcodeDist *Distribution) const;
};

class NaClBitcodeBlockDist : public NaClBitcodeDist {
  NaClBitcodeBlockDist(const NaClBitcodeBlockDist&) = delete;
  void operator=(const NaClBitcodeBlockDist&) = delete;

public:
  static bool classof(const NaClBitcodeDist *Dist) {
    return Dist->getKind() >= RD_BlockDist &&
        Dist->getKind() < RD_BlockDistLast;
  }

  static NaClBitcodeBlockDistElement DefaultSentinal;

  explicit NaClBitcodeBlockDist(
      const NaClBitcodeBlockDistElement *Sentinal=&DefaultSentinal)
      : NaClBitcodeDist(BlockStorage, Sentinal, RD_BlockDist)
  {}

  virtual ~NaClBitcodeBlockDist();

  // Returns the total number of bits in all blocks in the distribution.
  uint64_t GetTotalBits() const;

  // Returns the printable name associated with the given BlockID.
  //
  // Note: If the name is not known, an "UnknownBlock" name is
  // generated and returned.
  static std::string GetName(unsigned BlockID);

};

}

#endif
