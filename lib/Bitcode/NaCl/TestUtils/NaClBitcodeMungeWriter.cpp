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
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

// For debugging. When true, shows trace information of emitting
// bitcode.
static bool DebugEmit = false;

// Description of current block scope
struct BlockScope {
  BlockScope(unsigned CurBlockID, size_t AbbrevIndexLimit)
      : CurBlockID(CurBlockID), AbbrevIndexLimit(AbbrevIndexLimit) {}
  unsigned CurBlockID;
  // Defines the maximum value for abbreviation indices in block.
  size_t AbbrevIndexLimit;
  void print(raw_ostream &Out) const {
    Out << "BlockScope(ID=" << CurBlockID << ", AbbrevIndexLimit="
        << AbbrevIndexLimit << ")";
  }
};

inline raw_ostream &operator<<(raw_ostream &Out, const BlockScope &Scope) {
  Scope.print(Out);
  return Out;
}

// State of bitcode writing.
struct WriteState {
  // The block ID associated with records not in any block.
  static const unsigned UnknownWriteBlockID = UINT_MAX;
  // The SetBID for the blockinfo block.
  unsigned SetBID = UnknownWriteBlockID;
  // The stack of scopes the writer is in.
  SmallVector<BlockScope, 3> ScopeStack;
  // The set of write flags to use.
  const NaClMungedBitcode::WriteFlags &Flags;
  // The results of the attempted write.
  NaClMungedBitcode::WriteResults Results;
  // The minimum number of bits allowed to be specified in a block.
  const unsigned BlockMinBits;

  WriteState(const NaClMungedBitcode::WriteFlags &Flags)
      : Flags(Flags),
        BlockMinBits(NaClBitsNeededForValue(naclbitc::DEFAULT_MAX_ABBREV)) {
    BlockScope Scope(UnknownWriteBlockID, naclbitc::DEFAULT_MAX_ABBREV);
    ScopeStack.push_back(Scope);
  }

  // Returns stream to print error message to.
  raw_ostream &Error();

  // Returns stream to print error message to, assuming that
  // the error message can be repaired if Flags.TryToRecover is true.
  raw_ostream &RecoverableError() {
    if (Flags.getTryToRecover())
      ++Results.NumRepairs;
    return Error();
  }

  bool atOutermostScope() {
    assert(!ScopeStack.empty());
    return ScopeStack.size() == 1;
  }

  unsigned getCurWriteBlockID() const {
    assert(!ScopeStack.empty());
    return ScopeStack.back().CurBlockID;
  }

  unsigned getCurAbbrevIndexLimit() const {
    assert(!ScopeStack.empty());
    return ScopeStack.back().AbbrevIndexLimit;
  }

  // Converts the abbreviation record to the corresponding abbreviation.
  // Returns nullptr if unable to build abbreviation.
  NaClBitCodeAbbrev *buildAbbrev(const NaClBitcodeAbbrevRecord &Record);

  // Emits the given record to the bitcode file. Returns true if
  // successful.
  bool LLVM_ATTRIBUTE_UNUSED_RESULT
  emitRecord(NaClBitstreamWriter &Writer, const NaClBitcodeAbbrevRecord &Record);

  // Enter the given block
  bool LLVM_ATTRIBUTE_UNUSED_RESULT
  enterBlock(NaClBitstreamWriter &Writer, uint64_t WriteBlockID,
             uint64_t NumBits, const NaClBitcodeAbbrevRecord &Record);

  // Exit current block and return to previous block. Silently recovers
  // if at outermost scope.
  bool LLVM_ATTRIBUTE_UNUSED_RESULT
  exitBlock(NaClBitstreamWriter &Writer,
            const NaClBitcodeAbbrevRecord *Record = nullptr) {
    if (atOutermostScope())
      return false;
    Writer.ExitBlock();
    ScopeStack.pop_back();
    if (DebugEmit)
      printScopeStack(errs());
    return true;
  }

  // Completes the write.
  NaClMungedBitcode::WriteResults &finish(NaClBitstreamWriter &Writer,
                                          bool RecoverSilently);

  void printScopeStack(raw_ostream &Out) {
    Out << "Scope Stack:\n";
    for (auto &Scope : ScopeStack)
      Out << "  " << Scope << "\n";
  }
};

raw_ostream &WriteState::Error() {
  ++Results.NumErrors;
  raw_ostream &ErrStrm = Flags.getErrStream();
  unsigned WriteBlockID = getCurWriteBlockID();
  ErrStrm << "Error (Block ";
  if (WriteBlockID == UnknownWriteBlockID)
    ErrStrm << "unknown";
  else
    ErrStrm << WriteBlockID;
  return ErrStrm <<  "): ";
}

bool WriteState::enterBlock(NaClBitstreamWriter &Writer, uint64_t WriteBlockID,
                            uint64_t NumBits,
                            const NaClBitcodeAbbrevRecord &Record) {
  if (NumBits < BlockMinBits || NumBits > naclbitc::MaxAbbrevWidth) {
    RecoverableError()
        << "Block index bit limit " << NumBits << " invalid. Must be in ["
        << BlockMinBits << ".." << naclbitc::MaxAbbrevWidth << "]: "
        << Record << "\n";
    if (!Flags.getTryToRecover())
      return false;
    NumBits = naclbitc::MaxAbbrevWidth;
  }
  if (WriteBlockID > UINT_MAX) {
    RecoverableError() << "Block id must be <= " << UINT_MAX
                       << ": " << Record << "\n";
    if (!Flags.getTryToRecover())
      return false;
    WriteBlockID = UnknownWriteBlockID;
  }

  uint64_t MaxAbbrev = (static_cast<uint64_t>(1) << NumBits) - 1;
  BlockScope Scope(WriteBlockID, MaxAbbrev);
  ScopeStack.push_back(Scope);
  if (DebugEmit)
    printScopeStack(errs());
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
  return true;
}

