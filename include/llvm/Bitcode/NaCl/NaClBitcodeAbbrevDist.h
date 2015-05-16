//===-- NaClBitcodeAbbrevDist.h ---------------------------------------------===//
//      Defines distribution maps for abbreviations associated with
//      bitcode records.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Defines distribution maps for tracking abbreviations associated
// with bitcode records.

#ifndef LLVM_BITCODE_NACL_NACLBITCODEABBREVDIST_H
#define LLVM_BITCODE_NACL_NACLBITCODEABBREVDIST_H

#include "llvm/Bitcode/NaCl/NaClBitcodeDist.h"
#include "llvm/Bitcode/NaCl/NaClCompressCodeDist.h"

namespace llvm {

/// Collects the number of instances associated with a given abbreviation
/// index of a bitcode record. Note: Uses naclbitc::UNABBREV_RECORD index
/// to denote bitcode records that did not use an abbreviation.
class NaClBitcodeAbbrevDistElement : public NaClBitcodeDistElement {
  NaClBitcodeAbbrevDistElement(const NaClBitcodeAbbrevDistElement&) = delete;
  void operator=(const NaClBitcodeAbbrevDistElement&) = delete;

public:
  static bool classof(const NaClBitcodeDistElement *Element) {
    return Element->getKind() >= RDE_AbbrevDist &&
        Element->getKind() < RDE_AbbrevDistLast;
  }

  explicit NaClBitcodeAbbrevDistElement(unsigned BlockID=0)
      : NaClBitcodeDistElement(RDE_AbbrevDist),
        CodeDist(BlockID, &NaClCompressCodeDistElement::Sentinel) {
    NestedDists.push_back(&CodeDist);
  }

  // Sentinel to create instances of this.
  static NaClBitcodeAbbrevDistElement Sentinel;

  virtual ~NaClBitcodeAbbrevDistElement();

  virtual NaClBitcodeDistElement *CreateElement(
      NaClBitcodeDistValue Value) const;

  virtual void GetValueList(const NaClBitcodeRecord &Record,
                            ValueListType &ValueList) const;

  virtual void AddRecord(const NaClBitcodeRecord &Record);

  virtual const char *GetTitle() const;

  virtual const char *GetValueHeader() const;

  virtual void PrintRowValue(raw_ostream &Stream,
                             NaClBitcodeDistValue Value,
                             const NaClBitcodeDist *Distribution) const;

  virtual const SmallVectorImpl<NaClBitcodeDist*> *
  GetNestedDistributions() const;

  NaClBitcodeDist &GetCodeDist() {
    return CodeDist;
  }

private:
  // Nested blocks used by GetNestedDistributions.
  SmallVector<NaClBitcodeDist*, 1> NestedDists;

  /// The records associated with the given abbreviation.
  NaClBitcodeCodeDist CodeDist;
};

/// Separates record codes based on abbreviations. This is done so
/// that when we add abbreviations, they will be refinements of
/// existing abbreviations. This should guarantee that we don't lose
/// separation defined by previous iterations on calls to
/// pnacl-bccompress.
class NaClBitcodeAbbrevDist : public NaClBitcodeDist {
public:
  static bool classof(const NaClBitcodeDist *Dist) {
    return Dist->getKind() >= RD_AbbrevDist &&
        Dist->getKind() < RD_AbbrevDistLast;
  }

  explicit NaClBitcodeAbbrevDist(
      unsigned BlockID,
      NaClBitcodeAbbrevDistElement *Sentinel
      = &NaClBitcodeAbbrevDistElement::Sentinel,
      NaClBitcodeDistKind Kind=RD_AbbrevDist)
      : NaClBitcodeDist(RecordStorage, Sentinel, Kind),
        BlockID(BlockID) {
  }

  virtual ~NaClBitcodeAbbrevDist();

  // Returns the block id associated with the abbreviations in this
  // distribution map.
  unsigned GetBlockID() const {
    return BlockID;
  }

  virtual NaClBitcodeDistElement*
  CreateElement(NaClBitcodeDistValue Value) const;

private:
  // The block id associated with the abbreviations in this
  // distribution map.
  unsigned BlockID;
};

}

#endif
