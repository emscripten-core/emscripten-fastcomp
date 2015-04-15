//===-- NaClBitcodeCodeDist.h ---------------------------------------------===//
//      Defines distribution maps for various values in bitcode records.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Defines simple (non-nested) distribution maps for record codes
// appearing in bitcode records (instances of class NaClBitcodeRecord
// in NaClBitcodeParser.h).

#ifndef LLVM_BITCODE_NACL_NACLBITCODECODEDIST_H
#define LLVM_BITCODE_NACL_NACLBITCODECODEDIST_H

#include "llvm/Bitcode/NaCl/NaClBitcodeBitsAndAbbrevsDist.h"

namespace llvm {

// Collects the distribution of record codes/number of bits used for a
// particular blockID and Code ID.
class NaClBitcodeCodeDistElement
    : public NaClBitcodeBitsAndAbbrevsDistElement {
  NaClBitcodeCodeDistElement(const NaClBitcodeCodeDistElement&)
      = delete;
  void operator=(const NaClBitcodeCodeDistElement&)
      = delete;

public:
  static bool classof(const NaClBitcodeDistElement *Element) {
    return Element->getKind() >= RDE_CodeDist
        && Element->getKind() < RDE_CodeDistLast;
  }

  explicit NaClBitcodeCodeDistElement(
      NaClBitcodeDistElementKind Kind=RDE_CodeDist)
      : NaClBitcodeBitsAndAbbrevsDistElement(Kind)
  {}

  virtual ~NaClBitcodeCodeDistElement();

  virtual NaClBitcodeDistElement *CreateElement(
      NaClBitcodeDistValue Value) const;

  virtual void GetValueList(const NaClBitcodeRecord &Record,
                            ValueListType &ValueList) const;

  virtual const char *GetTitle() const;

  virtual const char *GetValueHeader() const;

  virtual void PrintRowValue(raw_ostream &Stream,
                             NaClBitcodeDistValue Value,
                             const NaClBitcodeDist *Distribution) const;
};

// Collects the distribution of record codes/number of bits used for a
// particular blockID. Assumes distribution elements are instances of
// NaClBitcodeCodeDistElement.
class NaClBitcodeCodeDist : public NaClBitcodeDist {
  NaClBitcodeCodeDist(const NaClBitcodeCodeDist&) = delete;
  void operator=(const NaClBitcodeCodeDist&) = delete;

public:
  static bool classof(const NaClBitcodeDist *Dist) {
    return Dist->getKind() >= RD_CodeDist
        && Dist->getKind() < RD_CodeDistLast;
  }

protected:
  NaClBitcodeCodeDist(NaClBitcodeDistElement *Sentinal,
                      unsigned BlockID,
                      NaClBitcodeDistKind Kind=RD_CodeDist)
      : NaClBitcodeDist(RecordStorage, Sentinal, Kind), BlockID(BlockID)
  {}

  static NaClBitcodeCodeDistElement DefaultSentinel;

public:
  explicit NaClBitcodeCodeDist(
      unsigned BlockID,
      NaClBitcodeCodeDistElement *Sentinel = &DefaultSentinel,
      NaClBitcodeDistKind Kind=RD_CodeDist)
      : NaClBitcodeDist(RecordStorage, Sentinel, Kind),
        BlockID(BlockID)
  {}

  virtual ~NaClBitcodeCodeDist();

  unsigned GetBlockID() const {
    return BlockID;
  }

  // Returns the printable name for record code CodeID in blocks
  // associated with BlockID.
  //
  // Note: If the name is not known, an "UnknownCode" name is
  // generated and return.
  static std::string GetCodeName(unsigned CodeID, unsigned BlockID);

private:
  // The blockID associated with the record code distribution.
  // Used so that we can look up the print name for each record code.
  unsigned BlockID;
};

}

#endif
