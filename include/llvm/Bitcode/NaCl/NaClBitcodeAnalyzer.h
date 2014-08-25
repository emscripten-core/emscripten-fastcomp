//===-- NaClBitcodeAnalyzer.h - Bitcode Analyzer --------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
// Analytical information about a bitcode file. Intended as an aid to developers
// of bitcode reading and writing software. It produces a summary of the bitcode
// file that shows various statistics about the contents of the file. By default
// this information is detailed and contains information about individual
// bitcode blocks and the functions in the module. The tool is also able to
// print a bitcode file in a straight forward text format that shows the
// containment and relationships of the information in the bitcode file (-dump
// option).
//
//===----------------------------------------------------------------------===//

#ifndef NACL_BITCODE_ANALYZER_H
#define NACL_BITCODE_ANALYZER_H

#include <memory>

namespace llvm {

class MemoryBuffer;
class StringRef;
class raw_ostream;

// Analysis options. See the command-line documentation in pnacl-bcanalyzer
// for a description.
struct AnalysisDumpOptions {
  AnalysisDumpOptions()
      : DumpRecords(false), DumpDetails(false), OpsPerLine(0)
  {}

  // When true, dump the records. When false, print out distribution
  // statistics.
  bool DumpRecords;

  // When true, print out abbreviations, abbreviation ID's, and
  // other (non-record specific) details when dumping records.
  bool DumpDetails;

  // The number of record operands to be dumped per text line.
  unsigned OpsPerLine;

  // When true, prints block statistics based on block ID rather than
  // size. When false, prints block statistics base on percentage of
  // file.
  bool OrderBlocksByID;
};

/// Run analysis on the given file. Output goes to OS.
int AnalyzeBitcodeInFile(const StringRef &InputFilename, raw_ostream &OS,
                         const AnalysisDumpOptions &DumpOptions);

/// Run analysis on a memory buffer with bitcode.  Output goes to
/// OS. The buffer is owned by the caller.
int AnalyzeBitcodeInBuffer(const std::unique_ptr<MemoryBuffer> &Buf,
                           raw_ostream &OS,
                           const AnalysisDumpOptions &DumpOptions);

} // namespace llvm

#endif
