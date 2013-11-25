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
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/raw_ostream.h"
using namespace llvm;


static cl::opt<std::string>
  InputFilename(cl::Positional, cl::desc("<input bitcode>"), cl::init("-"));

static cl::opt<bool> OptDump("dump", cl::desc("Dump low level bitcode trace"));

static cl::opt<bool>
 OptDumpRecords("dump-records",
         cl::desc("Dump contents of records in bitcode, leaving out"
                  " all bitstreaming information (including abbreviations)"),
         cl::init(false));

static cl::opt<unsigned> OpsPerLine(
    "operands-per-line",
    cl::desc("Number of operands to print per dump line. 0 implies "
             "all operands will be printed on the same line (default)"),
    cl::init(0));

static cl::opt<bool> NoHistogram("disable-histogram",
                                 cl::desc("Do not print per-code histogram"));

static cl::opt<bool>
NonSymbolic("non-symbolic",
            cl::desc("Emit numeric info in dump even if"
                     " symbolic info is available"));

int main(int argc, char **argv) {
  // Print a stack trace if we signal out.
  sys::PrintStackTraceOnErrorSignal();
  PrettyStackTraceProgram X(argc, argv);
  llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.
  cl::ParseCommandLineOptions(argc, argv, "pnacl-bcanalyzer file analyzer\n");

  AnalysisDumpOptions DumpOptions;
  DumpOptions.DoDump = OptDumpRecords || OptDump;
  DumpOptions.DumpOnlyRecords = OptDumpRecords;
  DumpOptions.OpsPerLine = OpsPerLine;
  DumpOptions.NoHistogram = NoHistogram;
  DumpOptions.NonSymbolic = NonSymbolic;

  return AnalyzeBitcodeInFile(InputFilename, outs(), DumpOptions);
}
