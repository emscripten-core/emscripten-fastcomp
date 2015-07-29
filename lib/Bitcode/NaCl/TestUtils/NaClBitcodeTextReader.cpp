//===- NaClBitcodeTextReader.cpp - Read texual bitcode record list --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Implements bitcode reader from textual form.
//
// A Textual bitcode file is a sequence of textual bitcode records.
// A Textual bitcode record is a sequence of integers, separated by
// commas, and terminated with a semicolon followed by a newline.
//
// In addition, unlike the binary form of bitcode, the input has no
// bitcode header record.
// ===---------------------------------------------------------------------===//

#include "llvm/ADT/STLExtras.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeMungeUtils.h"
#include "llvm/Bitcode/NaCl/NaClReaderWriter.h"

#include <cstring>

namespace llvm {
class Module;
} // end of namespace llvm

using namespace llvm;

namespace {

// Defines a text reader error code.
enum ReaderErrorType {
  NoCodeForRecord=1,  // Note: Error types must not be zero!
  NoValueAfterSeparator,
  NoSeparatorOrTerminator,
  NoNewlineAfterTerminator,
  BitcodeHeaderNotAllowed,
  NoAbbreviationsAllowed,
  UnableToWriteBitcode
};

// Defines the corresponding error messages.
class ReaderErrorCategoryType : public std::error_category {
  ReaderErrorCategoryType(const ReaderErrorCategoryType&) = delete;
  void operator=(const ReaderErrorCategoryType&) = delete;
public:
  static const ReaderErrorCategoryType &get() {
    return Sentinel;
  }
private:
  static const ReaderErrorCategoryType Sentinel;
  ReaderErrorCategoryType() {}
  const char *name() const LLVM_NOEXCEPT override {
    return "pnacl.text_bitcode";
  }
  std::string message(int IndexError) const override {
    switch(static_cast<ReaderErrorType>(IndexError)) {
    case NoCodeForRecord:
      return "Bitcode record doesn't begin with a record code";
    case NoValueAfterSeparator:
      return "Value expected after separator, but not found";
    case NoSeparatorOrTerminator:
      return "Separator/terminator expected after value";
    case NoNewlineAfterTerminator:
      return "Newline expecded after terminating semicolon";
    case BitcodeHeaderNotAllowed:
      return "Bitcode headers not allowed in bitcode text";
    case NoAbbreviationsAllowed:
      return "Bitcode abbreviations not allowed in bitcode text";
    case UnableToWriteBitcode:
      return "Unable to generate bitcode buffer from textual bitcode records";
    }
    llvm_unreachable("Unknown error type!");
  }
  ~ReaderErrorCategoryType() override = default;
};

const ReaderErrorCategoryType ReaderErrorCategoryType::Sentinel;

/// Parses text bitcode records, and adds them to an existing list of
/// bitcode records.
class TextRecordParser {
  TextRecordParser(const TextRecordParser&) = delete;
  void operator=(const TextRecordParser&) = delete;
public:
  /// Creates a parser to parse records from the InputBuffer, and
  /// append them to the list of Records.
  TextRecordParser(NaClBitcodeRecordList &Records,
                   MemoryBuffer *InputBuffer)
      : Records(Records), Buffer(InputBuffer->getBuffer()) {}

  /// Reads in the list of bitcode records in the input buffer.
  std::error_code read();

private:
  // The list of bitcode records to generate.
  NaClBitcodeRecordList &Records;
  // The input buffer to parse.
  StringRef Buffer;
  // The current location within the input buffer.
  size_t Cursor = 0;
  // The separator character.
  static const char *Separator;
  // The terminator character.
  static const char *Terminator;
  // The newline character that must follow a terminator.
  static const char *Newline;
  // Valid digits that can be used to define numbers.
  static const char *Digits;

  // Returns true if we have reached the end of the input buffer.
  bool atEof() const {
    return Cursor == Buffer.size();
  }

  // Tries to read a character in the given set of characters. Returns
  // the character found, or 0 if not found.
  char readChar(const char *Chars);

  // Tries to read a (integral) number. If successful, Value is set to
  // the parsed number and returns true. Otherwise false is returned.
  // Does not check for number overflow.
  bool readNumber(uint64_t &Value);

