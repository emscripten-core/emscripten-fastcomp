//===- NaClBitcodeMungeWriter.cpp - Write munged bitcode --------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Implements method NaClMungedBitcode.write(), which writes out a munged
// list of bitcode records using a bitstream writer.

#include "llvm/Bitcode/NaCl/NaClBitcodeMungeUtils.h"

#include "llvm/Bitcode/NaCl/NaClBitstreamWriter.h"
#include "llvm/Bitcode/NaCl/NaClReaderWriter.h"

using namespace llvm;

namespace {

// For debugging. When true, shows each PNaCl record that is
// emitted to the bitcode file.
static bool DebugEmitRecord = false;

// State of bitcode writing.
struct WriteState {
  // The block ID associated with the block being written.
  int WriteBlockID = -1;
  // The SetBID for the blockinfo block.
  int SetBID = -1;
  // The stack of maximum abbreviation indices allowed by block enter record.
  SmallVector<uint64_t, 3> AbbrevIndexLimitStack;
  // The set of write flags to use.
  const NaClMungedBitcode::WriteFlags &Flags;
  // The results of the attempted write.
  NaClMungedBitcode::WriteResults Results;

  WriteState(const NaClMungedBitcode::WriteFlags &Flags) : Flags(Flags) {}

  // Returns stream to print error message to.
  raw_ostream &Error() {
    ++Results.NumErrors;
    return Flags.getErrStream() << "Error (Block " << WriteBlockID << "): ";
  }

  // Returns stream to print error message to, assuming that
  // the error message can be repaired if Flags.TryToRecover is true.
  raw_ostream &RecoverableError() {
    if (Flags.getTryToRecover())
      ++Results.NumRepairs;
    return Error();
  }

  // Converts the abbreviation record to the corresponding abbreviation.
  // Returns nullptr if unable to build abbreviation.
  NaClBitCodeAbbrev *buildAbbrev(const NaClBitcodeAbbrevRecord &Record);

  // Emits the given record to the bitcode file. Returns true if
  // successful.
  bool emitRecord(NaClBitstreamWriter &Writer,
                  const NaClBitcodeAbbrevRecord &Record);

