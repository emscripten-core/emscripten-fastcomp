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

namespace llvm {

class MemoryBuffer;
class StringRef;
class raw_ostream;

// Analysis options. See the command-line documentation in pnacl-bcanalyzer
// for a description.
struct AnalysisDumpOptions {
  AnalysisDumpOptions()
    : DoDump(false), DumpOnlyRecords(false), OpsPerLine(0),
      NoHistogram(false), NonSymbolic(false)
  {}

  bool DoDump;
  bool DumpOnlyRecords;
  unsigned OpsPerLine;
  bool NoHistogram;
  bool NonSymbolic;
};

/// Run analysis on the given file. Output goes to OS.
/// Note: these analysis runs are not currently thread-safe.
int AnalyzeBitcodeInFile(const StringRef &InputFilename, raw_ostream &OS,
                         const AnalysisDumpOptions &DumpOptions);

/// Run analysis on a memory buffer with bitcode. The buffer is owned by the
/// caller.
int AnalyzeBitcodeInBuffer(const MemoryBuffer &Buf, raw_ostream &OS,
                           const AnalysisDumpOptions &DumpOptions);

} // namespace llvm

#endif
