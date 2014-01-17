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

/// Defines the distribution element of values for a given index in
/// the values of a bitcode record.
class NaClBitcodeValueDistElement : public NaClBitcodeDistElement {
  NaClBitcodeValueDistElement(const NaClBitcodeValueDistElement&)
  LLVM_DELETED_FUNCTION;
  void operator=(const NaClBitcodeValueDistElement&) LLVM_DELETED_FUNCTION;

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

  virtual const char *GetTitle() const;

  virtual const char *GetValueHeader() const;

  virtual void PrintRowValue(raw_ostream &Stream,
                             NaClBitcodeDistValue Value,
                             const NaClBitcodeDist *Distribution) const;
};

/// Defines the distribution of values for a given index in
/// the values of a bitcode record.
class NaClBitcodeValueDist : public NaClBitcodeDist {
  NaClBitcodeValueDist(const NaClBitcodeValueDist&) LLVM_DELETED_FUNCTION;
  void operator=(const NaClBitcodeValueDist&) LLVM_DELETED_FUNCTION;

public:
  static bool classof(const NaClBitcodeDist *Dist) {
    return Dist->getKind() >= RD_ValueDist &&
        Dist->getKind() < RD_ValueDistLast;
  }

  explicit NaClBitcodeValueDist(unsigned Index)
      : NaClBitcodeDist(RecordStorage,
                        &NaClBitcodeValueDistElement::Sentinel,
                        RD_ValueDist),
        Index(Index) {
  }

  virtual ~NaClBitcodeValueDist();

  unsigned GetIndex() const {
    return Index;
  }

  virtual void GetValueList(const NaClBitcodeRecord &Record,
                            ValueListType &ValueList) const;

private:
  unsigned Index;
};

/// Defines the value distribution for each value index in corresponding
/// bitcode records. This is a helper class used to separate each element
/// in the bitcode record, so that we can determine the proper abbreviation
/// for each element.
class NaClBitcodeValueIndexDistElement : public NaClBitcodeDistElement {
  NaClBitcodeValueIndexDistElement(
      const NaClBitcodeValueIndexDistElement&) LLVM_DELETED_FUNCTION;
  void operator=(
      const NaClBitcodeValueIndexDistElement&) LLVM_DELETED_FUNCTION;

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

  virtual double GetImportance() const;

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
