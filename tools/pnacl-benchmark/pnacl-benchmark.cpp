//===-- pnacl-benchmark.cpp -----------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// pnacl-benchmark: various benchmarking tools for the PNaCl LLVM toolchain.
//
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/NaCl/NaClBitcodeAnalyzer.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeHeader.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeParser.h"
#include "llvm/Bitcode/NaCl/NaClBitstreamReader.h"
#include "llvm/Bitcode/NaCl/NaClLLVMBitCodes.h"
#include "llvm/Bitcode/NaCl/NaClReaderWriter.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/Timer.h"
#include <memory>
#include <system_error>
#include <vector>

using namespace llvm;


static cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<input bitcode>"), cl::init("-"));

static cl::opt<unsigned>
NumRuns("num-runs", cl::desc("Number of runs"), cl::init(1));

/// Used in a lexical block to measure and report the block's execution time.
///
/// \param N block name
/// \param InputSize optional size of input operated upon. If given, the
///                  throughput will be reported as well in MB/sec.
class TimingOperationBlock {
public:
  TimingOperationBlock(StringRef N, size_t InputSize=0)
    : InputSize(InputSize) {
    outs() << "Timing: " << N << "... ";
    TStart = TimeRecord::getCurrentTime(true);
  }

  ~TimingOperationBlock() {
    TimeRecord TEnd = TimeRecord::getCurrentTime(false);
    double elapsed = TEnd.getWallTime() - TStart.getWallTime();
    outs() << format("%.3lf", elapsed) << " sec";

    if (InputSize != 0) {
      double MBPerSec = (InputSize / elapsed) / 1000000.0;
      outs() << format(" [%.3lf MB/sec]", MBPerSec);
    }
    outs() << "\n";
  }
private:
  TimeRecord TStart;
  size_t InputSize;
};

/// Simple parsing of bitcode with some basic bookkeeping that simulates doing
/// "something" with it.
class DummyBitcodeParser : public NaClBitcodeParser {
public:
  DummyBitcodeParser(NaClBitstreamCursor &Cursor)
    : NaClBitcodeParser(Cursor) {
    resetCounters();
  }

  DummyBitcodeParser(unsigned BlockID, DummyBitcodeParser *EnclosingBlock)
    : NaClBitcodeParser(BlockID, EnclosingBlock) {
    resetCounters();
  }

  virtual bool ParseBlock(unsigned BlockID) {
    DummyBitcodeParser Parser(BlockID, this);
    return Parser.ParseThisBlock();
  }

  virtual void EnterBlock(unsigned NumberWords) {
    NumBlocks++;
    BlockNames.push_back("<unknown>");
  }

  virtual void ProcessRecord() {
    NumRecords++;
    RecordCodes.push_back(Record.GetCode());

    const NaClBitcodeRecord::RecordVector &Values = Record.GetValues();
    for (unsigned i = 0, e = Values.size(); i != e; ++i) {
      RecordValues.push_back((int64_t) Values[i]);
    }
  }

private:
  void resetCounters() {
    NumBlocks = NumRecords = 0;
    BlockNames.clear();
    RecordCodes.clear();
    RecordValues.clear();
  }

  uint64_t NumBlocks, NumRecords;
  std::vector<StringRef> BlockNames;
  std::vector<unsigned> RecordCodes;
  std::vector<StringRef> RecordCodeNames;
  std::vector<int64_t> RecordValues;
};

void BenchmarkIRParsing() {
  outs() << "Benchmarking IR parsing...\n";
  ErrorOr<std::unique_ptr<MemoryBuffer>> ErrOrFile =
      MemoryBuffer::getFileOrSTDIN(InputFilename);
  if (std::error_code EC = ErrOrFile.getError())
    report_fatal_error("Could not open input file: " + EC.message());

  std::unique_ptr<MemoryBuffer> FileBuf(ErrOrFile.get().release());
  size_t BufSize = FileBuf->getBufferSize();
  const uint8_t *BufPtr =
      reinterpret_cast<const uint8_t *>(FileBuf->getBufferStart());
  const uint8_t *EndBufPtr =
      reinterpret_cast<const uint8_t *>(FileBuf->getBufferEnd());

  // Since MemoryBuffer may use mmap, make sure to first touch all bytes in the
  // input buffer to make sure it's actually in memory.
  volatile uint8_t *Slot = new uint8_t;
  for (const uint8_t *S = BufPtr; S != EndBufPtr; ++S) {
    *Slot = *S;
  }

  delete Slot;
  outs() << "Read bitcode into buffer. Size=" << BufSize << "\n";

  // Trivial copy into a new buffer with a cascading XOR that simulates
  // "touching" every byte in the buffer in a simple way.
  {
    TimingOperationBlock T("Simple XOR copy", BufSize);
    volatile uint8_t *OutBuf = new uint8_t[BufSize];
    OutBuf[0] = 1;
    size_t N = 1;
    // Run over the input buffer from start to end-1; run over the output buffer
    // from 1 to end.
    for (const uint8_t *S = BufPtr; S != EndBufPtr - 1; ++S, ++N) {
      OutBuf[N] = OutBuf[N - 1] ^ *S;
    }
    delete[] OutBuf;
  }

  // Simulate simple bitcode parsing. See DummyBitcodeParser for more details.
  {
    TimingOperationBlock T("Bitcode block parsing", BufSize);
    NaClBitcodeHeader Header;

    if (Header.Read(BufPtr, EndBufPtr)) {
      report_fatal_error("Invalid PNaCl bitcode header");
    }

    if (!Header.IsSupported()) {
      errs() << "Warning: " << Header.Unsupported() << "\n";
    }

    if (!Header.IsReadable()) {
      report_fatal_error("Bitcode file is not readable");
    }

    NaClBitstreamReader StreamFile(BufPtr, EndBufPtr);
    NaClBitstreamCursor Stream(StreamFile);
    DummyBitcodeParser Parser(Stream);
    while (!Stream.AtEndOfStream()) {
      if (Parser.Parse()) {
        report_fatal_error("Parsing failed");
      }
    }
  }

  // Running bitcode analysis (what bcanalyzer does).
  // Note that quite a bit of time here is spent on emitting I/O into nulls().
  {
    TimingOperationBlock T("Running bitcode analysis", BufSize);

    AnalysisDumpOptions DumpOptions;
    AnalyzeBitcodeInBuffer(FileBuf, nulls(), DumpOptions);
  }

  // Actual LLVM IR parsing and formation from the bitcode
  {
    TimingOperationBlock T("LLVM IR parsing", BufSize);
    SMDiagnostic Err;
    Module *M = NaClParseIRFile(InputFilename, PNaClFormat,
                                Err, getGlobalContext());

    if (!M) {
      report_fatal_error("Unable to NaClParseIRFile");
    }
  }
}

int main(int argc, char **argv) {
  sys::PrintStackTraceOnErrorSignal();
  PrettyStackTraceProgram X(argc, argv);

  llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.
  cl::ParseCommandLineOptions(argc, argv, "pnacl-benchmark\n");

  for (unsigned i = 0; i < NumRuns; i++) {
    BenchmarkIRParsing();
  }

  return 0;
}
