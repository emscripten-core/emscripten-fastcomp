//===- NaClBitstreamReader.cpp --------------------------------------------===//
//     NaClBitstreamReader implementation
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/NaCl/NaClBitstreamReader.h"

using namespace llvm;

//===----------------------------------------------------------------------===//
//  NaClBitstreamCursor implementation
//===----------------------------------------------------------------------===//

void NaClBitstreamCursor::operator=(const NaClBitstreamCursor &RHS) {
  freeState();

  BitStream = RHS.BitStream;
  NextChar = RHS.NextChar;
  CurWord = RHS.CurWord;
  BitsInCurWord = RHS.BitsInCurWord;
  CurCodeSize = RHS.CurCodeSize;

  // Copy abbreviations, and bump ref counts.
  CurAbbrevs = RHS.CurAbbrevs;
  for (size_t i = 0, e = CurAbbrevs.size(); i != e; ++i)
    CurAbbrevs[i]->addRef();

  // Copy block scope and bump ref counts.
  BlockScope = RHS.BlockScope;
  for (size_t S = 0, e = BlockScope.size(); S != e; ++S) {
    std::vector<NaClBitCodeAbbrev*> &Abbrevs = BlockScope[S].PrevAbbrevs;
    for (size_t i = 0, e = Abbrevs.size(); i != e; ++i)
      Abbrevs[i]->addRef();
  }
}

void NaClBitstreamCursor::freeState() {
  // Free all the Abbrevs.
  for (size_t i = 0, e = CurAbbrevs.size(); i != e; ++i)
    CurAbbrevs[i]->dropRef();
  CurAbbrevs.clear();

  // Free all the Abbrevs in the block scope.
  for (size_t S = 0, e = BlockScope.size(); S != e; ++S) {
    std::vector<NaClBitCodeAbbrev*> &Abbrevs = BlockScope[S].PrevAbbrevs;
    for (size_t i = 0, e = Abbrevs.size(); i != e; ++i)
      Abbrevs[i]->dropRef();
  }
  BlockScope.clear();
}

/// EnterSubBlock - Having read the ENTER_SUBBLOCK abbrevid, enter
/// the block, and return true if the block has an error.
bool NaClBitstreamCursor::EnterSubBlock(unsigned BlockID, unsigned *NumWordsP) {
  // Save the current block's state on BlockScope.
  BlockScope.push_back(Block(CurCodeSize));
  BlockScope.back().PrevAbbrevs.swap(CurAbbrevs);

  // Add the abbrevs specific to this block to the CurAbbrevs list.
  if (const NaClBitstreamReader::BlockInfo *Info =
      BitStream->getBlockInfo(BlockID)) {
    for (size_t i = 0, e = Info->Abbrevs.size(); i != e; ++i) {
      CurAbbrevs.push_back(Info->Abbrevs[i]);
      CurAbbrevs.back()->addRef();
    }
  }

  // Get the codesize of this block.
  CurCodeSize.IsFixed = true;
  CurCodeSize.NumBits = ReadVBR(naclbitc::CodeLenWidth);
  SkipToFourByteBoundary();
  unsigned NumWords = Read(naclbitc::BlockSizeWidth);
  if (NumWordsP) *NumWordsP = NumWords;

  // Validate that this block is sane.
  if (CurCodeSize.NumBits == 0 || AtEndOfStream())
    return true;

  return false;
}

void NaClBitstreamCursor::readAbbreviatedLiteral(
    const NaClBitCodeAbbrevOp &Op,
    SmallVectorImpl<uint64_t> &Vals) {
  assert(Op.isLiteral() && "Not a literal");
  // If the abbrev specifies the literal value to use, use it.
  Vals.push_back(Op.getLiteralValue());
}

