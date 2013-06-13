//===- NaClBitcodeHeader.cpp ----------------------------------------------===//
//     PNaCl bitcode header reader.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/NaCl/NaClBitcodeHeader.h"
#include "llvm/Bitcode/NaCl/NaClReaderWriter.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/StreamableMemoryObject.h"

#include <limits>
#include <cstring>
#include <iomanip>

using namespace llvm;

NaClBitcodeHeaderField::NaClBitcodeHeaderField()
    : ID(kInvalid), FType(kBufferType), Len(0), Data(0) {}

NaClBitcodeHeaderField::NaClBitcodeHeaderField(Tag MyID, uint32_t MyValue)
    : ID(MyID), FType(kUInt32Type), Len(4), Data(new uint8_t[4]) {
  Data[0] = static_cast<uint8_t>(MyValue & 0xFF);
  Data[1] = static_cast<uint8_t>((MyValue >> 8) & 0xFF);
  Data[2] = static_cast<uint8_t>((MyValue >> 16) & 0xFF);
  Data[3] = static_cast<uint8_t>((MyValue >> 24) & 0xFF);
}

uint32_t NaClBitcodeHeaderField::GetUInt32Value() const {
  assert(FType == kUInt32Type && "Header field must be uint32");
  return static_cast<uint32_t>(Data[0]) |
         (static_cast<uint32_t>(Data[1]) << 8) |
         (static_cast<uint32_t>(Data[2]) << 16) |
         (static_cast<uint32_t>(Data[2]) << 24);
}

NaClBitcodeHeaderField::NaClBitcodeHeaderField(Tag MyID, size_t MyLen,
                                               uint8_t *MyData)
    : ID(MyID), FType(kBufferType), Len(MyLen), Data(new uint8_t[MyLen]) {
  for (size_t i = 0; i < MyLen; ++i) {
    Data[i] = MyData[i];
  }
}

bool NaClBitcodeHeaderField::Write(uint8_t *Buf, size_t BufLen) const {
  size_t FieldsLen = kTagLenSize + Len;
  size_t PadLen = (WordSize - (FieldsLen & (WordSize-1))) & (WordSize-1);
  // Ensure buffer is large enough and that length can be represented
  // in 32 bits
  if (BufLen < FieldsLen + PadLen ||
      Len > std::numeric_limits<FixedSubfield>::max())
    return false;

  WriteFixedSubfield(EncodeTypedID(), Buf);
  WriteFixedSubfield(static_cast<FixedSubfield>(Len),
                     Buf + sizeof(FixedSubfield));
  memcpy(Buf + kTagLenSize, Data, Len);
  // Pad out to word alignment
  if (PadLen) {
    memset(Buf + FieldsLen, 0, PadLen);
  }
  return true;
}

bool NaClBitcodeHeaderField::Read(const uint8_t *Buf, size_t BufLen) {
  if (BufLen < kTagLenSize)
    return false;
  FixedSubfield IdField;
  ReadFixedSubfield(&IdField, Buf);
  FixedSubfield LengthField;
  ReadFixedSubfield(&LengthField, Buf + sizeof(FixedSubfield));
  size_t Length = static_cast<size_t>(LengthField);
  if (BufLen < kTagLenSize + Length)
    return false;
  if (Len != Length) {
    // Need to reallocate data buffer.
    if (Data)
      delete[] Data;
    Data = new uint8_t[Length];
  }
  Len = Length;
  DecodeTypedID(IdField, ID, FType);
  memcpy(Data, Buf + kTagLenSize, Len);
  return true;
}

std::string NaClBitcodeHeaderField::Contents() const {
  std::string buffer;
  raw_string_ostream ss(buffer);
  switch (ID) {
  case kPNaClVersion:
    ss << "PNaCl Version";
    break;
  case kInvalid:
    ss << "Invalid";
    break;
  default:
    report_fatal_error("PNaCl bitcode file contains unknown field tag");
  }
  ss << ": ";
  switch (FType) {
  case kUInt32Type:
    ss << GetUInt32Value();
    break;
  case kBufferType:
    ss << "[";
    for (size_t i = 0; i < Len; ++i) {
      if (i)
        ss << " ";
      ss << format("%02x", Data[i]);
    }
    ss << "]";
    break;
  default:
    report_fatal_error("PNaCL bitcode file contains unknown field type");
  }
  return ss.str();
}

NaClBitcodeHeader::NaClBitcodeHeader()
    : HeaderSize(0), UnsupportedMessage(), IsSupportedFlag(false),
      IsReadableFlag(false), PNaClVersion(0) {}

NaClBitcodeHeader::~NaClBitcodeHeader() {
  for (std::vector<NaClBitcodeHeaderField *>::const_iterator
           Iter = Fields.begin(),
           IterEnd = Fields.end();
       Iter != IterEnd; ++Iter) {
    delete *Iter;
  }
}

