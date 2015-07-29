//===- NaClBitcodeTextWriter.cpp - Write textual bitcode ------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Implements writeNaClBitcodeRecordList(), which writes out bitcode records
// as text.
//
// Note that textual bitcode records do not contain a header,
// abbreviations, or a blockinfo block. Records are defined as a
// sequence of integers, separated by commas, and terminated with a
// semicolon.
//
// For readability, a newline is added after each record.
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/NaCl/NaClBitcodeMungeUtils.h"

using namespace llvm;

namespace {

class TextWriter {
  TextWriter(const TextWriter&) = delete;
  void operator=(const TextWriter&) = delete;
public:
  TextWriter(SmallVectorImpl<char> &Buffer, raw_ostream &ErrStream)
      : Buffer(Buffer), ErrStream(ErrStream), ValueStream(ValueBuffer) {}

  bool emitRecord(NaClBitcodeAbbrevRecord &Record);

private:
  // Buffer to write textual bitcode records into.
  SmallVectorImpl<char> &Buffer;
  // Stream for error messages.
  raw_ostream &ErrStream;
  // Selector for number of bits to use for abbreviations.
  NaClBitcodeSelectorAbbrev DefaultAbbrevSelector;
  // True iff in the blockinfo block.
  bool InBlockInfoBlock = false;
  // String stream to convert integers to strings.
  std::string ValueBuffer;
  raw_string_ostream ValueStream;

  void writeValue(uint64_t Value);

  void writeSeparator() {
    Buffer.push_back(',');
  }

  void writeTerminator() {
    Buffer.push_back(';');
    Buffer.push_back('\n');
  }
};

void TextWriter::writeValue(uint64_t Value) {
  ValueStream << Value;
  ValueStream.flush();
  for (auto ch : ValueBuffer)
    Buffer.push_back(ch);
  ValueBuffer.clear();
}

bool TextWriter::emitRecord(NaClBitcodeAbbrevRecord &Record) {
  size_t NumValues = Record.Values.size();
  switch (Record.Code) {
  case naclbitc::BLK_CODE_ENTER:
    // Be careful to remove all records in the blockinfo block.
    if (InBlockInfoBlock) {
      ErrStream << "Blocks not allowed within the blockinfo block\n";
      return false;
    }
    if (NumValues != 2) {
      ErrStream << "Block enter doesn't contain 2 values: Found: "
                << NumValues << "\n";
      return false;
    }
    if (Record.Values[0] == naclbitc::BLOCKINFO_BLOCK_ID) {
      InBlockInfoBlock = true;
      return true;
    }
    writeValue(Record.Code);
    writeSeparator();
    writeValue(Record.Values[0]);
    writeSeparator();
    // Note: Since the textual form of the bitcode doesn't have
    // abbreviations, we simplify the number of bits field
    // (ie. Values[1] within the record) with the default bit width.
    writeValue(DefaultAbbrevSelector.NumBits);
    writeTerminator();
    return true;
  case naclbitc::BLK_CODE_EXIT:
    if (InBlockInfoBlock) {
      InBlockInfoBlock = false;
      return true;
    }
    if (NumValues != 0) {
      ErrStream << "Block exit shouldn't have any values. Found: "
                << NumValues << "\n";
    }
    writeValue(Record.Code);
    writeTerminator();
    return true;
  case naclbitc::BLK_CODE_DEFINE_ABBREV:
  case naclbitc::BLK_CODE_HEADER:
    // These records are skipped in textual bitcode.
    return true;
  default: {
    // Don't write records within blockinfo blocks.
    if (InBlockInfoBlock) {
      if (Record.Code == naclbitc::BLOCKINFO_CODE_SETBID)
        return true;
      ErrStream << "Invalid record found in blockinfo block\n";
      return false;
    }
    writeValue(Record.Code);
    for (const auto Value : Record.Values) {
      writeSeparator();
      writeValue(Value);
    }
    writeTerminator();
    return true;
  }
  }
}

} // end of anonymous namespace

bool llvm::writeNaClBitcodeRecordList(NaClBitcodeRecordList &RecordList,
                                      SmallVectorImpl<char> &Buffer,
                                      raw_ostream &ErrStream) {
  TextWriter Writer(Buffer, ErrStream);
  for (const auto &Record : RecordList) {
    if (!Writer.emitRecord(*Record))
      return false;
  }
  return true;
}
