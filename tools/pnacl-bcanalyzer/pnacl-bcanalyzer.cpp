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
  DumpOptions.OrderBlocksByID = OrderBlocksByID;

  return AnalyzeBitcodeInFile(InputFilename, outs(), DumpOptions);
}
