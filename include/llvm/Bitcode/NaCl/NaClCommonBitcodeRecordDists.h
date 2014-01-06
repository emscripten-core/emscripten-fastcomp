//===-- NaClCommonBitcodeRecordDists.cpp - Bitcode Analyzer ---------------===//
//      Defines distribution maps for various values in bitcode records.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Defines simple (non-nested) distribution maps for (common) values
// appearing in bitcode records (instances of class NaClBitcodeRecord
// in NaClBitcodeParser.h). This includes records for tracking:
//
// 1) BlockID's appearing in the bitcode file.
//
// 2) Record Code's appearing in blocks with a given BlockID.
//
// 3) Record abbreviations used for records in blocks with a given
// BlockID.
//
// 4) Value indicies defined in records, in blocks with a given
// BlockID.
//
// 5) Values in records, in blocks with a given BlockID.
//
// TODO(kschimpf) Define records 1, 3, 4, and 5.

#ifndef LLVM_BITCODE_NACL_NACLCOMMONBITCODERECORDDISTS_H
#define LLVM_BITCODE_NACL_NACLCOMMONBITCODERECORDDISTS_H

#include "llvm/Bitcode/NaCl/NaClBitcodeRecordBitsDist.h"

namespace llvm {

// Collects the distribution of record codes/number of bits used for a
// particular blockID and Code ID.
class NaClBitcodeCodeDistElement
    : public NaClBitcodeBitsDistElement {
  NaClBitcodeCodeDistElement(const NaClBitcodeCodeDistElement&)
      LLVM_DELETED_FUNCTION;
  void operator=(const NaClBitcodeCodeDistElement&)
      LLVM_DELETED_FUNCTION;

public:
  static bool classof(const NaClBitcodeDistElement *Element) {
    return Element->getKind() >= RDE_CodeDist
        && Element->getKind() < RDE_CodeDist_Last;
  }

  NaClBitcodeCodeDistElement(
      NaClBitcodeDistElementKind Kind=RDE_CodeDist)
      : NaClBitcodeBitsDistElement(Kind)
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
  NaClBitcodeCodeDist(const NaClBitcodeCodeDist&)
      LLVM_DELETED_FUNCTION;
  void operator=(const NaClBitcodeCodeDist&)
      LLVM_DELETED_FUNCTION;

public:
  static bool classof(const NaClBitcodeDist *Dist) {
    return Dist->getKind() >= RD_CodeDist
        && Dist->getKind() < RD_CodeDist_Last;
  }

protected:
  NaClBitcodeCodeDist(NaClBitcodeDistElement *Sentinal,
                      unsigned BlockID,
                      NaClBitcodeDistKind Kind=RD_CodeDist)
      : NaClBitcodeDist(RecordStorage, Sentinal, Kind), BlockID(BlockID)
  {}

  static NaClBitcodeCodeDistElement DefaultSentinal;

public:
  NaClBitcodeCodeDist(unsigned BlockID)
      : NaClBitcodeDist(RecordStorage, &DefaultSentinal, RD_CodeDist),
        BlockID(BlockID)
  {}

  virtual ~NaClBitcodeCodeDist();

  unsigned GetBlockID() const {
    return BlockID;
  }

  // Returns true if there is a known printable name for record code
  // CodeID in block associated with BlockID.
  static bool HasKnownCodeName(unsigned CodeID, unsigned BlockID);

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
