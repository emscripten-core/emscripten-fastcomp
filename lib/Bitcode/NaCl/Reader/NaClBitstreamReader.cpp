//===- NaClBitstreamReader.cpp --------------------------------------------===//
//     NaClBitstreamReader implementation
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/STLExtras.h"
#include "llvm/Bitcode/NaCl/NaClBitstreamReader.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {

static const char *ErrorLevelName[] = {
  "Warning",
  "Error",
  "Fatal"
};

} // End of anonymous namespace.

std::string llvm::naclbitc::getBitAddress(uint64_t Bit) {
  std::string Buffer;
  raw_string_ostream Stream(Buffer);
  Stream << (Bit / 8) << ":" << (Bit % 8);
  return Stream.str();
}

raw_ostream &llvm::naclbitc::ErrorAt(
    raw_ostream &Out, ErrorLevel Level, uint64_t BitPosition) {
  assert(Level < array_lengthof(::ErrorLevelName));
  return Out << ErrorLevelName[Level] << "("
             << naclbitc::getBitAddress(BitPosition) << "): ";
}

//===----------------------------------------------------------------------===//
//  NaClBitstreamCursor implementation
//===----------------------------------------------------------------------===//

void NaClBitstreamCursor::ErrorHandler::
Fatal(const std::string &ErrorMessage) const {
  // Default implementation is simply print message, and the bit where
  // the error occurred.
  std::string Buffer;
  raw_string_ostream StrBuf(Buffer);
  naclbitc::ErrorAt(StrBuf, naclbitc::Fatal, Cursor.GetCurrentBitNo())
      << ErrorMessage;
  report_fatal_error(StrBuf.str());
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

void NaClBitstreamCursor::reportInvalidAbbrevNumber(unsigned AbbrevNo) const {
  std::string Buffer;
  raw_string_ostream StrBuf(Buffer);
  StrBuf << "Invalid abbreviation # " << AbbrevNo << " defined for record";
  ErrHandler->Fatal(StrBuf.str());
}

void NaClBitstreamCursor::reportInvalidJumpToBit(uint64_t BitNo) const {
  std::string Buffer;
  raw_string_ostream StrBuf(Buffer);
  StrBuf << "Invalid jump to bit " << BitNo;
  ErrHandler->Fatal(StrBuf.str());
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

void NaClBitstreamCursor::skipAbbreviatedField(const NaClBitCodeAbbrevOp &Op) {
  // Decode the value as we are commanded.
  switch (Op.getEncoding()) {
  case NaClBitCodeAbbrevOp::Literal:
    // No read necessary for literal.
    break;
  case NaClBitCodeAbbrevOp::Fixed:
    (void)Read((unsigned)Op.getValue());
    break;
  case NaClBitCodeAbbrevOp::VBR:
    (void)ReadVBR64((unsigned)Op.getValue());
    break;
  case NaClBitCodeAbbrevOp::Array:
    // This can't happen because the abbreviation must be valid.
    llvm_unreachable("Bad array abbreviation encoding!");
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
    SkipToByteBoundaryIfAligned();
    return;
  }

  const NaClBitCodeAbbrev *Abbv = getAbbrev(AbbrevID);

  for (unsigned i = 0, e = Abbv->getNumOperandInfos(); i != e; ++i) {
    const NaClBitCodeAbbrevOp &Op = Abbv->getOperandInfo(i);
    switch (Op.getEncoding()) {
    default:
      skipAbbreviatedField(Op);
      break;
    case NaClBitCodeAbbrevOp::Literal:
      break;
    case NaClBitCodeAbbrevOp::Array: {
      // Array case.  Read the number of elements as a vbr6.
      unsigned NumElts = ReadVBR(6);

      // Get the element encoding.
      const NaClBitCodeAbbrevOp &EltEnc = Abbv->getOperandInfo(++i);

      // Read all the elements.
      for (; NumElts; --NumElts)
        skipAbbreviatedField(EltEnc);
      break;
    }
    }
  }
  SkipToByteBoundaryIfAligned();
}

bool NaClBitstreamCursor::readRecordAbbrevField(
    const NaClBitCodeAbbrevOp &Op, uint64_t &Value) {
  switch (Op.getEncoding()) {
  case NaClBitCodeAbbrevOp::Literal:
    Value = Op.getValue();
    break;
  case NaClBitCodeAbbrevOp::Array:
    // Returns number of elements in the array.
    Value = ReadVBR(6);
    return true;
  case NaClBitCodeAbbrevOp::Fixed:
    Value = Read((unsigned)Op.getValue());
    break;
  case NaClBitCodeAbbrevOp::VBR:
    Value = ReadVBR64((unsigned)Op.getValue());
    break;
  case NaClBitCodeAbbrevOp::Char6:
    Value = NaClBitCodeAbbrevOp::DecodeChar6(Read(6));
    break;
  }
  return false;
}

uint64_t NaClBitstreamCursor::readArrayAbbreviatedField(
    const NaClBitCodeAbbrevOp &Op) {
  // Decode the value as we are commanded.
  switch (Op.getEncoding()) {
  case NaClBitCodeAbbrevOp::Literal:
    return Op.getValue();
  case NaClBitCodeAbbrevOp::Fixed:
    return Read((unsigned)Op.getValue());
  case NaClBitCodeAbbrevOp::VBR:
    return ReadVBR64((unsigned)Op.getValue());
  case NaClBitCodeAbbrevOp::Array:
    // This can't happen because the abbreviation must be valid.
    llvm_unreachable("Bad array abbreviation encoding!");
    break;
  case NaClBitCodeAbbrevOp::Char6:
    return NaClBitCodeAbbrevOp::DecodeChar6(Read(6));
  }
}

void NaClBitstreamCursor::readArrayAbbrev(
    const NaClBitCodeAbbrevOp &Op, unsigned NumArrayElements,
    SmallVectorImpl<uint64_t> &Vals) {
  for (; NumArrayElements; --NumArrayElements) {
    Vals.push_back(readArrayAbbreviatedField(Op));
  }
}

unsigned NaClBitstreamCursor::readRecord(unsigned AbbrevID,
                                         SmallVectorImpl<uint64_t> &Vals) {
  if (AbbrevID == naclbitc::UNABBREV_RECORD) {
    unsigned Code = ReadVBR(6);
    unsigned NumElts = ReadVBR(6);
    for (unsigned i = 0; i != NumElts; ++i)
      Vals.push_back(ReadVBR64(6));
    SkipToByteBoundaryIfAligned();
    return Code;
  }

  // Read code.
  const NaClBitCodeAbbrev *Abbv = getAbbrev(AbbrevID);
  uint64_t Value;
  unsigned Code;
  if (readRecordAbbrevField(Abbv->getOperandInfo(0), Value)) {
    // Array found, use to read all elements.
    if (Value == 0)
      ErrHandler->Fatal("No code found for record!");
    const NaClBitCodeAbbrevOp &Op = Abbv->getOperandInfo(1);
    Code = readArrayAbbreviatedField(Op);
    readArrayAbbrev(Op, Value - 1, Vals);
    SkipToByteBoundaryIfAligned();
    return Code;
  }
  Code = Value;

  // Read arguments.
  unsigned NumOperands = Abbv->getNumOperandInfos();
  for (unsigned i = 1; i != NumOperands; ++i) {
    if (readRecordAbbrevField(Abbv->getOperandInfo(i), Value)) {
      ++i;
      readArrayAbbrev(Abbv->getOperandInfo(i), Value, Vals);
      SkipToByteBoundaryIfAligned();
      return Code;
    }
    Vals.push_back(Value);
  }
  SkipToByteBoundaryIfAligned();
  return Code;
}


NaClBitCodeAbbrevOp::Encoding NaClBitstreamCursor::
getEncoding(uint64_t Value) {
  if (!NaClBitCodeAbbrevOp::isValidEncoding(Value)) {
    std::string Buffer;
    raw_string_ostream StrBuf(Buffer);
    StrBuf << "Invalid abbreviation encoding specified in bitcode file: "
           << Value;
    ErrHandler->Fatal(StrBuf.str());
  }
  return NaClBitCodeAbbrevOp::Encoding(Value);
}

void NaClBitstreamCursor::ReadAbbrevRecord(bool IsLocal,
                                           NaClAbbrevListener *Listener) {
  NaClBitCodeAbbrev *Abbv = new NaClBitCodeAbbrev();
  unsigned NumOpInfo = ReadVBR(5);
  if (Listener) Listener->Values.push_back(NumOpInfo);
  for (unsigned i = 0; i != NumOpInfo; ++i) {
    bool IsLiteral = Read(1) ? true : false;
    if (Listener) Listener->Values.push_back(IsLiteral);
    if (IsLiteral) {
      uint64_t Value = ReadVBR64(8);
      if (Listener) Listener->Values.push_back(Value);
      Abbv->Add(NaClBitCodeAbbrevOp(Value));
      continue;
    }
    NaClBitCodeAbbrevOp::Encoding E = getEncoding(Read(3));
    if (Listener) Listener->Values.push_back(E);
    if (NaClBitCodeAbbrevOp::hasValue(E)) {
      unsigned Data = ReadVBR64(5);
      if (Listener) Listener->Values.push_back(Data);

      // As a special case, handle fixed(0) (i.e., a fixed field with zero bits)
      // and vbr(0) as a literal zero.  This is decoded the same way, and avoids
      // a slow path in Read() to have to handle reading zero bits.
      if ((E == NaClBitCodeAbbrevOp::Fixed || E == NaClBitCodeAbbrevOp::VBR) &&
          Data == 0) {
        if (Listener) Listener->Values.push_back(0);
        Abbv->Add(NaClBitCodeAbbrevOp(0));
        continue;
      }
      if (!NaClBitCodeAbbrevOp::isValid(E, Data)) {
        std::string Buffer;
        raw_string_ostream StrBuf(Buffer);
        StrBuf << "Invalid abbreviation encoding ("
               << NaClBitCodeAbbrevOp::getEncodingName(E)
               << ", " << Data << ")";
        ErrHandler->Fatal(StrBuf.str());
      }
      Abbv->Add(NaClBitCodeAbbrevOp(E, Data));
    } else {
      if (!NaClBitCodeAbbrevOp::isValid(E)) {
        std::string Buffer;
        raw_string_ostream StrBuf(Buffer);
        StrBuf << "Invalid abbreviation encoding ("
               << NaClBitCodeAbbrevOp::getEncodingName(E) << ")";
        ErrHandler->Fatal(StrBuf.str());
      }
      Abbv->Add(NaClBitCodeAbbrevOp(E));
    }
  }
  SkipToByteBoundaryIfAligned();
  if (!Abbv->isValid())
    ErrHandler->Fatal("Invalid abbreviation specified in bitcode file");
  CurAbbrevs.push_back(Abbv);
  if (Listener) {
    Listener->ProcessAbbreviation(Abbv, IsLocal);
    // Reset record information of the listener.
    Listener->Values.clear();
    Listener->StartBit = GetCurrentBitNo();
  }
}

void NaClBitstreamCursor::SkipAbbrevRecord() {
  unsigned NumOpInfo = ReadVBR(5);
  for (unsigned i = 0; i != NumOpInfo; ++i) {
    bool IsLiteral = Read(1) ? true : false;
    if (IsLiteral) {
      ReadVBR64(8);
      continue;
    }
    NaClBitCodeAbbrevOp::Encoding E = getEncoding(Read(3));
    if (NaClBitCodeAbbrevOp::hasValue(E)) {
      ReadVBR64(5);
    }
  }
  SkipToByteBoundaryIfAligned();
}

bool NaClBitstreamCursor::ReadBlockInfoBlock(NaClAbbrevListener *Listener) {
  // If this is the second stream to get to the block info block, skip it.
  if (BitStream->hasBlockInfoRecords())
    return SkipBlock();

  unsigned NumWords;
  if (EnterSubBlock(naclbitc::BLOCKINFO_BLOCK_ID, &NumWords)) return true;

  if (Listener) Listener->BeginBlockInfoBlock(NumWords);

  NaClBitcodeRecordVector Record;
  NaClBitstreamReader::BlockInfo *CurBlockInfo = 0;

  // Read records of the BlockInfo block.
  while (1) {
    if (Listener) Listener->StartBit = GetCurrentBitNo();
    NaClBitstreamEntry Entry = advance(AF_DontAutoprocessAbbrevs, Listener);

    switch (Entry.Kind) {
    case llvm::NaClBitstreamEntry::SubBlock:  // PNaCl doesn't allow!
    case llvm::NaClBitstreamEntry::Error:
      return true;
    case llvm::NaClBitstreamEntry::EndBlock:
      if (Listener) Listener->EndBlockInfoBlock();
      return false;
    case llvm::NaClBitstreamEntry::Record:
      // The interesting case.
      break;
    }

    // Read abbrev records, associate them with CurBID.
    if (Entry.ID == naclbitc::DEFINE_ABBREV) {
      if (!CurBlockInfo) return true;
      ReadAbbrevRecord(false, Listener);

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
      default: 
        // No other records should be found!
        return true;
      case naclbitc::BLOCKINFO_CODE_SETBID:
        if (Record.size() < 1) return true;
        CurBlockInfo = &BitStream->getOrCreateBlockInfo((unsigned)Record[0]);
        if (Listener) {
          Listener->Values = Record;
          Listener->SetBID();
        }
        break;
    }
  }
}