  // Adds any missing end blocks to written bitcode.
  void writeMissingEndBlocks(NaClBitstreamWriter &Writer) {
    while (!AbbrevIndexLimitStack.empty()) {
      Writer.ExitBlock();
      AbbrevIndexLimitStack.pop_back();
    }
  }
};

bool WriteState::emitRecord(NaClBitstreamWriter &Writer,
                            const NaClBitcodeAbbrevRecord &Record) {
  size_t NumValues = Record.Values.size();
  if (DebugEmitRecord) {
    errs() << "Emit " << Record.Abbrev << ": <" << Record.Code;
    for (size_t i = 0; i < NumValues; ++i) {
      errs() << ", " << Record.Values[i];
    }
    errs() << ">\n";
  }

  switch (Record.Code) {
  case naclbitc::BLK_CODE_ENTER: {
    unsigned NumBits = NaClBitsNeededForValue(naclbitc::DEFAULT_MAX_ABBREV);
    WriteBlockID = -1;
    if (Record.Abbrev != naclbitc::ENTER_SUBBLOCK) {
      RecoverableError()
          << "Uses illegal abbreviation index in enter block record: "
          << Record << "\n";
      if (!Flags.getTryToRecover())
        return false;
    }
    if (NumValues == 2) {
      WriteBlockID = Record.Values[0];
      NumBits = Record.Values[1];
      if (NumBits > 32 || NumBits < 2) {
        RecoverableError()
            << "Bit size " << NumBits << " for record should be "
            << (NumBits > 32 ? "<= 32" : ">= 2") << ": " << Record << "\n";
        if (!Flags.getTryToRecover())
          return false;
        NumBits = 32;
      }
    } else {
      Error() << "Values for enter record should be of size 2, but found "
              << NumValues << ": " << Record << "\n";
      return false;
    }
    uint64_t MaxAbbrev = (static_cast<uint64_t>(1) << NumBits) - 1;
    AbbrevIndexLimitStack.push_back(MaxAbbrev);
    if (WriteBlockID == naclbitc::BLOCKINFO_BLOCK_ID) {
      unsigned DefaultMaxBits =
          NaClBitsNeededForValue(naclbitc::DEFAULT_MAX_ABBREV);
      if (NumBits != DefaultMaxBits) {
        RecoverableError()
            << "Numbits entry for abbreviations record not "
            << DefaultMaxBits << " but found " << NumBits <<
            ": " << Record << "\n";
        if (!Flags.getTryToRecover())
          return false;
      }
      Writer.EnterBlockInfoBlock();
    } else {
      NaClBitcodeSelectorAbbrev CurCodeLen(MaxAbbrev);
      Writer.EnterSubblock(WriteBlockID, CurCodeLen);
    }
    break;
  }
  case naclbitc::BLK_CODE_EXIT:
    if (Record.Abbrev != naclbitc::END_BLOCK) {
      RecoverableError()
          << "Uses illegal abbreviation index in exit block record: "
          << Record << "\n";
      if (!Flags.getTryToRecover())
        return false;
    }
    if (NumValues != 0) {
      RecoverableError() << "Exit block should not have values: "
                         << Record << "\n";
      if (!Flags.getTryToRecover())
        return false;
    }
    if (!AbbrevIndexLimitStack.empty())
      AbbrevIndexLimitStack.pop_back();
    Writer.ExitBlock();
    break;
  case naclbitc::BLK_CODE_DEFINE_ABBREV: {
    if (Record.Abbrev != naclbitc::DEFINE_ABBREV) {
      RecoverableError()
          << "Uses illegal abbreviation index in define abbreviation record: "
          << Record << "\n";
      if (!Flags.getTryToRecover())
        return false;
    }
    NaClBitCodeAbbrev *Abbrev = buildAbbrev(Record);
    if (Abbrev == NULL)
      return Flags.getTryToRecover();
    if (WriteBlockID == naclbitc::BLOCKINFO_BLOCK_ID) {
      Writer.EmitBlockInfoAbbrev(SetBID, Abbrev);
    } else {
      Writer.EmitAbbrev(Abbrev);
    }
    break;
  }
  case naclbitc::BLK_CODE_HEADER:
    // Note: There is no abbreviation index here. Ignore.
    for (uint64_t Value : Record.Values)
      Writer.Emit(Value, 8);
    break;
  default:
    if (AbbrevIndexLimitStack.empty()) {
      Error() << "Can't write record outside record block: " << Record << "\n";
      return false;
    }
    bool UsesDefaultAbbrev = Record.Abbrev == naclbitc::UNABBREV_RECORD;
    if (!UsesDefaultAbbrev
        && !Writer.isUserRecordAbbreviation(Record.Abbrev)) {
      // Illegal abbreviation index found.
      if (Flags.getWriteBadAbbrevIndex()) {
        Error() << "Uses illegal abbreviation index: " << Record << "\n";
        // Generate bad abbreviation index so that the bitcode reader
        // can be tested.
        Results.WroteBadAbbrevIndex = true;
        Writer.EmitCode(Record.Abbrev);
        // Note: We need to close blocks or the bitcode Writer will terminate
        // due to assertions.
        writeMissingEndBlocks(Writer);
        return false;
      }
      RecoverableError() << "Uses illegal abbreviation index: "
                         << Record << "\n";
      if (!Flags.getTryToRecover())
        return false;
      UsesDefaultAbbrev = true;
    }
    if (WriteBlockID == naclbitc::BLOCKINFO_BLOCK_ID
        && Record.Code == naclbitc::BLOCKINFO_CODE_SETBID) {
      // Note: SetBID records are handled by Writer->EmitBlockInfoAbbrev,
      // based on the SetBID value. Don't bother to generate SetBID record here.
      // Rather just set SetBID and let call to Writer->EmitBlockInfoAbbrev
      // generate the SetBID record.
      if (NumValues != 1) {
        Error() << "SetBID record expects 1 value but found "
                << NumValues << ": " << Record << "\n";
        return false;
      }
      SetBID = Record.Values[0];
      return true;
    }
    if (UsesDefaultAbbrev)
      Writer.EmitRecord(Record.Code, Record.Values);
    else
      Writer.EmitRecord(Record.Code, Record.Values, Record.Abbrev);
  }
  return true;
}

static NaClBitCodeAbbrev *deleteAbbrev(NaClBitCodeAbbrev *Abbrev) {
  Abbrev->dropRef();
  return nullptr;
}

NaClBitCodeAbbrev *WriteState::buildAbbrev(
    const NaClBitcodeAbbrevRecord &Record) {
  // Note: Recover by removing abbreviation.
  NaClBitCodeAbbrev *Abbrev = new NaClBitCodeAbbrev();
  size_t Index = 0;
  size_t NumValues = Record.Values.size();
  if (NumValues == 0) {
    RecoverableError() << "Empty abbreviation record not allowed: "
                       << Record << "\n";
    return deleteAbbrev(Abbrev);
  }
  size_t NumAbbreviations = Record.Values[Index++];
  if (NumAbbreviations == 0) {
    RecoverableError() << "Abbreviation must contain at least one operator: "
                       << Record << "\n";
    return deleteAbbrev(Abbrev);
  }
  for (size_t Count = 0; Count < NumAbbreviations; ++Count) {
    if (Index >= NumValues) {
      RecoverableError()
          << "Malformed abbreviation found. Expects " << NumAbbreviations
          << " operands but ound " << Count << ": " << Record << "\n";
      return deleteAbbrev(Abbrev);
    }
    switch (Record.Values[Index++]) {
    case 1:
      if (Index >= NumValues) {
        RecoverableError() << "Malformed literal abbreviation: "
                           << Record << "\n";
        return deleteAbbrev(Abbrev);
      }
      Abbrev->Add(NaClBitCodeAbbrevOp(Record.Values[Index++]));
      break;
    case 0: {
      if (Index >= NumValues) {
        RecoverableError() << "Malformed abbreviation found: "
                           << Record << "\n";
        return deleteAbbrev(Abbrev);
      }
      unsigned Kind = Record.Values[Index++];
      switch (Kind) {
      case NaClBitCodeAbbrevOp::Fixed:
        if (Index >= NumValues) {
          RecoverableError() << "Malformed fixed abbreviation found: "
                             << Record << "\n";
          return deleteAbbrev(Abbrev);
        }
        Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed,
                                        Record.Values[Index++]));
        break;
      case NaClBitCodeAbbrevOp::VBR:
        if (Index >= NumValues) {
          RecoverableError() << "Malformed vbr abbreviation found: "
                             << Record << "\n";
          return deleteAbbrev(Abbrev);
        }
        Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR,
                                        Record.Values[Index++]));
        break;
      case NaClBitCodeAbbrevOp::Array:
        if (Index >= NumValues) {
          RecoverableError() << "Malformed array abbreviation found: "
                             << Record << "\n";
          return deleteAbbrev(Abbrev);
        }
        Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Array));
        break;
      case NaClBitCodeAbbrevOp::Char6:
        Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Char6));
        break;
      default:
        RecoverableError() << "Unknown abbreviation kind " << Kind
                           << ": " << Record << "\n";
        return deleteAbbrev(Abbrev);
      }
      break;
    }
    default:
      RecoverableError() << "Error: Bad literal flag " << Record.Values[Index]
                         << ": " << Record << "\n";
      return deleteAbbrev(Abbrev);
    }
  }
  return Abbrev;
}

} // end of anonymous namespace.

NaClMungedBitcode::WriteResults NaClMungedBitcode::writeMaybeRepair(
    SmallVectorImpl<char> &Buffer, bool AddHeader,
    const WriteFlags &Flags) const {
  NaClBitstreamWriter Writer(Buffer);
  WriteState State(Flags);
  if (AddHeader) {
    NaClWriteHeader(Writer, true);
  }
  for (const NaClBitcodeAbbrevRecord &Record : *this) {
    if (!State.emitRecord(Writer, Record))
      break;
  }
  if (!State.AbbrevIndexLimitStack.empty()) {
    State.RecoverableError()
        << "Bitcode missing " << State.AbbrevIndexLimitStack.size()
        << " close blocks.\n";
    if (Flags.getTryToRecover())
      State.writeMissingEndBlocks(Writer);
  }
  return State.Results;
}
