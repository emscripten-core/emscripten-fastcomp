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
#include "llvm/Bitcode/NaCl/NaClBitstreamWriter.h"
#include "llvm/Bitcode/NaCl/NaClCompress.h"
#include "llvm/Bitcode/NaCl/NaClReaderWriter.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/MemoryBuffer.h"

#include <memory>

using namespace llvm;

// For debugging. When true, shows each PNaCl record that is
// emitted to the bitcode file.
static bool DebugEmitRecord = false;

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
  NaClBitstreamWriter OutStream(MungedInputBuffer);
  Writer = &OutStream;

  if (DebugEmitRecord) {
    errs() << "*** Run test: " << TestName << "\n";
  }

  WriteBlockID = -1;
  SetBID = -1;

  MungedBitcode.munge(Munges, MungesSize, RecordTerminator);
  writeMungedBitcode(MungedBitcode, AddHeader);

  // Add null terminator, so that we meet the requirements of the
  // MemoryBuffer API.
  MungedInputBuffer.push_back('\0');

  MungedInput = MemoryBuffer::getMemBuffer(
      StringRef(MungedInputBuffer.data(), MungedInputBuffer.size()-1),
      TestName);
}

void NaClBitcodeMunger::cleanupTest() {
  MungedBitcode.removeEdits();
  MungedInput.reset();
  assert(DumpStream && "Dump stream removed before cleanup!");
  DumpStream->flush();
  delete DumpStream;
  DumpStream = nullptr;
  Writer = nullptr;
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

void NaClBitcodeMunger::writeMungedBitcode(const NaClMungedBitcode &Bitcode,
                                           bool AddHeader) {
  if (AddHeader) {
    NaClWriteHeader(*Writer, true);
  }
  for (const NaClBitcodeAbbrevRecord &Record : Bitcode) {
    emitRecord(Record);
  }
}

void NaClBitcodeMunger::emitRecord(const NaClBitcodeAbbrevRecord &Record) {
  if (DebugEmitRecord) {
    errs() << "Emit " << Record.Abbrev << ": <" << Record.Code;
    for (size_t i = 0, e = Record.Values.size(); i < e; ++i) {
      errs() << ", " << Record.Values[i];
    }
    errs() << ">\n";
  }

  switch (Record.Code) {
  case naclbitc::BLK_CODE_ENTER: {
    unsigned NumBits = naclbitc::DEFAULT_MAX_ABBREV;
    WriteBlockID = -1;
    if (Record.Abbrev != naclbitc::ENTER_SUBBLOCK) {
      Fatal() << "Enter block record code " << Record.Code
              << " uses illegal abbreviation index " << Record.Abbrev << "\n";
      ReportFatalError();
    }
    if (Record.Values.size() == 2) {
      WriteBlockID = Record.Values[0];
      NumBits = Record.Values[1];
      if (NumBits > 32) {
        Fatal() << "Error: Bit size " << NumBits
                << " for record should be <= 32\n";
        ReportFatalError();
      }
      if (NumBits < 2) {
        Fatal() << "Error: Bit size " << NumBits
                << " for record should be >= 2\n";
        ReportFatalError();
      }
    } else {
      Fatal() << "Error: Values for enter record should be of size 2. Found: "
              << Record.Values.size();
      ReportFatalError();
    }
    uint64_t MaxAbbrev = (static_cast<uint64_t>(1) << NumBits) - 1;
    AbbrevIndexLimitStack.push_back(MaxAbbrev);
    if (WriteBlockID == naclbitc::BLOCKINFO_BLOCK_ID) {
      if (NumBits != naclbitc::DEFAULT_MAX_ABBREV) {
        Fatal()
            << "Error: Numbits entry for abbreviations record not 2. Found: "
            << NumBits << "\n";
        ReportFatalError();
      }
      Writer->EnterBlockInfoBlock();
    } else {
      NaClBitcodeSelectorAbbrev CurCodeLen(MaxAbbrev);
      Writer->EnterSubblock(WriteBlockID, CurCodeLen);
    }
    return;
  }
  case naclbitc::BLK_CODE_EXIT:
    if (Record.Abbrev != naclbitc::END_BLOCK) {
      Fatal() << "Error: Exit block record code " << Record.Code
              << " uses illegal abbreviation index " << Record.Abbrev << "\n";
      ReportFatalError();
    }
    if (!Record.Values.empty()) {
      Fatal() << "Error: Exit block should not have values. Found: "
              << Record.Values.size() << "\n";
      ReportFatalError();
    }
    if (!AbbrevIndexLimitStack.empty())
      AbbrevIndexLimitStack.pop_back();
    Writer->ExitBlock();
    return;
  case naclbitc::BLK_CODE_DEFINE_ABBREV: {
    if (Record.Abbrev != naclbitc::DEFINE_ABBREV) {
      Fatal() << "Error: Define abbreviation record code " << Record.Code
               << " uses illegal abbreviation index " << Record.Abbrev << "\n";
      ReportFatalError();
    }
    NaClBitCodeAbbrev *Abbrev = buildAbbrev(Record);
    if (Abbrev == NULL) return;
    if (WriteBlockID == naclbitc::BLOCKINFO_BLOCK_ID) {
      Writer->EmitBlockInfoAbbrev(SetBID, Abbrev);
    } else {
      Writer->EmitAbbrev(Abbrev);
    }
    return;
  }
  case naclbitc::BLK_CODE_HEADER:
    // Note: There is no abbreviation index here. Ignore.
    for (uint64_t Value : Record.Values)
      Writer->Emit(Value, 8);
    return;
  default:
    if ((Record.Abbrev != naclbitc::UNABBREV_RECORD
         && !Writer->isUserRecordAbbreviation(Record.Abbrev))) {
      uint64_t BlockAbbrevIndexLimit = 0;
      if (!AbbrevIndexLimitStack.empty())
        BlockAbbrevIndexLimit = AbbrevIndexLimitStack.back();
      if (Record.Abbrev > BlockAbbrevIndexLimit) {
        Fatal() << "Error: Record code " << Record.Code
                << " uses illegal abbreviation index " << Record.Abbrev
                << ". Must not exceed " << BlockAbbrevIndexLimit << "\n";
        ReportFatalError();
      }
      // Note: If this point is reached, the abbreviation is
      // bad. However, that may be the point of munge being
      // applied. Hence, emit the bad abbreviation and the data so
      // that the reader can be tested on this bad input.  For
      // simplicity, we output the record data using the default
      // abbreviation pattern.
      errs() << "Warning: Record code " << Record.Code
             << " uses illegal abbreviation index " << Record.Abbrev << "\n";
      Writer->EmitCode(Record.Abbrev);
      Writer->EmitVBR(Record.Code, 6);
      uint32_t NumValues = static_cast<uint32_t>(Record.Values.size());
      Writer->EmitVBR(NumValues, 6);
      for (uint32_t i = 0; i < NumValues; ++i) {
        Writer->EmitVBR64(Record.Values[i], 6);
      }
      return;
    }
    if (Record.Abbrev == naclbitc::UNABBREV_RECORD)
      Writer->EmitRecord(Record.Code, Record.Values);
    else
      Writer->EmitRecord(Record.Code, Record.Values, Record.Abbrev);
    return;
  }
  Fatal() << "emitRecord on unimplemented code" << "\n";
  ReportFatalError();
}

NaClBitCodeAbbrev *NaClBitcodeMunger::buildAbbrev(
    const NaClBitcodeAbbrevRecord &Record) {
  NaClBitCodeAbbrev *Abbrev = new NaClBitCodeAbbrev();
  size_t Index = 0;
  if (Record.Values.empty()) {
    Fatal() << "Empty abbreviation record not allowed\n";
    ReportFatalError();
  }
  size_t NumAbbreviations = Record.Values[Index++];
  size_t NumValues = Record.Values.size();
  if (NumAbbreviations == 0) {
    Fatal() << "Abbreviation must contain at least one operator\n";
    ReportFatalError();
  }
  for (size_t Count = 0; Count < NumAbbreviations; ++Count) {
    if (Index >= NumValues) {
      Fatal() << "Malformed abbreviation found. Expects "
              << NumAbbreviations << " operands. Found: "
              << Count << "\n";
      ReportFatalError();
    }
    switch (Record.Values[Index++]) {
    case 1:
      if (Index >= NumValues) {
        Fatal() << "Malformed literal abbreviation.\n";
        ReportFatalError();
      }
      Abbrev->Add(NaClBitCodeAbbrevOp(Record.Values[Index++]));
      break;
    case 0: {
      if (Index >= NumValues) {
        Fatal() << "Malformed abbreviation found.\n";
        ReportFatalError();
      }
      unsigned Kind = Record.Values[Index++];
      switch (Kind) {
      case NaClBitCodeAbbrevOp::Fixed:
        if (Index >= NumValues) {
          Fatal() << "Malformed fixed abbreviation found.\n";
          ReportFatalError();
        }
        Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed,
                                        Record.Values[Index++]));
        break;
      case NaClBitCodeAbbrevOp::VBR:
        if (Index >= NumValues) {
          Fatal() << "Malformed vbr abbreviation found.\n";
          ReportFatalError();
        }
        Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR,
                                        Record.Values[Index++]));
        break;
      case NaClBitCodeAbbrevOp::Array:
        if (Index >= NumValues) {
          Fatal() << "Malformed array abbreviation found.\n";
          ReportFatalError();
        }
        Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Array));
        break;
      case NaClBitCodeAbbrevOp::Char6:
        Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Char6));
        break;
      default:
        Fatal() << "Unknown abbreviation kind. Found: " << Kind << "\n";
        ReportFatalError();
      }
      break;
    }
    default:
      Fatal() << "Error: Bad literal flag in abbreviation. Found: "
              << Record.Values[Index];
      ReportFatalError();
    }
  }
  return Abbrev;
}

bool NaClObjDumpMunger::runTestWithFlags(
    const char *Name, const uint64_t Munges[], size_t MungesSize,
    bool AddHeader, bool NoRecords, bool NoAssembly) {
  setupTest(Name, Munges, MungesSize, AddHeader);

  /// If running in death mode, redirect output directly to the
  /// error stream (rather than buffering in DumpStream), so that
  /// output can be seen in gtest death test.
  raw_ostream &Output = RunAsDeathTest ? errs() : *DumpStream;
  if (NaClObjDump(MungedInput.get()->getMemBufferRef(),
                  Output, NoRecords, NoAssembly))
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
  raw_ostream *VerboseStrm = VerboseErrors ? DumpStream : nullptr;
  ErrorOr<Module *> ModuleOrError =
      NaClParseBitcodeFile(MungedInput->getMemBufferRef(), Context,
                           VerboseStrm);
  if (ModuleOrError) {
    if (VerboseErrors)
      *DumpStream << "Successful parse!\n";
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
  bool Result = Compressor.compress(MungedInput.get(), *DumpStream);
  cleanupTest();
  return Result;
}
