//===-- NaClAnalyzerBlockDist.h --------------------------------------------===//
//      Defines distribution maps used to collect block and record
//      distributions for tools pnacl-bcanalyzer.
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
  NaClAnalyzerBlockDistElement(const NaClAnalyzerBlockDistElement&) = delete;
  void operator=(const NaClAnalyzerBlockDistElement&) = delete;

public:
  static bool classof(const NaClBitcodeDistElement *Element) {
    return Element->getKind() >= RDE_NaClAnalBlockDist &&
        Element->getKind() < RDE_NaClAnalBlockDistLast;
  }

  explicit NaClAnalyzerBlockDistElement(unsigned BlockID=0,
                                        bool OrderBlocksByID=false)
      : NaClBitcodeBlockDistElement(RDE_NaClAnalBlockDist),
        BlockID(BlockID),
        RecordDist(BlockID),
        OrderBlocksByID(OrderBlocksByID) {
    NestedDists.push_back(&SubblockDist);
    NestedDists.push_back(&RecordDist);
  }

  virtual ~NaClAnalyzerBlockDistElement();

  virtual NaClBitcodeDistElement*
  CreateElement(NaClBitcodeDistValue Value) const;

  virtual double GetImportance(NaClBitcodeDistValue Value) const;

  virtual const SmallVectorImpl<NaClBitcodeDist*> *
  GetNestedDistributions() const;

  NaClBitcodeDist &GetSubblockDist() {
    return SubblockDist;
  }

  NaClBitcodeCodeDist &GetRecordDist() {
    return RecordDist;
  }

private:
  // The block ID of the distribution.
  unsigned BlockID;

  // Subblocks that appear in this block.
  NaClBitcodeSubblockDist SubblockDist;

  // Records that appear in this block.
  NaClBitcodeCodeDist RecordDist;

  // Nested blocks used by GetNestedDistributions.
  SmallVector<NaClBitcodeDist*, 2> NestedDists;

  // If true, order (top-level) blocks by block ID instead of file
  // size.
  bool OrderBlocksByID;
};

/// Holds block distribution, and nested subblock and record code distributions,
/// to be collected during analysis.
class NaClAnalyzerBlockDist : public NaClBitcodeBlockDist {
  NaClAnalyzerBlockDist(const NaClAnalyzerBlockDist&) = delete;
  void operator=(const NaClAnalyzerBlockDist&) = delete;
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
