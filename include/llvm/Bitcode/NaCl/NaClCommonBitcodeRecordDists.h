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
// particular blockID. Assumes distribution elements are instances of
// NaClBitcodeRecordBitsDistElement.
class NaClBitcodeRecordCodeDist : public NaClBitcodeRecordBitsDist {
  NaClBitcodeRecordCodeDist(const NaClBitcodeRecordCodeDist&)
      LLVM_DELETED_FUNCTION;
  void operator=(const NaClBitcodeRecordCodeDist&)
      LLVM_DELETED_FUNCTION;

public:

  bool classof(const NaClBitcodeRecordDist *Dist) {
    return Dist->getKind() >= RD_RecordCodeDist
        && Dist->getKind() < RD_RecordCodeDist_Last;
  }

  NaClBitcodeRecordCodeDist(unsigned BlockID,
                            NaClBitcodeRecordDistKind Kind=RD_RecordCodeDist)
      : NaClBitcodeRecordBitsDist(Kind), BlockID(BlockID)
  {}

  virtual ~NaClBitcodeRecordCodeDist();

protected:
  virtual void GetValueList(const NaClBitcodeRecord &Record,
                            ValueListType &ValueList) const;

  virtual const char *GetTitle() const;

  virtual const char *GetValueHeader() const;

  virtual void PrintRowValue(raw_ostream &Stream,
                             const std::string &Indent,
                             NaClBitcodeRecordDistValue Value) const;

public:
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
