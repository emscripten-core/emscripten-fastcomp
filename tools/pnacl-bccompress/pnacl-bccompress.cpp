//===-- pnacl-bccompress.cpp - Bitcode (abbrev) compression ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This tool may be invoked in the following manner:
//  pnacl-bccompress [options] bcin.pexe -o bcout.pexe
//      - Read frozen PNaCl bitcode from the bcin.pexe and introduce
//        abbreviations to compress it into bcout.pexe.
//
//  Options:
//      --help      - Output information about command line switches
//
// This tool analyzes the data in bcin.pexe, and determines what
// abbreviations can be added to compress the bitcode file. The result
// is written to bcout.pexe.
//
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/NaCl/NaClCompress.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/ToolOutputFile.h"

namespace {

using namespace llvm;

static cl::opt<bool>
TraceGeneratedAbbreviations(
    "abbreviations",
    cl::desc("Trace abbreviations added to compressed file"),
    cl::init(false));

static cl::opt<std::string>
  InputFilename(cl::Positional, cl::desc("<input bitcode>"), cl::init("-"));

static cl::opt<std::string>
OutputFilename("o", cl::desc("Specify output filename"),
               cl::value_desc("filename"), cl::init("-"));

static cl::opt<bool>
ShowValueDistributions(
    "show-distributions",
    cl::desc("Show collected value distributions in bitcode records. "
             "Turns off compression."),
    cl::init(false));

static cl::opt<bool>
ShowAbbrevLookupTries(
    "show-lookup-tries",
    cl::desc("Show lookup tries used to minimize search for \n"
             "matching abbreviations. Turns off compression."),
    cl::init(false));

static cl::opt<bool>
ShowAbbreviationFrequencies(
    "show-abbreviation-frequencies",
    cl::desc("Show how often each abbreviation is used. "
             "Turns off compression."),
    cl::init(false));

// Note: When this flag is true, we still generate new abbreviations,
// because we don't want to add the complexity of turning it off.
// Rather, we simply make sure abbreviations are ignored when writing
// out the final copy.
static cl::opt<bool>
RemoveAbbreviations(
    "remove-abbreviations",
    cl::desc("Remove abbreviations from input bitcode file."),
    cl::init(false));

static bool Fatal(const std::string &Err) {
  errs() << Err << "\n";
  exit(1);
}

// Reads the input file into the given buffer.
static void ReadAndBuffer(std::unique_ptr<MemoryBuffer> &MemBuf) {
  ErrorOr<std::unique_ptr<MemoryBuffer>> ErrOrFile =
      MemoryBuffer::getFileOrSTDIN(InputFilename);
  if (std::error_code EC = ErrOrFile.getError())
    Fatal("Error reading '" + InputFilename + "': " + EC.message());

  MemBuf.reset(ErrOrFile.get().release());
  if (MemBuf->getBufferSize() % 4 != 0)
    Fatal("Bitcode stream should be a multiple of 4 bytes in length");
}

}  // namespace

int main(int argc, char **argv) {
  // Print a stack trace if we signal out.
  sys::PrintStackTraceOnErrorSignal();
  PrettyStackTraceProgram X(argc, argv);
  llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.
  cl::ParseCommandLineOptions(argc, argv, "pnacl-bccompress file analyzer\n");

  std::unique_ptr<MemoryBuffer> MemBuf;
  ReadAndBuffer(MemBuf);

  std::error_code EC;
  std::unique_ptr<tool_output_file> OutFile(
      new tool_output_file(OutputFilename.c_str(), EC, sys::fs::F_None));
  if (EC)
    Fatal(EC.message());

  NaClBitcodeCompressor Compressor;
  Compressor.Flags.TraceGeneratedAbbreviations = TraceGeneratedAbbreviations;
  Compressor.Flags.ShowValueDistributions = ShowValueDistributions;
  Compressor.Flags.ShowAbbrevLookupTries = ShowAbbrevLookupTries;
  Compressor.Flags.ShowAbbreviationFrequencies = ShowAbbreviationFrequencies;
  Compressor.Flags.RemoveAbbreviations = RemoveAbbreviations;

  if (ShowValueDistributions
      || ShowAbbreviationFrequencies
      || ShowAbbrevLookupTries) {
    // Assume we are only interested in analysis.
    int ReturnStatus = !Compressor.analyze(MemBuf.get(), OutFile->os());
    OutFile->keep();
    return ReturnStatus;
  }

  if (!Compressor.compress(MemBuf.get(), OutFile->os()))
    return 1;
  OutFile->keep();
  return 0;
}
