//===-- NaClBitcodeSubblockDist.h -----------------------------------------===//
//      Defines distribution maps for subblock values within an
//      (externally specified) block.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Defines the notion subblock distribution map. Shows what subblocks appear
// within a given block (defined externally to the distribution map).

#ifndef LLVM_BITCODE_NACL_NACLBITCODESUBBLOCKDIST_H
#define LLVM_BITCODE_NACL_NACLBITCODESUBBLOCKDIST_H

#include "llvm/Bitcode/NaCl/NaClBitcodeDist.h"

namespace llvm {

/// Collects the distribution of subblocks within an (externally defined)
/// block.
class NaClBitcodeSubblockDistElement : public NaClBitcodeDistElement {
  NaClBitcodeSubblockDistElement(const NaClBitcodeSubblockDistElement &)
  = delete;
  void operator=(const NaClBitcodeSubblockDistElement&);

public:
  static bool classof(const NaClBitcodeDistElement *Element) {
    return Element->getKind() >= RDE_SubblockDist &&
        Element->getKind() < RDE_SubblockDistLast;
  }

  NaClBitcodeSubblockDistElement()
      : NaClBitcodeDistElement(RDE_SubblockDist) {}

  virtual ~NaClBitcodeSubblockDistElement();

  virtual NaClBitcodeDistElement *CreateElement(
      NaClBitcodeDistValue Value) const;

  virtual const char *GetTitle() const;

  virtual const char *GetValueHeader() const;

  virtual void PrintRowValue(raw_ostream &Stream,
                             NaClBitcodeDistValue Value,
                             const NaClBitcodeDist *Distribution) const;
};

/// Collects the distribution of subblocks within an (externally
/// defined) block. Assumes distribution elements are instances of
/// NaClBitcodeSubblockDistElement.
class NaClBitcodeSubblockDist : public NaClBitcodeDist {
  NaClBitcodeSubblockDist(const NaClBitcodeSubblockDist&) = delete;
  void operator=(const NaClBitcodeSubblockDist&) = delete;

public:
  static bool classof(const NaClBitcodeDist *Dist) {
    return Dist->getKind() >= RD_SubblockDist &&
        Dist->getKind() < RD_SubblockDistLast;
  }

  static NaClBitcodeSubblockDistElement DefaultSentinal;

  NaClBitcodeSubblockDist()
      : NaClBitcodeDist(BlockStorage, &DefaultSentinal, RD_SubblockDist)
  {}

  virtual ~NaClBitcodeSubblockDist();
};

}

#endif