NaClMungedBitcode::WriteResults &WriteState::finish(
    NaClBitstreamWriter &Writer, bool RecoverSilently) {
  // Be sure blocks are balanced.
  while (!atOutermostScope()) {
    if (!RecoverSilently)
      RecoverableError() << "Missing close block.\n";
    if (!exitBlock(Writer)) {
      Error() << "Failed to add missing close block at end of file.\n";
      break;
    }
  }

  // Be sure that generated bitcode buffer is word aligned.
  if (Writer.GetCurrentBitNo() % 4 * CHAR_BIT) {
    if (!RecoverSilently)
      RecoverableError() << "Written bitstream not word aligned\n";
    // Force a repair so that the bitstream writer doesn't crash.
    Writer.FlushToWord();
  }
  return Results;
}

bool WriteState::emitRecord(NaClBitstreamWriter &Writer,
                            const NaClBitcodeAbbrevRecord &Record) {
  size_t NumValues = Record.Values.size();
  if (DebugEmit) {
    errs() << "Emit " << Record.Abbrev << ": <" << Record.Code;
    for (size_t i = 0; i < NumValues; ++i) {
      errs() << ", " << Record.Values[i];
    }
    errs() << ">\n";
  }

  switch (Record.Code) {
  case naclbitc::BLK_CODE_ENTER: {
    uint64_t WriteBlockID = UnknownWriteBlockID;
    uint64_t NumBits = naclbitc::MaxAbbrevWidth; // Default to safest value.
    if (Record.Abbrev != naclbitc::ENTER_SUBBLOCK) {
      RecoverableError()
          << "Uses illegal abbreviation index in enter block record: "
          << Record << "\n";
      if (!Flags.getTryToRecover())
        return false;
    }
    if (NumValues != 2) {
      RecoverableError()
          << "Values for enter record should be of size 2, but found "
          << NumValues << ": " << Record << "\n";
      if (!Flags.getTryToRecover())
        return false;
    }
    if (NumValues > 0)
      WriteBlockID = Record.Values[0];
    if (NumValues > 1)
      NumBits = Record.Values[1];
    return enterBlock(Writer, WriteBlockID, NumBits, Record);
  }
  case naclbitc::BLK_CODE_EXIT: {
    if (atOutermostScope()) {
      RecoverableError()
          << "Extraneous exit block: " << Record << "\n";
      if (!Flags.getTryToRecover())
        return false;
      break;
    }
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
    if (!exitBlock(Writer)) {
      Error() << "Failed to write exit block, can't continue: "
              << Record << "\n";
      return false;
    }
    break;
  }
  case naclbitc::BLK_CODE_DEFINE_ABBREV: {
    if (Record.Abbrev != naclbitc::DEFINE_ABBREV) {
      RecoverableError()
          << "Uses illegal abbreviation index in define abbreviation record: "
          << Record << "\n";
      if (!Flags.getTryToRecover())
        return false;
    }
    if (getCurWriteBlockID() != naclbitc::BLOCKINFO_BLOCK_ID
        && Writer.getMaxCurAbbrevIndex() >= getCurAbbrevIndexLimit()) {
      RecoverableError() << "Exceeds abbreviation index limit of "
                         << getCurAbbrevIndexLimit() << ": " << Record << "\n";
      // Recover by not writing.
      return Flags.getTryToRecover();
    }
    NaClBitCodeAbbrev *Abbrev = buildAbbrev(Record);
    if (Abbrev == NULL)
      return Flags.getTryToRecover();
    if (getCurWriteBlockID() == naclbitc::BLOCKINFO_BLOCK_ID) {
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
    bool UsesDefaultAbbrev = Record.Abbrev == naclbitc::UNABBREV_RECORD;
    if (atOutermostScope()) {
      RecoverableError() << "Record outside block: " << Record << "\n";
      if (!Flags.getTryToRecover())
        return false;
      // Create a dummy block to hold record.
      if (!enterBlock(Writer, UnknownWriteBlockID,
                      naclbitc::DEFAULT_MAX_ABBREV, Record)) {
        Error() << "Failed to recover from record outside block\n";
        return false;
      }
      UsesDefaultAbbrev = true;
    }
    if (!UsesDefaultAbbrev
        && !Writer.isUserRecordAbbreviation(Record.Abbrev)) {
      // Illegal abbreviation index found.
      if (Flags.getWriteBadAbbrevIndex()) {
        Error() << "Uses illegal abbreviation index: " << Record << "\n";
        // Generate bad abbreviation index so that the bitcode reader
        // can be tested, and then quit.
        Results.WroteBadAbbrevIndex = true;
        Writer.EmitCode(Record.Abbrev);
        bool RecoverSilently = true;
        finish(Writer, RecoverSilently);
        return false;
      }
      RecoverableError() << "Uses illegal abbreviation index: "
                         << Record << "\n";
      if (!Flags.getTryToRecover())
        return false;
      UsesDefaultAbbrev = true;
    }
    if (getCurWriteBlockID() == naclbitc::BLOCKINFO_BLOCK_ID
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
  bool RecoverSilently =
      State.Results.NumErrors > 0 && !Flags.getTryToRecover();
  return State.finish(Writer, RecoverSilently);
}
