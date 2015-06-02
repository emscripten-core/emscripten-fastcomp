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

#include <set>

using namespace llvm;

namespace {

// For debugging. When true, shows trace information of emitting
// bitcode.
static bool DebugEmit = false;

// Description of current block scope
struct BlockScope {
  BlockScope(unsigned CurBlockID, size_t AbbrevIndexLimit)
      : CurBlockID(CurBlockID), AbbrevIndexLimit(AbbrevIndexLimit),
        OmittedAbbreviations(false) {}
  unsigned CurBlockID;
  // Defines the maximum value for abbreviation indices in block.
  size_t AbbrevIndexLimit;
  // Defines if an abbreviation definition was omitted (i.e. not
  // written) from this block. Used to turn off writing further
  // abbreviation definitions for this block.
  bool OmittedAbbreviations;
  void print(raw_ostream &Out) const {
    Out << "BlockScope(ID=" << CurBlockID << ", AbbrevIndexLimit="
        << AbbrevIndexLimit << "OmittedAbbreviations="
        << OmittedAbbreviations << ")";
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
  SmallVector<BlockScope, 4> ScopeStack;
  // The set of write flags to use.
  const NaClMungedBitcode::WriteFlags &Flags;
  // The results of the attempted write.
  NaClMungedBitcode::WriteResults Results;
  // The minimum number of bits allowed to be specified in a block.
  const unsigned BlockMinBits;
  // The set of block IDs for which abbreviation definitions have been
  // omitted in the blockinfo block.
  std::set<unsigned> BlocksWithOmittedAbbrevs;

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

  // Returns whether any abbreviation definitions were not written to
  // the bitcode buffer.
  bool curBlockHasOmittedAbbreviations() const {
    assert(!ScopeStack.empty());
    return ScopeStack.back().OmittedAbbreviations
        || BlocksWithOmittedAbbrevs.count(getCurWriteBlockID());
  }

  // Marks that an abbreviation definition is being omitted (i.e. not
  // written) for the current block.
  void markCurrentBlockWithOmittedAbbreviations() {
    assert(!ScopeStack.empty());
    ScopeStack.back().OmittedAbbreviations = true;
    if (getCurWriteBlockID() == naclbitc::BLOCKINFO_BLOCK_ID)
      BlocksWithOmittedAbbrevs.insert(SetBID);
  }

  // Returns true if abbreviation operand is legal. If not, logs
  // a recoverable error message and returns false. Assumes that
  // the caller does the actual error recovery if applicable.
  bool verifyAbbrevOp(NaClBitCodeAbbrevOp::Encoding Encoding, uint64_t Value,
                      const NaClBitcodeAbbrevRecord &Record);

  // Converts the abbreviation record to the corresponding abbreviation.
  // Returns nullptr if unable to build abbreviation.
  NaClBitCodeAbbrev *buildAbbrev(const NaClBitcodeAbbrevRecord &Record);

  // Gets the next value (based on Index) from the record values,
  // assigns it to ExtractedValue, and returns true. Otherwise, logs a
  // recoverable error message and returns false. Assumes that the
  // caller does the actual error recovery if applicable.
  bool LLVM_ATTRIBUTE_UNUSED_RESULT
  nextAbbrevValue(uint64_t &ExtractedValue,
                  const NaClBitcodeAbbrevRecord &Record, size_t &Index) {
    if (Index >= Record.Values.size()) {
      RecoverableError()
          << "Malformed abbreviation found: " << Record << "\n";
      return false;
    }
    ExtractedValue = Record.Values[Index++];
    return true;
  }

  // Emits the given record to the bitcode file. Returns true if
  // successful.
  bool LLVM_ATTRIBUTE_UNUSED_RESULT
  emitRecord(NaClBitstreamWriter &Writer,
             const NaClBitcodeAbbrevRecord &Record);

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

  void WriteRecord(NaClBitstreamWriter &Writer,
                   const NaClBitcodeAbbrevRecord &Record,
                   bool UsesDefaultAbbrev) {
    if (UsesDefaultAbbrev)
      Writer.EmitRecord(Record.Code, Record.Values);
    else
      Writer.EmitRecord(Record.Code, Record.Values, Record.Abbrev);
  }

  // Returns true if the abbreviation index defines an abbreviation
  // that can be applied to the record.
  bool LLVM_ATTRIBUTE_UNUSED_RESULT
  canApplyAbbreviation(NaClBitstreamWriter &Writer,
                       const NaClBitcodeAbbrevRecord &Record);

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

bool WriteState::canApplyAbbreviation(
    NaClBitstreamWriter &Writer, const NaClBitcodeAbbrevRecord &Record) {
  const NaClBitCodeAbbrev *Abbrev = Writer.getAbbreviation(Record.Abbrev);
  if (Abbrev == nullptr)
    return false;

  // Merge record code into values and then match abbreviation.
  NaClBitcodeValues Values(Record);
  size_t ValueIndex = 0;
  size_t ValuesSize = Values.size();
  size_t AbbrevIndex = 0;
  size_t AbbrevSize = Abbrev->getNumOperandInfos();
  bool FoundArray = false;
  while (ValueIndex < ValuesSize && AbbrevIndex < AbbrevSize) {
    const NaClBitCodeAbbrevOp *Op = &Abbrev->getOperandInfo(AbbrevIndex++);
    uint64_t Value = Values[ValueIndex++];
    if (Op->getEncoding() == NaClBitCodeAbbrevOp::Array) {
      if (AbbrevIndex + 1 != AbbrevSize)
        return false;
      Op = &Abbrev->getOperandInfo(AbbrevIndex);
      --AbbrevIndex; // i.e. don't advance to next abbreviation op.
      FoundArray = true;
    }
    switch (Op->getEncoding()) {
    case NaClBitCodeAbbrevOp::Literal:
      if (Value != Op->getValue())
        return false;
      continue;
    case NaClBitCodeAbbrevOp::Fixed:
      if (Value >= (static_cast<uint64_t>(1)
                    << NaClBitstreamWriter::MaxEmitNumBits)
          || NaClBitsNeededForValue(Value) > Op->getValue())
        return false;
      continue;
    case NaClBitCodeAbbrevOp::VBR:
      if (Op->getValue() < 2)
        return false;
      continue;
    case NaClBitCodeAbbrevOp::Array:
      llvm_unreachable("Array(Array) abbreviation is not legal!");
    case NaClBitCodeAbbrevOp::Char6:
      if (!Op->isChar6(Value))
        return false;
      continue;
    }
  }
  return ValueIndex == ValuesSize && (FoundArray || AbbrevIndex == AbbrevSize);
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
    if (curBlockHasOmittedAbbreviations()) {
      // If reached, a previous abbreviation for the block was omitted. Can't
      // generate more abbreviations without having to fix abbreviation indices.
      RecoverableError() << "Ignoring abbreviation: " << Record << "\n";
      return Flags.getTryToRecover();
    }
    if (Record.Abbrev != naclbitc::DEFINE_ABBREV) {
      RecoverableError()
          << "Uses illegal abbreviation index in define abbreviation record: "
          << Record << "\n";
      if (!Flags.getTryToRecover())
        return false;
    }
    NaClBitCodeAbbrev *Abbrev = buildAbbrev(Record);
    if (Abbrev == nullptr) {
      markCurrentBlockWithOmittedAbbreviations();
      return Flags.getTryToRecover();
    }
    if (atOutermostScope()) {
      RecoverableError() << "Abbreviation definition not in block: "
                         << Record << "\n";
      return Flags.getTryToRecover();
    }
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
    if (!UsesDefaultAbbrev && !canApplyAbbreviation(Writer, Record)) {
      if (Writer.getAbbreviation(Record.Abbrev) != nullptr) {
        RecoverableError() << "Abbreviation doesn't apply to record: "
                           << Record << "\n";
        UsesDefaultAbbrev = true;
        if (!Flags.getTryToRecover())
          return false;
        WriteRecord(Writer, Record, UsesDefaultAbbrev);
        return true;
      }
      if (Flags.getWriteBadAbbrevIndex()) {
        // The abbreviation is not understood by the bitcode writer,
        // and the flag value implies that we should still write it
        // out so that unit tests for this error condition can be
        // applied.
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
      UsesDefaultAbbrev = true;
      if (!Flags.getTryToRecover())
        return false;
      WriteRecord(Writer, Record, UsesDefaultAbbrev);
      return true;
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
    WriteRecord(Writer, Record, UsesDefaultAbbrev);
    break;
  }
  return true;
}

static NaClBitCodeAbbrev *deleteAbbrev(NaClBitCodeAbbrev *Abbrev) {
  Abbrev->dropRef();
  return nullptr;
}

bool WriteState::verifyAbbrevOp(NaClBitCodeAbbrevOp::Encoding Encoding,
                                uint64_t Value,
                                const NaClBitcodeAbbrevRecord &Record) {
  if (NaClBitCodeAbbrevOp::isValid(Encoding, Value))
    return true;
  RecoverableError() << "Invalid abbreviation "
                     << NaClBitCodeAbbrevOp::getEncodingName(Encoding)
                     << "(" << static_cast<int64_t>(Value)
                     << ") in: " << Record << "\n";
  return false;
}

NaClBitCodeAbbrev *WriteState::buildAbbrev(
    const NaClBitcodeAbbrevRecord &Record) {
  // Note: Recover by removing abbreviation.
  NaClBitCodeAbbrev *Abbrev = new NaClBitCodeAbbrev();
  size_t Index = 0;
  uint64_t NumAbbrevOps;
  if (!nextAbbrevValue(NumAbbrevOps, Record, Index))
    return deleteAbbrev(Abbrev);
  if (NumAbbrevOps == 0) {
    RecoverableError() << "Abbreviation must contain at least one operator: "
                       << Record << "\n";
    return deleteAbbrev(Abbrev);
  }
  for (uint64_t Count = 0; Count < NumAbbrevOps; ++Count) {
    uint64_t IsLiteral;
    if (!nextAbbrevValue(IsLiteral, Record, Index))
      return deleteAbbrev(Abbrev);
    switch (IsLiteral) {
    case 1: {
      uint64_t Value;
      if (!nextAbbrevValue(Value, Record, Index))
        return deleteAbbrev(Abbrev);
      if (!verifyAbbrevOp(NaClBitCodeAbbrevOp::Literal, Value, Record))
        return deleteAbbrev(Abbrev);
      Abbrev->Add(NaClBitCodeAbbrevOp(Value));
      break;
    }
    case 0: {
      uint64_t Kind;
      if (!nextAbbrevValue(Kind, Record, Index))
        return deleteAbbrev(Abbrev);
      switch (Kind) {
      case NaClBitCodeAbbrevOp::Fixed: {
        uint64_t Value;
        if (!nextAbbrevValue(Value, Record, Index))
          return deleteAbbrev(Abbrev);
        if (!verifyAbbrevOp(NaClBitCodeAbbrevOp::Fixed, Value, Record))
          return deleteAbbrev(Abbrev);
        Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, Value));
        break;
      }
      case NaClBitCodeAbbrevOp::VBR: {
        uint64_t Value;
        if (!nextAbbrevValue(Value, Record, Index))
          return deleteAbbrev(Abbrev);
        if (!verifyAbbrevOp(NaClBitCodeAbbrevOp::VBR, Value, Record))
          return deleteAbbrev(Abbrev);
        Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, Value));
        break;
      }
      case NaClBitCodeAbbrevOp::Array:
        if (Count + 2 != NumAbbrevOps) {
          RecoverableError() << "Array abbreviation must be second to last: "
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
      RecoverableError() << "Bad abbreviation operand encoding "
                         << Record.Values[Index-1] << ": " << Record << "\n";
      return deleteAbbrev(Abbrev);
    }
  }
  if (Index != Record.Values.size()) {
    RecoverableError() << "Error: Too many values for number of operands ("
                       << NumAbbrevOps << "): "
                       << Record << "\n";
    return deleteAbbrev(Abbrev);
  }
  if (!Abbrev->isValid()) {
    raw_ostream &Out = RecoverableError();
    Abbrev->Print(Out << "Abbreviation ");
    Out << " not valid: " << Record << "\n";
    return deleteAbbrev(Abbrev);
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
