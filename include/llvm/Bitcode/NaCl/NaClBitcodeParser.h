//===- NaClBitcodeParser.h -----------------------------------*- C++ -*-===//
//     Low-level bitcode driver to parse PNaCl bitcode files.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Parses and processes low-level PNaCl bitcode files. Defines class
// NaClBitcodeParser.
//
// The concepts of PNaCl bitcode files are basically the same as for
// LLVM bitcode files (see http://llvm.org/docs/BitCodeFormat.html for
// details).
//
// The bitstream format is an abstract encoding of structured data,
// very similar to XML in some ways. Like XML, bitstream files contain
// tags, and nested structures, and you can parse the file without
// having to understand the tags. Unlike XML, the bitstream format is
// a binary encoding, and provides a mechanism for the file to
// self-describe "abbreviations".  Abbreviations are effectively size
// optimizations for the content.
//
// The bitcode file is conceptually a sequence of "blocks", defining
// the content. Blocks contain a sequence of records and
// blocks. Nested content is defined using nested blocks.  A (data)
// "record" is a tag, and a vector of (unsigned integer) values.
//
// Blocks are identified using Block IDs. Each kind of block has a
// unique block "ID". Records have two elements:
//
//   a) A "code" identifying what type of record it is.
//   b) A vector of "values" defining the contents of the record.
//
// The bitstream "reader" (defined in NaClBitstreamReader.h) defines
// the implementation that converts the low-level bit file into
// records and blocks. The bit stream is processed by moving a
// "cursor" over the sequence of bits.
//
// The bitstream reader assumes that each block/record is read in by
// first reading the "entry". The entry defines whether it corresponds
// to one of the following:
//
//    a) At the beginning of a (possibly nested) block
//    b) At the end of the current block.
//    c) The input defines an abberviation.
//    d) The input defines a record.
//
// An entry contains two values, a "kind" and an "ID". The kind
// defines which of the four cases above occurs. The ID provides
// identifying information on how to further process the input. For
// case (a), the ID is the identifier associated with the the block
// being processed. For case (b) and (c) the ID is ignored. For case
// (d) the ID identifies the abbreviation that should be used to parse
// the values.
//
// The class NaClBitcodeParser defines a bitcode parser that extracts
// the blocks and records, which are then processed using virtual
// callbacks. In general, you will want to implement derived classes
// for each type of block, so that the corresponding data is processed
// appropriately.
//
// The class NaClBitcodeParser parses a bitcode block, and defines a
// set of callbacks for that block, including:
//
//    a) EnterBlock: What to do once we have entered the block.
//    b) ProcessRecord: What to do with each parsed record.
//    c) ProcessAbbrevRecord: What to do with a parsed abbreviation.
//    d) ParseBlock: Parse the (nested) block with the given ID.
//    e) ExitBlock: What to do once we have finished processing the block.
//
// Note that a separate instance of NaClBitcodeParser (or a
// corresponding derived class) is created for each nested block. Each
// instance is responsible for only parsing a single block. Method
// ParseBlock creates new instances to parse nested blocks. Method
// GetEnclosingParser() can be used to refer to the parser associated
// with the enclosing block.
//
// TODO(kschimpf): Define an intermediate derived class of
// NaClBitcodeParser that defines callbacks based on the actual
// structure of PNaCl bitcode files.  That is, it has callbacks for
// each of the types of blocks (i.e. module, types, global variables,
// function, symbol tables etc). This derivied class can then be used
// as the base class for the bitcode reader.
//
// TODO(kschimpf): Currently, the processing of abbreviations is
// handled by the PNaCl bitstream reader, rather than by the
// parser. Hence, we currently require defining methods
// EnterBlockInfo, ExitBlockInfo, and ProcessRecordAbbrev. BlockInfo
// is a special block that defines abbreviations to be applied to all
// blocks. Record abbreviations (which are a special kind of record)
// define abbreviations for a the current block.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_BITCODE_NACL_NACLBITCODEPARSER_H
#define LLVM_BITCODE_NACL_NACLBITCODEPARSER_H

#include "llvm/Bitcode/NaCl/NaClBitstreamReader.h"
#include "llvm/Support/raw_ostream.h"
#include <vector>

