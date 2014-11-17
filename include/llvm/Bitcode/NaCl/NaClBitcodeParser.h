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

namespace llvm {

class NaClBitcodeRecord;
class NaClBitcodeParser;

// Defines the base class for data extracted from the input bitstream
// (i.e blocks and records).
class NaClBitcodeData {
  void operator=(const NaClBitcodeData&) LLVM_DELETED_FUNCTION;

public:
  /// Create data element to be read from input cursor.
  explicit NaClBitcodeData(NaClBitstreamCursor &Cursor)
      : Cursor(Cursor), StartBit(Cursor.GetCurrentBitNo())
  {}

  /// Create copy of the given data element.
  explicit NaClBitcodeData(const NaClBitcodeData &Data)
      : Cursor(Data.Cursor), StartBit(Data.StartBit)
  {}

  /// Returns the bitstream reader being used.
  NaClBitstreamReader &GetReader() const {
    return *Cursor.getBitStreamReader();
  }

  /// Returns the cursor position within the bitstream.
  NaClBitstreamCursor &GetCursor() const {
    return Cursor;
  }

  /// Returns the number of bits defined by the data.
  unsigned GetNumBits() const {
    return GetCursor().GetCurrentBitNo() - StartBit;
  }

  /// Returns the first bit of the stream data.
  unsigned GetStartBit() const {
    return StartBit;
  }

protected:
  /// Change the start bit for the data to the new value.
  void SetStartBit(uint64_t NewValue) {
    StartBit = NewValue;
  }

private:
  // The bitstream cursor defining location within the bitcode file.
  NaClBitstreamCursor &Cursor;

  // Start bit for the record.
  uint64_t StartBit;
};

/// Models the block defined by a (begin) block record, through the
/// (end) block record.
class NaClBitcodeBlock : public NaClBitcodeData {
  NaClBitcodeBlock(const NaClBitcodeBlock &) LLVM_DELETED_FUNCTION;
  void operator=(const NaClBitcodeBlock &) LLVM_DELETED_FUNCTION;

public:
  /// Given the found (begin) block record for block BlockID, create
  /// the corresponding data associated with that block.
  NaClBitcodeBlock(unsigned BlockID, const NaClBitcodeRecord &Record);

  /// Create block data for block BlockID, using the input cursor.
  NaClBitcodeBlock(unsigned BlockID, NaClBitstreamCursor &Cursor)
      : NaClBitcodeData(Cursor),
        BlockID(BlockID),
        EnclosingBlock(0)
  {
    LocalStartBit = GetStartBit();
  }

  /// Print the contents out to the given stream.
  void Print(raw_ostream& os) const;

  /// Returns pointer to the enclosing block.
  const NaClBitcodeBlock *GetEnclosingBlock() const {
    return EnclosingBlock;
  }

  /// Returns the block ID of the block.
  unsigned GetBlockID() const {
    return BlockID;
  }

  /// Returns the number of bits in the block associated with the
  /// bitcode parser parsing this block, excluding nested blocks.
  unsigned GetLocalNumBits() const {
    return GetCursor().GetCurrentBitNo() - LocalStartBit;
  }

protected:
  // The block ID associated with this record.
  unsigned BlockID;
  // The enclosing block, if defined.
  const NaClBitcodeBlock *EnclosingBlock;
  // Start bit for the block, updated to skip nested blocks.
  uint64_t LocalStartBit;

  // Note: We friend class NaClBitcodeParser, so that it can
  // update field LocalStartBit.
  friend class NaClBitcodeParser;
};

/// Defines the data associated with reading a block record in the
/// PNaCl bitcode stream.
class NaClBitcodeRecord : public NaClBitcodeData {
public:
  /// Type for vector of values representing a record.
  typedef SmallVector<uint64_t, 8> RecordVector;

  /// Creates a bitcode record, starting at the position defined
  /// by cursor.
  explicit NaClBitcodeRecord(const NaClBitcodeBlock &Block)
      : NaClBitcodeData(Block.GetCursor()),
        Block(Block)
  {}

  /// Print the contents out to the given stream.
  void Print(raw_ostream& os) const;

