//===- NaClBitcodeMungeReader.cpp - Read bitcode record list ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Implements bitcode reader for NaClBitcodeRecordList and NaClMungedBitcode.

#include "llvm/Bitcode/NaCl/NaClBitcodeMungeUtils.h"

#include "llvm/Bitcode/NaCl/NaClBitcodeHeader.h"

using namespace llvm;

namespace {

class BitcodeParser;

// \brief The state associated with parsing a bitcode buffer.
class BitcodeParseState {
  BitcodeParseState(const BitcodeParseState&) = delete;
  BitcodeParseState &operator=(const BitcodeParseState&) = delete;
public:
  // \brief Construct the bitcode parse state.
  //
  // \param Parser The parser used to parse the bitcode.
  // \param[out] Records Filled with parsed records.
  BitcodeParseState(BitcodeParser *Parser,
                    NaClBitcodeRecordList &Records);

  // List to read records into.
  NaClBitcodeRecordList &Records;
  // Listener used to get abbreviations as they are read.
  NaClBitcodeParserListener AbbrevListener;
};

// \brief The bitcode parser to extract bitcode records.
class BitcodeParser : public NaClBitcodeParser {
  BitcodeParser(const BitcodeParser &) = delete;
  BitcodeParser &operator=(const BitcodeParser&) = delete;
public:
  // \brief Top-level constructor for a bitcode parser.
  //
  // \param Cursor The beginning position of the bitcode to parse.
  // \param[out] Records Filled with parsed records.
  BitcodeParser(NaClBitstreamCursor &Cursor,
                NaClBitcodeRecordList &Records)
      : NaClBitcodeParser(Cursor),
        State(new BitcodeParseState(this, Records)) {
    SetListener(&State->AbbrevListener);
  }

  ~BitcodeParser() override {
    if (EnclosingParser == nullptr)
      delete State;
  }

  bool ParseBlock(unsigned BlockID) override {
    BitcodeParser NestedParser(BlockID, this);
    return NestedParser.ParseThisBlock();
  }

  void EnterBlock(unsigned NumWords) override {
    NaClRecordVector Values;
    Values.push_back(GetBlockID());
    Values.push_back(Record.GetCursor().getAbbrevIDWidth());
    std::unique_ptr<NaClBitcodeAbbrevRecord> AbbrevRec(
        new NaClBitcodeAbbrevRecord(naclbitc::ENTER_SUBBLOCK,
                                    naclbitc::BLK_CODE_ENTER,
                                    Values));
    State->Records.push_back(std::move(AbbrevRec));
  }

  void ExitBlock() override {
    NaClRecordVector Values;
    std::unique_ptr<NaClBitcodeAbbrevRecord> AbbrevRec(
        new NaClBitcodeAbbrevRecord(naclbitc::END_BLOCK,
                                    naclbitc::BLK_CODE_EXIT,
                                    Values));
    State->Records.push_back(std::move(AbbrevRec));
  }

  void ProcessRecord() override {
    std::unique_ptr<NaClBitcodeAbbrevRecord> AbbrevRec(
        new NaClBitcodeAbbrevRecord(Record.GetAbbreviationIndex(),
                                    Record.GetCode(),
                                    Record.GetValues()));
    State->Records.push_back(std::move(AbbrevRec));
  }

  void SetBID() override {
    ProcessRecord();
  }

  void ProcessAbbreviation(unsigned BlockID,
                           NaClBitCodeAbbrev *Abbrev,
                           bool IsLocal) override {
    ProcessRecord();
  }

private:
  // \brief Nested constructor for blocks within the bitcode buffer.
  //
  // \param BlockID The identifying constant associated with the block.
  // \param EnclosingParser The bitcode parser parsing the enclosing block.
  BitcodeParser(unsigned BlockID, BitcodeParser *EnclosingParser)
      : NaClBitcodeParser(BlockID, EnclosingParser),
        State(EnclosingParser->State) {}

  // The state of the bitcode parser.
  BitcodeParseState* State;
};

BitcodeParseState::BitcodeParseState(BitcodeParser *Parser,
                                     NaClBitcodeRecordList &Records)
    : Records(Records), AbbrevListener(Parser) {}

} // end of anonymous namespace

void llvm::readNaClBitcodeRecordList(
    NaClBitcodeRecordList &RecordList,
    std::unique_ptr<MemoryBuffer> InputBuffer) {
  if (InputBuffer->getBufferSize() % 4 != 0)
    report_fatal_error(
        "Bitcode stream must be a multiple of 4 bytes in length");

  const unsigned char *BufPtr =
      (const unsigned char *) InputBuffer->getBufferStart();
  const unsigned char *EndBufPtr = BufPtr + InputBuffer->getBufferSize();

  // Read header and verify it is good.
  NaClBitcodeHeader Header;
  if (Header.Read(BufPtr, EndBufPtr))
    report_fatal_error("Invalid PNaCl bitcode header.\n");
  if (!Header.IsSupported())
    errs() << Header.Unsupported();
  if (!Header.IsReadable())
    report_fatal_error("Invalid PNaCl bitcode header.\n");

  NaClBitstreamReader Reader(BufPtr, EndBufPtr, Header);
  NaClBitstreamCursor Cursor(Reader);

  // Parse the bitcode buffer.
  BitcodeParser Parser(Cursor, RecordList);

  while (!Cursor.AtEndOfStream()) {
    if (Parser.Parse())
      report_fatal_error("Malformed records founds, unable to continue");
  }
}