using namespace llvm;

class NaClBitcodeParser;

/// Defines the data associated with reading a block record in the
/// PNaCl bitcode stream.
class NaClBitcodeRecord {
public:
  /// Type for vector of values representing a record.
  typedef SmallVector<uint64_t, 64> RecordVector;

  NaClBitcodeRecord(unsigned BlockID, NaClBitstreamCursor &Cursor)
      : BlockID(BlockID),
        Cursor(Cursor),
        StartBit(Cursor.GetCurrentBitNo()) {
  }

  /// Print the contents out to the given stream (for debugging).
  void Print(raw_ostream& os) const;

  /// Returns the bitstream reader being used.
  NaClBitstreamReader &GetReader() const {
    return *Cursor.getBitStreamReader();
  }

  /// Returns the cursor position within the bitstream.
  NaClBitstreamCursor &GetCursor() const {
    return Cursor;
  }

  /// Returns the block ID of the record.
  unsigned GetBlockID() const {
    return BlockID;
  }

  /// Returns the kind of entry read from the input stream.
  unsigned GetEntryKind() const {
    return Entry.Kind;
  }

  /// Returns the code value (i.e. selector) associated with the
  /// record.
  unsigned GetCode() const {
    return Code;
  }

  /// Returns the EntryID (e.g. abbreviation if !=
  /// naclbitcod::UNABBREV_RECORD) associated with the record. Note:
  /// for block-enter, block-exit, and define-abbreviation, EntryID is
  /// not the corresponding abbreviation.
  unsigned GetEntryID() const {
    return Entry.ID;
  }

  /// Returns the (value) record associated with the read record.
  const RecordVector &GetValues() const {
    return Values;
  }

  /// Returns the number of bits in this record.
  unsigned GetNumBits() const {
    return GetCursor().GetCurrentBitNo() - StartBit;
  }

protected:
  // The block ID associated with this record.
  unsigned BlockID;
  // The bitstream cursor defining location within the bitcode file.
  NaClBitstreamCursor &Cursor;
  // The entry ID associated with the record.
  unsigned EntryID;
  // The selector code associated with the record.
  unsigned Code;
  // The sequence of values defining the parsed record.
  RecordVector Values;
  // The entry (i.e. value(s) preceding the record that define what
  // value comes next).
  NaClBitstreamEntry Entry;
  // Start bit for the record.
  uint64_t StartBit;

  /// Returns the position of the start bit for this record.
  unsigned GetStartBit() const {
    return StartBit;
  }

private:
  // Allows class NaClBitcodeParser to read values into the
  // record, thereby hiding the details of how to read values.
  friend class NaClBitcodeParser;

  /// Read bitstream entry. Defines what construct appears next in the
  /// bitstream.
  void ReadEntry() {
    StartBit = GetCursor().GetCurrentBitNo();
    Entry = GetCursor().advance(NaClBitstreamCursor::AF_DontAutoprocessAbbrevs);
  }

  /// Reads in a record's values, if the entry defines a record (Must
  /// be called after ReadEntry).
  void ReadValues() {
    Values.clear();
    Code = GetCursor().readRecord(Entry.ID, Values);
  }

  NaClBitcodeRecord(const NaClBitcodeRecord &Rcd) LLVM_DELETED_FUNCTION;
  void operator=(const NaClBitcodeRecord &Rcd) LLVM_DELETED_FUNCTION;
};

/// Parses a block in the PNaCL bitcode stream.
class NaClBitcodeParser {
public:

  // Creates a parser to parse the the block at the given cursor in
  // the PNaCl bitcode stream. This instance is a "dummy" instance
  // that starts the parser.
  explicit NaClBitcodeParser(NaClBitstreamCursor &Cursor)
      : EnclosingParser(0),
        Record(ILLEGAL_BLOCK_ID, Cursor),
        StartBit(Cursor.GetCurrentBitNo()) {
    BlockStart = StartBit;
  }

  virtual ~NaClBitcodeParser();

  /// Reads the (top-level) block associated with the given block
  /// record at the stream cursor. Returns true if unable to parse.
  /// Can be called multiple times to parse multiple blocks.
  bool Parse();