  /// The block the record appears in.
  const NaClBitcodeBlock &GetBlock() const {
    return Block;
  }

  /// Returns the block ID associated with the record.
  unsigned GetBlockID() const {
    return Block.GetBlockID();
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
  /// naclbitc::UNABBREV_RECORD) associated with the record. Note:
  /// for block-enter, block-exit, and define-abbreviation, EntryID is
  /// not the corresponding abbreviation.
  unsigned GetEntryID() const {
    return Entry.ID;
  }

  /// Returns the (value) record associated with the read record.
  const RecordVector &GetValues() const {
    return Values;
  }

  /// Returns true if the record was read using an abbreviation.
  bool UsedAnAbbreviation() const {
    return GetEntryKind() == NaClBitstreamEntry::Record &&
        GetEntryID() != naclbitc::UNABBREV_RECORD;
  }

  /// Returns the abbrevation index used to read the record.
  /// Returns naclbitc::UNABBREV_RECORD if not applicable.
  unsigned GetAbbreviationIndex() const {
    return UsedAnAbbreviation()
        ? GetEntryID() : static_cast<unsigned>(naclbitc::UNABBREV_RECORD);
  }

  /// Destructively change the abbreviation ID to the given value.
  void SetAbbreviationIndex(unsigned Index) {
    Entry.ID = Index;
  }

protected:
  // The block associated with the record.
  const NaClBitcodeBlock &Block;
  // The selector code associated with the record.
  unsigned Code;
  // The sequence of values defining the parsed record.
  RecordVector Values;
  // The entry (i.e. value(s) preceding the record that define what
  // value comes next).
  NaClBitstreamEntry Entry;

private:
  // Allows class NaClBitcodeParser to read values into the
  // record, thereby hiding the details of how to read values.
  friend class NaClBitcodeParser;

  /// Read bitstream entry. Defines what construct appears next in the
  /// bitstream.
  void ReadEntry() {
    SetStartBit(GetCursor().GetCurrentBitNo());
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
        Block(ILLEGAL_BLOCK_ID, Cursor),
        Record(Block)
  {}

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
  virtual bool Error(const std::string &Message) {
    errs() << "Error: " << Message << "\n";
    return true;
  }

  // Returns the number of bits in this block, including nested blocks.
  unsigned GetBlockNumBits() const {
    return Block.GetNumBits();
  }

  // Returns the number of bits in this block, excluding nested blocks.
  unsigned GetBlockLocalNumBits() const {
    return Block.GetLocalNumBits();
  }

  /// Returns the block ID associated with the Parser.
  unsigned GetBlockID() const {
    return Block.GetBlockID();
  }

  NaClBitcodeBlock &GetBlock() {
    return Block;
  }

  /// Returns the enclosing parser of this block.
  NaClBitcodeParser *GetEnclosingParser() const {
    // Note: The top-level parser instance is a dummy instance
    // and is not considered an enclosing parser.
    return EnclosingParser->EnclosingParser ? EnclosingParser : 0;
  }

  // Parses the block using the parser defined by
  // ParseBlock(unsigned).  Returns true if unable to parse the
  // block. Note: Should only be called by virtual
  // ParseBlock(unsigned).
  bool ParseThisBlock();

protected:
  // The containing parser.
  NaClBitcodeParser *EnclosingParser;

  // The block the parser is associated with.
  NaClBitcodeBlock Block;

  // The current record (within the block) being processed.
  NaClBitcodeRecord Record;

  // Creates a block parser to parse the block associated with the
  // bitcode entry that defines the beginning of a block. This
  // instance actually parses the corresponding block.
  NaClBitcodeParser(unsigned BlockID, NaClBitcodeParser *EnclosingParser)
      : EnclosingParser(EnclosingParser),
        Block(BlockID, EnclosingParser->Record),
        Record(Block)
  {}

private:
  // Special constant identifying the top-level instance.
  static const unsigned ILLEGAL_BLOCK_ID = UINT_MAX;

  void operator=(const NaClBitcodeParser &Parser) LLVM_DELETED_FUNCTION;
  NaClBitcodeParser(const NaClBitcodeParser &Parser) LLVM_DELETED_FUNCTION;
};

}  // namespace llvm

#endif