  // Reads a record from the input buffer.
  std::error_code readRecord();
};

const char *TextRecordParser::Newline = "\n";
const char *TextRecordParser::Separator = ",";
const char *TextRecordParser::Terminator = ";";
const char *TextRecordParser::Digits = "0123456789";

std::error_code TextRecordParser::read() {
  while (!atEof()) {
    if (std::error_code EC = readRecord())
      return EC;
  }
  return std::error_code();
}

char TextRecordParser::readChar(const char *Chars) {
  if (atEof())
    return 0;
  char Ch = Buffer[Cursor];
  if (std::strchr(Chars, Ch) == 0)
    return 0;
  ++Cursor;
  return Ch;
}

bool TextRecordParser::readNumber(uint64_t &Value) {
  Value = 0;
  bool NumberFound = false;
  while (1) {
    char Ch = readChar(Digits);
    if (!Ch)
      return NumberFound;
    Value = (Value * 10) + (Ch - '0');
    NumberFound = true;
  }
}

std::error_code TextRecordParser::readRecord() {
  // States of parser used to parse bitcode records.
  enum ParseState {
    // Begin parsing a record.
    StartParse,
    // Before a value in the record.
    BeforeValue,
    // Immediately after a value in the record.
    AfterValue,
    // Completed parsing a record.
    FinishParse
  } State = StartParse;
  unsigned Code = 0;
  NaClRecordVector Values;
  uint64_t Number = 0;
  while (1) {
    switch (State) {
      case StartParse:
        if (!readNumber(Number)) {
          if (atEof())
            return std::error_code();
          return std::error_code(NoCodeForRecord,
                                 ReaderErrorCategoryType::get());
        }
        Code = Number;
        State = AfterValue;
        continue;
      case BeforeValue:
        if (!readNumber(Number))
          return std::error_code(NoValueAfterSeparator,
                                 ReaderErrorCategoryType::get());
        Values.push_back(Number);
        State = AfterValue;
        continue;
      case AfterValue:
        if (readChar(Separator)) {
          State = BeforeValue;
          continue;
        }
        if (readChar(Terminator)) {
          State = FinishParse;
          if (!readChar(Newline))
            return std::error_code(NoNewlineAfterTerminator,
                                   ReaderErrorCategoryType::get());
          continue;
        }
        return std::error_code(NoSeparatorOrTerminator,
                               ReaderErrorCategoryType::get());
      case FinishParse: {
        unsigned Abbrev = naclbitc::UNABBREV_RECORD;
        switch (Code) {
        case naclbitc::BLK_CODE_ENTER:
          Abbrev = naclbitc::ENTER_SUBBLOCK;
          break;
        case naclbitc::BLK_CODE_EXIT:
          Abbrev = naclbitc::END_BLOCK;
          break;
        case naclbitc::BLK_CODE_HEADER:
          return std::error_code(BitcodeHeaderNotAllowed,
                                 ReaderErrorCategoryType::get());
        case naclbitc::BLK_CODE_DEFINE_ABBREV:
          return std::error_code(NoAbbreviationsAllowed,
                                 ReaderErrorCategoryType::get());
        default: // All other records.
          break;
        }
        Records.push_back(make_unique<NaClBitcodeAbbrevRecord>(
            Abbrev, Code, Values));
        return std::error_code();
      }
    }
  }
}

} // end of anonymous namespace

std::error_code llvm::readNaClRecordTextAndBuildBitcode(
    StringRef Filename, SmallVectorImpl<char> &Buffer, raw_ostream *Verbose) {
  // Open the input file with text records.
  ErrorOr<std::unique_ptr<MemoryBuffer>>
      MemBuf(MemoryBuffer::getFileOrSTDIN(Filename));
  if (!MemBuf)
    return MemBuf.getError();

  // Read in the bitcode text records.
  std::unique_ptr<NaClBitcodeRecordList> Records(new NaClBitcodeRecordList());
  if (std::error_code EC =
      readNaClTextBcRecordList(*Records, std::move(MemBuf.get())))
    return EC;

  // Write out the records into Buffer.
  NaClMungedBitcode Bitcode(std::move(Records));
  NaClMungedBitcode::WriteFlags Flags;
  if (Verbose)
    Flags.setErrStream(*Verbose);
  bool AddHeader = true;
  if (!Bitcode.write(Buffer, AddHeader)) {
    return std::error_code(UnableToWriteBitcode,
                           ReaderErrorCategoryType::get());
  }
  return std::error_code();
}

std::error_code llvm::readNaClTextBcRecordList(
    NaClBitcodeRecordList &RecordList,
    std::unique_ptr<MemoryBuffer> InputBuffer) {
  TextRecordParser Parser(RecordList, InputBuffer.get());
  return Parser.read();
}

llvm::ErrorOr<llvm::Module *>
llvm::parseNaClBitcodeText(const std::string &Filename, LLVMContext &Context,
                           raw_ostream *Verbose) {
  SmallVector<char, 1024> Buffer;

  // Fill Buffer with corresponding bitcode records from Filename.
  if (std::error_code EC =
      readNaClRecordTextAndBuildBitcode(Filename, Buffer, Verbose))
    return EC;

  // Parse buffer as ordinary binary bitcode file.
  StringRef BitcodeBuffer(Buffer.data(),  Buffer.size());
  MemoryBufferRef MemBufRef(BitcodeBuffer, Filename);
  return NaClParseBitcodeFile(MemBufRef, Context, Verbose);
}