void NaClBitstreamCursor::readAbbreviatedField(
    const NaClBitCodeAbbrevOp &Op,
    SmallVectorImpl<uint64_t> &Vals) {
  assert(!Op.isLiteral() && "Use ReadAbbreviatedLiteral for literals!");

  // Decode the value as we are commanded.
  switch (Op.getEncoding()) {
  default:
    report_fatal_error("Should not reach here");
  case NaClBitCodeAbbrevOp::Fixed:
    Vals.push_back(Read((unsigned)Op.getEncodingData()));
    break;
  case NaClBitCodeAbbrevOp::VBR:
    Vals.push_back(ReadVBR64((unsigned)Op.getEncodingData()));
    break;
  case NaClBitCodeAbbrevOp::Char6:
    Vals.push_back(NaClBitCodeAbbrevOp::DecodeChar6(Read(6)));
    break;
  }
}

void NaClBitstreamCursor::skipAbbreviatedField(const NaClBitCodeAbbrevOp &Op) {
  assert(!Op.isLiteral() && "Use ReadAbbreviatedLiteral for literals!");

  // Decode the value as we are commanded.
  switch (Op.getEncoding()) {
  default:
    report_fatal_error("Should not reach here");
  case NaClBitCodeAbbrevOp::Fixed:
    (void)Read((unsigned)Op.getEncodingData());
    break;
  case NaClBitCodeAbbrevOp::VBR:
    (void)ReadVBR64((unsigned)Op.getEncodingData());
    break;
  case NaClBitCodeAbbrevOp::Char6:
    (void)Read(6);
    break;
  }
}



/// skipRecord - Read the current record and discard it.
void NaClBitstreamCursor::skipRecord(unsigned AbbrevID) {
  // Skip unabbreviated records by reading past their entries.
  if (AbbrevID == naclbitc::UNABBREV_RECORD) {
    unsigned Code = ReadVBR(6);
    (void)Code;
    unsigned NumElts = ReadVBR(6);
    for (unsigned i = 0; i != NumElts; ++i)
      (void)ReadVBR64(6);
    return;
  }

  const NaClBitCodeAbbrev *Abbv = getAbbrev(AbbrevID);

  for (unsigned i = 0, e = Abbv->getNumOperandInfos(); i != e; ++i) {
    const NaClBitCodeAbbrevOp &Op = Abbv->getOperandInfo(i);
    if (Op.isLiteral())
      continue;

    if (Op.getEncoding() == NaClBitCodeAbbrevOp::Blob)
      report_fatal_error("Should not reach here");

    if (Op.getEncoding() != NaClBitCodeAbbrevOp::Array) {
      skipAbbreviatedField(Op);
      continue;
    }

    if (Op.getEncoding() == NaClBitCodeAbbrevOp::Array) {
      // Array case.  Read the number of elements as a vbr6.
      unsigned NumElts = ReadVBR(6);

      // Get the element encoding.
      assert(i+2 == e && "array op not second to last?");
      const NaClBitCodeAbbrevOp &EltEnc = Abbv->getOperandInfo(++i);

      // Read all the elements.
      for (; NumElts; --NumElts)
        skipAbbreviatedField(EltEnc);
      continue;
    }
  }
}

unsigned NaClBitstreamCursor::readRecord(unsigned AbbrevID,
                                         SmallVectorImpl<uint64_t> &Vals) {
  if (AbbrevID == naclbitc::UNABBREV_RECORD) {
    unsigned Code = ReadVBR(6);
    unsigned NumElts = ReadVBR(6);
    for (unsigned i = 0; i != NumElts; ++i)
      Vals.push_back(ReadVBR64(6));
    return Code;
  }

  const NaClBitCodeAbbrev *Abbv = getAbbrev(AbbrevID);

  for (unsigned i = 0, e = Abbv->getNumOperandInfos(); i != e; ++i) {
    const NaClBitCodeAbbrevOp &Op = Abbv->getOperandInfo(i);
    if (Op.isLiteral()) {
      readAbbreviatedLiteral(Op, Vals);
      continue;
    }

    if (Op.getEncoding() == NaClBitCodeAbbrevOp::Blob)
      report_fatal_error("Should not reach here");

    if (Op.getEncoding() != NaClBitCodeAbbrevOp::Array) {
      readAbbreviatedField(Op, Vals);
      continue;
    }

    if (Op.getEncoding() == NaClBitCodeAbbrevOp::Array) {
      // Array case.  Read the number of elements as a vbr6.
      unsigned NumElts = ReadVBR(6);

      // Get the element encoding.
      assert(i+2 == e && "array op not second to last?");
      const NaClBitCodeAbbrevOp &EltEnc = Abbv->getOperandInfo(++i);

      // Read all the elements.
      for (; NumElts; --NumElts)
        readAbbreviatedField(EltEnc, Vals);
      continue;
    }
  }

  unsigned Code = (unsigned)Vals[0];
  Vals.erase(Vals.begin());
  return Code;
}