bool NaClBitcodeHeader::ReadPrefix(const unsigned char *BufPtr,
                                   const unsigned char *BufEnd,
                                   unsigned &NumFields, unsigned &NumBytes) {
  // Must contain PEXE.
  if (!isNaClBitcode(BufPtr, BufEnd))
    return true;
  BufPtr += WordSize;

  // Read #Fields and number of bytes needed for the header.
  if (BufPtr + WordSize > BufEnd)
    return true;
  NumFields = static_cast<unsigned>(BufPtr[0]) |
      (static_cast<unsigned>(BufPtr[1]) << 8);
  NumBytes = static_cast<unsigned>(BufPtr[2]) |
      (static_cast<unsigned>(BufPtr[3]) << 8);
  BufPtr += WordSize;
  return false;
}

bool NaClBitcodeHeader::ReadFields(const unsigned char *BufPtr,
                                   const unsigned char *BufEnd,
                                   unsigned NumFields, unsigned NumBytes) {
  HeaderSize = NumBytes + (2 * WordSize);

  // Read in each field.
  for (size_t i = 0; i < NumFields; ++i) {
    NaClBitcodeHeaderField *Field = new NaClBitcodeHeaderField();
    Fields.push_back(Field);
    if (!Field->Read(BufPtr, BufEnd - BufPtr))
      return true;
    size_t FieldSize = Field->GetTotalSize();
    BufPtr += FieldSize;
  }
  return false;
}

bool NaClBitcodeHeader::Read(const unsigned char *&BufPtr,
                             const unsigned char *&BufEnd) {
  unsigned NumFields;
  unsigned NumBytes;
  if (ReadPrefix(BufPtr, BufEnd, NumFields, NumBytes))
    return true;
  BufPtr += 2 * WordSize;

  if (ReadFields(BufPtr, BufEnd, NumFields, NumBytes))
    return true;
  BufPtr += NumBytes;
  InstallFields();
  return false;
}

bool NaClBitcodeHeader::Read(StreamableMemoryObject *Bytes) {
  unsigned NumFields;
  unsigned NumBytes;
  {
    unsigned char Buffer[2 * WordSize];
    if (Bytes->readBytes(0, sizeof(Buffer), Buffer, NULL) ||
        ReadPrefix(Buffer, Buffer + sizeof(Buffer), NumFields, NumBytes))
      return true;
  }
  uint8_t *Header = new uint8_t[NumBytes];
  bool failed =
      Bytes->readBytes(2 * WordSize, NumBytes, Header, NULL) ||
      ReadFields(Header, Header + NumBytes, NumFields, NumBytes);
  delete[] Header;
  if (failed)
    return true;
  InstallFields();
  return false;
}

NaClBitcodeHeaderField *
NaClBitcodeHeader::GetTaggedField(NaClBitcodeHeaderField::Tag ID) const {
  for (std::vector<NaClBitcodeHeaderField *>::const_iterator
           Iter = Fields.begin(),
           IterEnd = Fields.end();
       Iter != IterEnd; ++Iter) {
    if ((*Iter)->GetID() == ID) {
      return *Iter;
    }
  }
  return 0;
}

NaClBitcodeHeaderField *NaClBitcodeHeader::GetField(size_t index) const {
  if (index >= Fields.size())
    return 0;
  return Fields[index];
}

NaClBitcodeHeaderField *GetPNaClVersionPtr(NaClBitcodeHeader *Header) {
  if (NaClBitcodeHeaderField *Version =
          Header->GetTaggedField(NaClBitcodeHeaderField::kPNaClVersion)) {
    if (Version->GetType() == NaClBitcodeHeaderField::kUInt32Type) {
      return Version;
    }
  }
  return 0;
}

void NaClBitcodeHeader::InstallFields() {
  // Assume supported until contradicted.
  bool UpdatedUnsupportedMessage = false;
  IsSupportedFlag = true;
  IsReadableFlag = true;
  UnsupportedMessage = "Supported";
  PNaClVersion = 0;
  if (NaClBitcodeHeaderField *Version = GetPNaClVersionPtr(this)) {
    PNaClVersion = Version->GetUInt32Value();
  }
  if (PNaClVersion != 1) {
    IsSupportedFlag = false;
    IsReadableFlag = false;
    UnsupportedMessage = "Unsupported Version";
    UpdatedUnsupportedMessage = true;
  }
  if (Fields.size() != 1) {
    IsSupportedFlag = false;
    IsReadableFlag = false;
    if (!UpdatedUnsupportedMessage)
      UnsupportedMessage = "Unknown header field(s) found";
  }
}
