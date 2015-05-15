//===- llvm/unittest/Bitcode/NaClBitstreamReaderTest.cpp ------------------===//
//     Tests issues in NaCl Bitstream Reader.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// Tests issues in NaCl Bitstream Reader.

// TODO(kschimpf) Add more Tests.

#include "llvm/Bitcode/NaCl/NaClBitstreamReader.h"

#include "gtest/gtest.h"

using namespace llvm;

namespace {

static const uint64_t BitZero = 0;

// Initializes array to sequence of alternating zeros/ones.
void* InitAltOnes(uint8_t *Array, size_t ArraySize) {
  for (size_t i = 0; i <ArraySize; ++i) {
    Array[i] = 0x9;
  }
  return Array;
}

// Tests that the default bitstream cursor is at bit zero.
TEST(NaClBitstreamTest, DefaultCursorAtBitZero) {
  uint8_t CursorMemory[sizeof(NaClBitstreamCursor)];
  NaClBitstreamCursor *Cursor =
      new (InitAltOnes(CursorMemory, sizeof(NaClBitstreamCursor)))
      NaClBitstreamCursor();
  EXPECT_EQ(BitZero, Cursor->GetCurrentBitNo());
}

// Tests that when we initialize the bitstream cursor with an array-filled
// bitstream reader, the cursor is at bit zero.
TEST(NaClBitstreamTest, ReaderCursorAtBitZero) {
  static const size_t BufferSize = 12;
  unsigned char Buffer[BufferSize];
  NaClBitstreamReader Reader(
      getNonStreamedMemoryObject(Buffer, Buffer+BufferSize), 0);
  uint8_t CursorMemory[sizeof(NaClBitstreamCursor)];
  NaClBitstreamCursor *Cursor =
      new (InitAltOnes(CursorMemory, sizeof(NaClBitstreamCursor)))
      NaClBitstreamCursor(Reader);
  EXPECT_EQ(BitZero, Cursor->GetCurrentBitNo());
}

TEST(NaClBitstreamTest, CursorAtReaderInitialAddress) {
  static const size_t BufferSize = 12;
  static const size_t InitialAddress = 8;
  unsigned char Buffer[BufferSize];
  NaClBitstreamReader Reader(
      getNonStreamedMemoryObject(Buffer, Buffer+BufferSize), InitialAddress);
  uint8_t CursorMemory[sizeof(NaClBitstreamCursor)];
  NaClBitstreamCursor *Cursor =
      new (InitAltOnes(CursorMemory, sizeof(NaClBitstreamCursor)))
      NaClBitstreamCursor(Reader);
  EXPECT_EQ(InitialAddress * CHAR_BIT, Cursor->GetCurrentBitNo());
}

} // end of anonymous namespace
