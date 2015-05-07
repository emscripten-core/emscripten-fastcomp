//===--- Bitcode/NaCl/TestUtils/NaClBitcodeMunge.cpp - Bitcode Munger -----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Bitcode writer/munger implementation for testing.
//
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/NaCl/NaClBitcodeMunge.h"

#include "llvm/Bitcode/NaCl/NaClBitcodeHeader.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeParser.h"
#include "llvm/Bitcode/NaCl/NaClCompress.h"
#include "llvm/Bitcode/NaCl/NaClReaderWriter.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MemoryBuffer.h"

#include <memory>

using namespace llvm;

// For debugging. When true, shows each test being run.
static bool TraceTestRuns = false;

void NaClBitcodeMunger::setupTest(
    const char *TestName, const uint64_t Munges[], size_t MungesSize,
    bool AddHeader) {
  assert(DumpStream == nullptr && "Test run with DumpStream already defined");
  assert(MungedInput.get() == nullptr
         && "Test run with MungedInput already defined");
  FoundErrors = false;
  DumpResults.clear(); // Throw away any previous results.
  std::string DumpBuffer;
  DumpStream = new raw_string_ostream(DumpResults);
  MungedInputBuffer.clear();

  if (TraceTestRuns) {
    errs() << "*** Run test: " << TestName << "\n";
  }

  MungedBitcode.munge(Munges, MungesSize, RecordTerminator);
  WriteFlags.setErrStream(getDumpStream());
  NaClMungedBitcode::WriteResults Results =
      MungedBitcode.writeMaybeRepair(MungedInputBuffer, AddHeader, WriteFlags);
  if (Results.NumErrors != 0
      && !(WriteFlags.getTryToRecover()
           && Results.NumRepairs == Results.NumErrors)
      && !(WriteFlags.getWriteBadAbbrevIndex()
           && Results.WroteBadAbbrevIndex && Results.NumErrors == 1))
    report_fatal_error("Unable to generate bitcode file due to write errors");

  // Add null terminator, so that we meet the requirements of the
  // MemoryBuffer API.
  MungedInputBuffer.push_back('\0');

  MungedInput = MemoryBuffer::getMemBuffer(
      StringRef(MungedInputBuffer.data(), MungedInputBuffer.size()-1),
      TestName);
}

void NaClBitcodeMunger::cleanupTest() {
  RunAsDeathTest = false;
  WriteFlags.reset();
  MungedBitcode.removeEdits();
  MungedInput.reset();
  assert(DumpStream && "Dump stream removed before cleanup!");
  DumpStream->flush();
  delete DumpStream;
  DumpStream = nullptr;
}

// Return the next line of input (including eoln), starting from
// Pos. Then increment Pos past the end of that line.
static std::string getLine(const std::string &Input, size_t &Pos) {
  std::string Line;
  if (Pos >= Input.size()) {
    Pos = std::string::npos;
    return Line;
  }
  size_t Eoln = Input.find_first_of("\n", Pos);
  if (Eoln != std::string::npos) {
    for (size_t i = Pos; i <= Eoln; ++i)
      Line.push_back(Input[i]);
    Pos = Eoln + 1;
    return Line;
  }
  Pos = std::string::npos;
  return Input.substr(Pos);
}

std::string NaClBitcodeMunger::
getLinesWithTextMatch(const std::string &Substring, bool MustBePrefix) const {
  std::string Messages;
  size_t LastLinePos = 0;
  while (1) {
    std::string Line = getLine(DumpResults, LastLinePos);
    if (LastLinePos == std::string::npos) break;
    size_t Pos = Line.find(Substring);
    if (Pos != std::string::npos && (!MustBePrefix || Pos == 0)) {
      Messages.append(Line);
    }
  }
  return Messages;
}

bool NaClObjDumpMunger::runTestWithFlags(
    const char *Name, const uint64_t Munges[], size_t MungesSize,
    bool AddHeader, bool NoRecords, bool NoAssembly) {
  setupTest(Name, Munges, MungesSize, AddHeader);

  if (NaClObjDump(MungedInput.get()->getMemBufferRef(),
                  getDumpStream(), NoRecords, NoAssembly))
    FoundErrors = true;
  cleanupTest();
  return !FoundErrors;
}

bool NaClParseBitcodeMunger::runTest(
    const char *Name, const uint64_t Munges[], size_t MungesSize,
    bool VerboseErrors) {
  bool AddHeader = true;
  setupTest(Name, Munges, MungesSize, AddHeader);
  LLVMContext &Context = getGlobalContext();
  raw_ostream *VerboseStrm = VerboseErrors ? &getDumpStream() : nullptr;
  ErrorOr<Module *> ModuleOrError =
      NaClParseBitcodeFile(MungedInput->getMemBufferRef(), Context,
                           VerboseStrm);
  if (ModuleOrError) {
    if (VerboseErrors)
      getDumpStream() << "Successful parse!\n";
    delete ModuleOrError.get();
  } else {
    Error() << ModuleOrError.getError().message() << "\n";
  }
  cleanupTest();
  return !FoundErrors;
}

bool NaClCompressMunger::runTest(const char* Name, const uint64_t Munges[],
                                 size_t MungesSize) {
  bool AddHeader = true;
  setupTest(Name, Munges, MungesSize, AddHeader);
  NaClBitcodeCompressor Compressor;
  bool Result = Compressor.compress(MungedInput.get(), getDumpStream());
  cleanupTest();
  return Result;
}
