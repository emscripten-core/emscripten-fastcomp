//===-- NaClAnalyzerBlockDist.h --------------------------------------------===//
//      Defines distribution maps used to collect block and record
//      distributions for tools pnacl-bcanalyzer and pnacl-benchmark.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Defines a block distribution, with nested subblock and record code
// distributions, to be collected during analysis.

#ifndef LLVM_BITCODE_NACL_NACLANALYZERBLOCKDIST_H
#define LLVM_BITCODE_NACL_NACLANALYZERBLOCKDIST_H

#include "llvm/Bitcode/NaCl/NaClBitcodeBlockDist.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeCodeDist.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeSubblockDist.h"

namespace llvm {

/// Holds block distribution, and nested subblock and record code distributions,
/// to be collected during analysis.
class NaClAnalyzerBlockDistElement : public NaClBitcodeBlockDistElement {

public:
  static bool classof(const NaClBitcodeDistElement *Element) {
    return Element->getKind() >= RDE_PNaClAnalBlockDist &&
        Element->getKind() < RDE_PNaClAnalBlockDist_Last;
  }

  /// Creates the sentinel distribution map element for
  /// class NaClAnalyzerBlockDist.
  explicit NaClAnalyzerBlockDistElement(bool OrderBlocksByID=false)
      : NaClBitcodeBlockDistElement(RDE_PNaClAnalBlockDist),
        RecordDist(0),
        BlockID(0),
        OrderBlocksByID(OrderBlocksByID) {
    Init();
  }

  virtual ~NaClAnalyzerBlockDistElement();

  virtual NaClBitcodeDistElement*
  CreateElement(NaClBitcodeDistValue Value) const;

  virtual double GetImportance() const;

  virtual const SmallVectorImpl<NaClBitcodeDist*> *
  GetNestedDistributions() const;

  // Subblocks that appear in this block.
  NaClBitcodeSubblockDist SubblockDist;

  // Records that appear in this block.
  NaClBitcodeCodeDist RecordDist;

protected:
  // Creates instance to put in distribution map. Called by
  // method CreateInstance.
  NaClAnalyzerBlockDistElement(unsigned BlockID,
                               bool OrderBlocksByID)
      : NaClBitcodeBlockDistElement(RDE_PNaClAnalBlockDist),
        RecordDist(BlockID),
        BlockID(BlockID),
        OrderBlocksByID(OrderBlocksByID) {
    Init();
  }

private:
  // The block ID of the distribution.
  unsigned BlockID;

  // Nested blocks used by GetNestedDistributions.
  SmallVector<NaClBitcodeDist*, 2> NestedDists;

  // If true, order (top-level) blocks by block ID instead of file
  // size.
  bool OrderBlocksByID;

  void Init() {
    NestedDists.push_back(&SubblockDist);
    NestedDists.push_back(&RecordDist);
  }
};

/// Holds block distribution, and nested subblock and record code distributions,
/// to be collected during analysis.
class NaClAnalyzerBlockDist : public NaClBitcodeBlockDist {
public:
  NaClAnalyzerBlockDist(NaClAnalyzerBlockDistElement &Sentinel)
      : NaClBitcodeBlockDist(&Sentinel)
  {}

  virtual ~NaClAnalyzerBlockDist() {}

  virtual void AddRecord(const NaClBitcodeRecord &Record);

  virtual void AddBlock(const NaClBitcodeBlock &Block);
};

}
#endif
