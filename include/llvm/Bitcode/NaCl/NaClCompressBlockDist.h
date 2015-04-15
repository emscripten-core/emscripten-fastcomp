//===-- NaClCompressBlockDist.h -------------------------------------------===//
//      Defines distribution maps used to collect block and record
//      distributions for tool pnacl-bccompress.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_BITCODE_NACL_NACLCOMPRESSBLOCKDIST_H
#define LLVM_BITCODE_NACL_NACLCOMPRESSBLOCKDIST_H


#include "llvm/Bitcode/NaCl/NaClBitcodeAbbrevDist.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeBlockDist.h"

namespace llvm {

/// Nests record distributions within the block they appear in,
/// in a block distribution. The record distributions are refined
/// by separating record codes that use the same abbreviation.
class NaClCompressBlockDistElement : public NaClBitcodeBlockDistElement {
public:
  static bool classof(const NaClBitcodeDistElement *Element) {
    return Element->getKind() >= RDE_PNaClCompressBlockDist
        && Element->getKind() < RDE_PNaClCompressBlockDistLast;
  }

  explicit NaClCompressBlockDistElement(unsigned BlockID=0)
      : NaClBitcodeBlockDistElement(RDE_PNaClCompressBlockDist),
        AbbrevDist(BlockID) {
    NestedDists.push_back(&AbbrevDist);
  }

  virtual ~NaClCompressBlockDistElement();

  virtual NaClBitcodeDistElement*
  CreateElement(NaClBitcodeDistValue Value) const;

  virtual const SmallVectorImpl<NaClBitcodeDist*> *
  GetNestedDistributions() const;

  NaClBitcodeDist &GetAbbrevDist() {
    return AbbrevDist;
  }

  // Sentinel for generating elements of this type.
  static NaClCompressBlockDistElement Sentinel;

private:
  // Nested blocks used by GetNestedDistributions.
  SmallVector<NaClBitcodeDist*, 1> NestedDists;

  // The abbreviations/records associated with the corresponding block.
  NaClBitcodeAbbrevDist AbbrevDist;
};

}

#endif