void NaClBitstreamCursor::ReadAbbrevRecord() {
  NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
  unsigned NumOpInfo = ReadVBR(5);
  for (unsigned i = 0; i != NumOpInfo; ++i) {
    bool IsLiteral = Read(1) ? true : false;
    if (IsLiteral) {
      Abbv->Add(NaClBitCodeAbbrevOp(ReadVBR64(8)));
      continue;
    }

    NaClBitCodeAbbrevOp::Encoding E = (NaClBitCodeAbbrevOp::Encoding)Read(3);
    if (NaClBitCodeAbbrevOp::hasEncodingData(E)) {
      unsigned Data = ReadVBR64(5);

      // As a special case, handle fixed(0) (i.e., a fixed field with zero bits)
      // and vbr(0) as a literal zero.  This is decoded the same way, and avoids
      // a slow path in Read() to have to handle reading zero bits.
      if ((E == NaClBitCodeAbbrevOp::Fixed || E == NaClBitCodeAbbrevOp::VBR) &&
          Data == 0) {
        Abbv->Add(NaClBitCodeAbbrevOp(0));
        continue;
      }
      
      Abbv->Add(NaClBitCodeAbbrevOp(E, Data));
    } else
      Abbv->Add(NaClBitCodeAbbrevOp(E));
  }
  CurAbbrevs.push_back(Abbv);
}

bool NaClBitstreamCursor::ReadBlockInfoBlock() {
  // If this is the second stream to get to the block info block, skip it.
  if (BitStream->hasBlockInfoRecords())
    return SkipBlock();

  if (EnterSubBlock(naclbitc::BLOCKINFO_BLOCK_ID)) return true;

  SmallVector<uint64_t, 64> Record;
  NaClBitstreamReader::BlockInfo *CurBlockInfo = 0;

  // Read all the records for this module.
  while (1) {
    NaClBitstreamEntry Entry = advanceSkippingSubblocks(AF_DontAutoprocessAbbrevs);

    switch (Entry.Kind) {
    case llvm::NaClBitstreamEntry::SubBlock: // Handled for us already.
    case llvm::NaClBitstreamEntry::Error:
      return true;
    case llvm::NaClBitstreamEntry::EndBlock:
      return false;
    case llvm::NaClBitstreamEntry::Record:
      // The interesting case.
      break;
    }

    // Read abbrev records, associate them with CurBID.
    if (Entry.ID == naclbitc::DEFINE_ABBREV) {
      if (!CurBlockInfo) return true;
      ReadAbbrevRecord();

      // ReadAbbrevRecord installs the abbrev in CurAbbrevs.  Move it to the
      // appropriate BlockInfo.
      NaClBitCodeAbbrev *Abbv = CurAbbrevs.back();
      CurAbbrevs.pop_back();
      CurBlockInfo->Abbrevs.push_back(Abbv);
      continue;
    }

    // Read a record.
    Record.clear();
    switch (readRecord(Entry.ID, Record)) {
      default: break;  // Default behavior, ignore unknown content.
      case naclbitc::BLOCKINFO_CODE_SETBID:
        if (Record.size() < 1) return true;
        CurBlockInfo = &BitStream->getOrCreateBlockInfo((unsigned)Record[0]);
        break;
    }
  }
}
