//===-- pnacl-bcanalyzer.cpp - Bitcode Analyzer -------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This tool is a thin wrapper over NaClBitcodeAnalyzer; see
// NaClBitcodeAnalyzer.h for more details.
// 
// Invoke in the following manner:
//
//  pnacl-bcanalyzer [options]      - Read frozen PNaCl bitcode from stdin
//  pnacl-bcanalyzer [options] x.bc - Read frozen PNaCl bitcode from the x.bc
//                                    file
// Run with -help to see supported options.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "pnacl-bcanalyzer"

#include "llvm/Bitcode/NaCl/NaClBitcodeAnalyzer.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeBlockDist.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeCodeDist.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeSubblockDist.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;


static cl::opt<std::string>
  InputFilename(cl::Positional, cl::desc("<input bitcode>"), cl::init("-"));

static cl::opt<bool>
 OptDumpRecords(
     "dump-records",
     cl::desc("Dump contents of records in bitcode, leaving out details, "
              "instead of displaying record distributions."),
     cl::init(false));

static cl::opt<bool>
OptDumpDetails(
    "dump-details",
    cl::desc("Include details when dumping contents of records in bitcode."),
    cl::init(false));

static cl::opt<unsigned> OpsPerLine(
    "operands-per-line",
    cl::desc("Number of operands to print per dump line. 0 implies "
             "all operands will be printed on the same line (default)"),
    cl::init(0));

static cl::opt<bool> OrderBlocksByID(
    "order-blocks-by-id",
    cl::desc("Print blocks statistics based on block id rather than size"),
    cl::init(false));

/// Holds block distribution, and nested subblock and record code distributions,
/// to be collected during analysis.
class PNaClAnalyzerBlockDistElement : public NaClBitcodeBlockDistElement {

public:
  static bool classof(const NaClBitcodeDistElement *Element) {
    return Element->getKind() >= RDE_PNaClAnalBlockDist &&
        Element->getKind() < RDE_PNaClAnalBlockDist_Last;
  }

  /// Creates the default sentinel distribution map element.
  PNaClAnalyzerBlockDistElement()
      : NaClBitcodeBlockDistElement(RDE_PNaClAnalBlockDist),
        RecordDist(0),
        BlockID(0)
  {
    Init();
  }

  virtual ~PNaClAnalyzerBlockDistElement() {}

  virtual NaClBitcodeDistElement*
  CreateElement(NaClBitcodeDistValue Value) const {
    return new PNaClAnalyzerBlockDistElement(Value);
  }

  virtual double GetImportance() const {
    if (OrderBlocksByID)
      // Negate importance to "undo" reverse ordering of sort.
      return -static_cast<double>(BlockID);
    else
      return NaClBitcodeBlockDistElement::GetImportance();
  }

  virtual const SmallVectorImpl<NaClBitcodeDist*> *
  GetNestedDistributions() const {
    return &NestedDists;
  }

  // Sentinel to generate instances.
  static PNaClAnalyzerBlockDistElement Sentinel;

  // Subblocks that appear in this block.
  NaClBitcodeSubblockDist SubblockDist;

  // Records that appear in this block.
  NaClBitcodeCodeDist RecordDist;

protected:
  // Creates instance to put in distribution map. Called by
  // method CreateInstance.
  explicit PNaClAnalyzerBlockDistElement(unsigned BlockID)
      : NaClBitcodeBlockDistElement(RDE_PNaClAnalBlockDist),
        RecordDist(BlockID),
        BlockID(BlockID) {
    Init();
  }

private:
  // The block ID of the distribution.
  unsigned BlockID;

  // Nested blocks used by GetNestedDistributions.
  SmallVector<NaClBitcodeDist*, 2> NestedDists;

  void Init() {
    NestedDists.push_back(&SubblockDist);
    NestedDists.push_back(&RecordDist);
  }
};

PNaClAnalyzerBlockDistElement PNaClAnalyzerBlockDistElement::Sentinel;

/// Holds block distribution, and nested subblock and record code distributions,
/// to be collected during analysis.
class PNaClAnalyzerBlockDist : public NaClBitcodeBlockDist {
public:
  PNaClAnalyzerBlockDist()
      : NaClBitcodeBlockDist(&PNaClAnalyzerBlockDistElement::Sentinel)
  {}

  virtual ~PNaClAnalyzerBlockDist() {}

  virtual void AddRecord(const NaClBitcodeRecord &Record) {
    cast<PNaClAnalyzerBlockDistElement>(GetElement(Record.GetBlockID()))
        ->RecordDist.AddRecord(Record);
  }

  virtual void AddBlock(const NaClBitcodeBlock &Block) {
    NaClBitcodeBlockDist::AddBlock(Block);
    if (const NaClBitcodeBlock *EncBlock = Block.GetEnclosingBlock()) {
      cast<PNaClAnalyzerBlockDistElement>(GetElement(EncBlock->GetBlockID()))
          ->SubblockDist.AddBlock(Block);
    }
  }
};

int main(int argc, char **argv) {
  // Print a stack trace if we signal out.
  sys::PrintStackTraceOnErrorSignal();
  PrettyStackTraceProgram X(argc, argv);
  llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.
  cl::ParseCommandLineOptions(argc, argv, "pnacl-bcanalyzer file analyzer\n");

  if (OptDumpDetails && !OptDumpRecords) {
    errs() << "Can't dump details unless records are dumped!\n";
    return 1;
  }

  AnalysisDumpOptions DumpOptions;
  DumpOptions.DumpRecords = OptDumpRecords;
  DumpOptions.DumpDetails = OptDumpDetails;
  DumpOptions.OpsPerLine = OpsPerLine;

  PNaClAnalyzerBlockDist BlockDist;
  return AnalyzeBitcodeInFile(InputFilename, outs(), DumpOptions, &BlockDist);
}
