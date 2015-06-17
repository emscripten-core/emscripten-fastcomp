//===-- pnacl-bcfuzz.cpp - Record fuzzer for PNaCl bitcode ----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Generates (record-level) fuzzed PNaCl bitcode files from an input
// PNaCl bitcode file.
//
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/NaCl/NaClFuzz.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/ToolOutputFile.h"

using namespace llvm;
using namespace naclfuzz;

static cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<frozen file>"), cl::init("-"));

static cl::opt<std::string>
OutputPrefix("output", cl::desc("<output prefix>"), cl::init(""));

static cl::opt<unsigned>
FuzzCount("count", cl::desc("Number of fuzz results to generate"),
          cl::init(1));

static cl::opt<bool>
ConvertToTextRecords(
    "convert-to-text",
    cl::desc("Convert input to record text file (specified by -output)"),
    cl::init(false));

static cl::opt<std::string>
RandomSeed("random-seed",
     cl::desc("Use this value for seed of random number generator "
              "(rather than input)"),
     cl::init(""));

static cl::opt<bool>
ShowFuzzRecordDistribution(
    "record-distribution",
    cl::desc("Show distribution of record edits while fuzzing"),
    cl::init(false));

static cl::opt<bool>
ShowFuzzEditDistribution(
    "edit-distribution",
    cl::desc("Show distribution of editing actions while fuzzing"),
    cl::init(false));

static cl::opt<unsigned>
PercentageToEdit(
    "edit-percentage",
    cl::desc("Percentage of records to edit during fuzz (between 1 and"
             " '-percentage-base')"),
    cl::init(1));

static cl::opt<unsigned>
PercentageBase(
    "percentage-base",
    cl::desc("Base that '-edit-precentage' is defined on (defaults to 100)"),
    cl::init(100));

static cl::opt<bool>
Verbose("verbose",
        cl::desc("Show details of fuzzing/writing of bitcode files"),
        cl::init(false));

static void WriteOutputFile(SmallVectorImpl<char> &Buffer,
                            StringRef OutputFilename) {
  std::error_code EC;
  std::unique_ptr<tool_output_file> Out(
      new tool_output_file(OutputFilename, EC, sys::fs::F_None));
  if (EC) {
    errs() << EC.message() << '\n';
    exit(1);
  }

  for (SmallVectorImpl<char>::const_iterator
           Iter = Buffer.begin(), IterEnd = Buffer.end();
       Iter != IterEnd; ++Iter) {
    Out->os() << *Iter;
  }

  // Declare success.
  Out->keep();
}

static bool WriteBitcode(NaClMungedBitcode &Bitcode,
                         NaClMungedBitcode::WriteFlags &WriteFlags,
                         StringRef OutputFile) {
  if (Verbose) {
    errs() << "Records:\n";
    for (const auto &Record : Bitcode) {
      errs() << "  " << Record << "\n";
    }
  }

  SmallVector<char, 100> Buffer;
  if (!Bitcode.write(Buffer, true, WriteFlags)) {
    errs() << "Error: Failed to write bitcode: " << OutputFile << "\n";
    return false;
  }
  WriteOutputFile(Buffer, OutputFile);
  return true;
}

static void WriteFuzzedBitcodeFiles(NaClMungedBitcode &Bitcode,
                                    NaClMungedBitcode::WriteFlags &WriteFlags) {
  std::string RandSeed(RandomSeed);
  if (RandomSeed.empty())
    RandSeed = InputFilename;

  DefaultRandomNumberGenerator Generator(RandSeed);
  std::unique_ptr<RecordFuzzer> Fuzzer(
      RecordFuzzer::createSimpleRecordFuzzer(Bitcode, Generator));

  for (size_t i = 1; i <= FuzzCount; ++i) {
    Generator.saltSeed(i);
    std::string OutputFile;
    {
      raw_string_ostream StrBuf(OutputFile);
      StrBuf << OutputPrefix << "-" << i;
      StrBuf.flush();
    }

    if (Verbose)
      errs() << "Generating " << OutputFile << "\n";
    if (!Fuzzer->fuzz(PercentageToEdit, PercentageBase)) {
      errs() << "Error: Fuzzing failed: " << OutputFile << "\n";
      continue;
    }
    WriteBitcode(Bitcode, WriteFlags, OutputFile);
  }

  if (ShowFuzzRecordDistribution)
    Fuzzer->showRecordDistribution(outs());
  if (ShowFuzzEditDistribution)
    Fuzzer->showEditDistribution(outs());
}

bool writeTextualBitcodeRecords(std::unique_ptr<MemoryBuffer> InputBuffer) {
  NaClBitcodeRecordList Records;
  readNaClBitcodeRecordList(Records, std::move(InputBuffer));
  SmallVector<char, 1024> OutputBuffer;
  if (!writeNaClBitcodeRecordList(Records, OutputBuffer, errs()))
    return false;
  WriteOutputFile(OutputBuffer, OutputPrefix);
  return true;
}

int main(int argc, char **argv) {
  // Print a stack trace if we signal out.
  sys::PrintStackTraceOnErrorSignal();
  PrettyStackTraceProgram X(argc, argv);

  cl::ParseCommandLineOptions(argc, argv, "Fuzz a PNaCl bitcode file\n");

  if (OutputPrefix.empty()) {
    errs() << "Output prefix not specified!\n";
    return 1;
  }

  ErrorOr<std::unique_ptr<MemoryBuffer>>
      MemBuf(MemoryBuffer::getFileOrSTDIN(InputFilename));
  if (!MemBuf) {
    errs() << MemBuf.getError().message() << "\n";
    return 1;
  }

  if (ConvertToTextRecords)
    return !writeTextualBitcodeRecords(std::move(MemBuf.get()));

  if (PercentageToEdit > PercentageBase) {
    errs() << "Edit percentage " << PercentageToEdit
           << " must not exceed: " << PercentageBase << "\n";
    return 1;
  }

  raw_null_ostream NullStrm;
  NaClMungedBitcode::WriteFlags WriteFlags;
  WriteFlags.setTryToRecover(true);
  if (!Verbose) {
    WriteFlags.setErrStream(NullStrm);
  }

  NaClMungedBitcode Bitcode(std::move(MemBuf.get()));
  WriteFuzzedBitcodeFiles(Bitcode, WriteFlags);
  return 0;
}
