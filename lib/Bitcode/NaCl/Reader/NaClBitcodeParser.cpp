//===- NaClBitcodeParser.cpp ----------------------------------------------===//
//     Low-level bitcode driver to parse PNaCl bitcode files.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "NaClBitcodeParser"

#include "llvm/Bitcode/NaCl/NaClBitcodeParser.h"
#include "llvm/Support/Debug.h"

void NaClBitcodeRecord::Print(raw_ostream& os) const {
  DEBUG(os << "Block " << GetBlockID() << ", Code " << Code
        << ", EntryID " << Entry.ID << ", <";
        for (unsigned i = 0, e = Values.size(); i != e; ++i) {
          if (i > 0) os << " ";
          os << Values[i];
        }
        os << ">");
}

NaClBitcodeParser::~NaClBitcodeParser() {}

bool NaClBitcodeParser::Parse() {
  Record.ReadEntry();

  if (Record.GetEntryKind() != NaClBitstreamEntry::SubBlock)
    return Error("Expected block, but not found");

  return ParseBlock(Record.GetEntryID());
}

bool NaClBitcodeParser::ParseThisBlock() {
  if (GetBlockID() == naclbitc::BLOCKINFO_BLOCK_ID) {
    // BLOCKINFO is a special part of the stream. Let the bitstream
    // reader process this block.
    //
    // TODO(kschimpf): Move this out of the bitstream reader, so that
    // we have simplier API's for this class.
    EnterBlockInfo();
    if (Record.GetCursor().ReadBlockInfoBlock())
      return Error("Malformed BlockInfoBlock");
    RemoveBlockBitsFromEnclosingBlock();
    ExitBlockInfo();
    return false;
  }

  // Regular block. Enter subblock.
  unsigned NumWords;
  if (Record.GetCursor().EnterSubBlock(GetBlockID(), &NumWords)) {
    return Error("Malformed block record");
  }

  EnterBlock(NumWords);

  // Process records.
  while (1) {
    if (Record.GetCursor().AtEndOfStream())
      return Error("Premature end of bitstream");

    // Read entry defining type of entry.
    Record.ReadEntry();

    switch (Record.GetEntryKind()) {
    case NaClBitstreamEntry::Error:
      return Error("malformed bitcode file");
    case NaClBitstreamEntry::EndBlock: {
      ExitBlock();
      RemoveBlockBitsFromEnclosingBlock();
      return false;
    }
    case NaClBitstreamEntry::SubBlock: {
      if (ParseBlock(Record.GetEntryID())) return true;
      break;
    }
    case NaClBitstreamEntry::Record:
      // The interesting case.
      if (Record.GetEntryID() == naclbitc::DEFINE_ABBREV) {
        //Process any block-local abbreviation definitions.
        Record.GetCursor().ReadAbbrevRecord();
        ProcessRecordAbbrev();
      } else {
        // Read in a record.
        Record.ReadValues();
        ProcessRecord();
      }
      break;
    }
  }
  return false;
}