  // Called once the bitstream reader has entered the corresponding
  // subblock.  Argument NumWords is set to the number of words in the
  // corresponding subblock.
  virtual void EnterBlock(unsigned NumWords) {}

  // Called when the corresponding EndBlock of the block being parsed
  // is found.
  virtual void ExitBlock() {}

  // Called before a BlockInfo block is parsed. Note: BlockInfo blocks
  // are special. They include abbreviations to be used for blocks.
  // After this routine is called, the NaClBitstreamParser is called
  // to parse the BlockInfo block (rather than making a call to
  // Parser->Parse()).
  virtual void EnterBlockInfo() {}

  // Called after a BlockInfo block is parsed.
  virtual void ExitBlockInfo() { ExitBlock(); }

  // Called after each record (within the block) is read (into field Record).
  virtual void ProcessRecord() {}

  // Called if a block-specific abbreviation is read (into field
  // Record), after processing by the bitstream reader.
  virtual void ProcessRecordAbbrev() {}

  // Creates an instance of the NaClBitcodeParser to use to parse the
  // block with the given block ID, and then call's method
  // ParseThisBlock() to parse the corresponding block. Note:
  // Each derived class should define it's own version of this
  // method, following the pattern below.
  virtual bool ParseBlock(unsigned BlockID) {
    // Default implementation just builds a parser that does nothing.
    NaClBitcodeParser Parser(BlockID, this);
    return Parser.ParseThisBlock();
  }

  // Called when error occurs. Message is the error to report. Always
  // returns true (the error return value of Parse).
  virtual bool Error(const std::string Message) {
    errs() << "Error: " << Message << "\n";
    return true;
  }

  // Returns the number of bits in this block.
  unsigned GetNumBits() {
    return Record.GetCursor().GetCurrentBitNo() - StartBit;
  }

  // Returns the number of bits in this block, but not subblocks
  // within this block.
  unsigned GetLocalNumBits() {
    return Record.GetCursor().GetCurrentBitNo() - BlockStart;
  }

  /// Returns the block ID associated with the Parser.
  unsigned GetBlockID() {
    return Record.GetBlockID();
  }

  /// Returns the enclosing block parser of this block.
  NaClBitcodeParser *GetEnclosingParser() const {
    // Note: The top-level parser instance is a dummy instance
    // and is not considered an enclosing parser.
    return EnclosingParser->EnclosingParser ? EnclosingParser : 0;
  }

protected:
  // The containing parser.
  NaClBitcodeParser *EnclosingParser;

  // The current record (within the block) being processed.
  NaClBitcodeRecord Record;

  // Creates a block parser to parse the block associated with the
  // bitcode entry that defines the beginning of a block. This
  // instance actually parses the corresponding block.
  NaClBitcodeParser(unsigned BlockID,
                    NaClBitcodeParser *EnclosingParser)
      : EnclosingParser(EnclosingParser),
        Record(BlockID, EnclosingParser->Record.GetCursor()),
        StartBit(EnclosingParser->Record.GetStartBit()) {
    BlockStart = StartBit;
  }

  // Parses the block using the parser defined by
  // ParseBlock(unsigned).  Returns true if unable to parse the
  // block. Note: Should only be called by virtual
  // ParseBlock(unsigned).
  bool ParseThisBlock();

private:
  // Special constant identifying the top-level instance.
  static const unsigned ILLEGAL_BLOCK_ID = UINT_MAX;

  // The start bit of the block.
  unsigned StartBit;
  // The start bit of the block, plus the bits in all subblocks.  Used
  // to compute the number of (block local) bits.
  unsigned BlockStart;

  // Updates BlockStart in the enclosingblock, so that bits in this
  // block are not counted as local bits for the enclosing block.
  void RemoveBlockBitsFromEnclosingBlock() {
    EnclosingParser->BlockStart += GetNumBits();
  }

  void operator=(const NaClBitcodeParser &Parser) LLVM_DELETED_FUNCTION;
  NaClBitcodeParser(const NaClBitcodeParser &Parser) LLVM_DELETED_FUNCTION;
};


#endif
