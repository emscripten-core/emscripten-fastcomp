//===-- NaClCompressCodeDist.h -------------------------------------------===//
//      Defines distribution maps for record codes for pnacl-bccompress.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_BITCODE_NACL_NACLCOMPRESSCODEDIST_H
#define LLVM_BITCODE_NACL_NACLCOMPRESSCODEDIST_H

#include "llvm/Bitcode/NaCl/NaClBitcodeCodeDist.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeSizeDist.h"

namespace llvm {

/// Defines a record code distribution, with nested distributions separating
/// the code distributions based on size and values.
class NaClCompressCodeDistElement : public NaClBitcodeCodeDistElement {
public:
  static bool classof(const NaClBitcodeDistElement *Element) {
    return Element->getKind() >= RDE_CompressCodeDist &&
        Element->getKind() < RDE_CompressCodeDistLast;
  }

  NaClCompressCodeDistElement()
      : NaClBitcodeCodeDistElement(RDE_CompressCodeDist),
        SizeDist(NaClBitcodeDist::RecordStorage,
                 &NaClBitcodeSizeDistElement::Sentinel) {
    NestedDists.push_back(&SizeDist);
  }

  virtual ~NaClCompressCodeDistElement();

  virtual NaClBitcodeDistElement *CreateElement(
      NaClBitcodeDistValue Value) const;

  virtual void AddRecord(const NaClBitcodeRecord &Record);

  virtual const SmallVectorImpl<NaClBitcodeDist*> *
  GetNestedDistributions() const;

  NaClBitcodeDist &GetSizeDist() {
    return SizeDist;
  }

  /// The sentinel used to generate instances of this in
  /// a record code distribution map.
  static NaClCompressCodeDistElement Sentinel;

private:
  // Nested blocks used by GetNestedDistributions.
  SmallVector<NaClBitcodeDist*, 1> NestedDists;

  /// The distribution of values, based on size.
  NaClBitcodeDist SizeDist;
};

}

#endif
