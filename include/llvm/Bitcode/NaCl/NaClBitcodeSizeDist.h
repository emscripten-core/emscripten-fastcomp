//===-- NaClBitcodeSizeDist.h ---------------------------------------------===//
//      Defines distribution maps for bitcode record sizes (arity).
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Defines distribution maps for tracking bitcode record sizes
// (arity).

#ifndef LLVM_BITCODE_NACL_NACLBITCODESIZEDIST_H
#define LLVM_BITCODE_NACL_NACLBITCODESIZEDIST_H

#include "llvm/Bitcode/NaCl/NaClBitcodeDist.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeValueDist.h"

namespace llvm {

/// Collects the number of bitcode record instances with the same number
/// of elements in the vector of values, with nested value distribution maps.
class NaClBitcodeSizeDistElement : public NaClBitcodeDistElement {
  NaClBitcodeSizeDistElement(const NaClBitcodeSizeDistElement&) = delete;
  void operator=(const NaClBitcodeSizeDistElement&) = delete;

public:
  static bool classof(const NaClBitcodeDistElement *Element) {
    return Element->getKind() >= RDE_SizeDist &&
        Element->getKind() < RDE_SizeDistLast;
  }

  NaClBitcodeSizeDistElement()
      : NaClBitcodeDistElement(RDE_SizeDist),
        ValueIndexDist(NaClBitcodeDist::RecordStorage,
                       &NaClBitcodeValueIndexDistElement::Sentinel) {
    NestedDists.push_back(&ValueIndexDist);
  }

  static NaClBitcodeSizeDistElement Sentinel;

  virtual ~NaClBitcodeSizeDistElement();

  virtual NaClBitcodeDistElement *CreateElement(
      NaClBitcodeDistValue Value) const;

  virtual void GetValueList(const NaClBitcodeRecord &Record,
                            ValueListType &ValueList) const;

  virtual void AddRecord(const NaClBitcodeRecord &Record);

  virtual const SmallVectorImpl<NaClBitcodeDist*> *
  GetNestedDistributions() const;

  virtual const char *GetTitle() const;

  virtual const char *GetValueHeader() const;

  virtual void PrintRowValue(raw_ostream &Stream,
                             NaClBitcodeDistValue Value,
                             const NaClBitcodeDist *Distribution) const;

  NaClBitcodeDist &GetValueIndexDist() {
    return ValueIndexDist;
  }

private:
  // Nested blocks used by GetNestedDistributions.
  SmallVector<NaClBitcodeDist*, 1> NestedDists;

  // The value distributions associated with records of the given size.
  NaClBitcodeDist ValueIndexDist;
};

}

#endif
