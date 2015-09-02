//===- llvm/unittest/Bitcode/NaClParseTypesTest.cpp ---------------------===//
//     Tests parser for PNaCl bitcode.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// Tests record errors in the types block when parsing PNaCl bitcode.

// TODO(kschimpf) Add more tests.

#include "NaClMungeTest.h"

using namespace llvm;

namespace naclmungetest {

TEST(NaClParseTypesTest, BadTypeReferences) {
  const uint64_t BitcodeRecords[] = {
    1, 65535, 8, 2, Terminator,
    1, 65535, 17, 2, Terminator,
    3, 1, 2, Terminator,
    3, 7, 32, Terminator,
    3, 3, Terminator,
    0, 65534, Terminator,
    0, 65534, Terminator
  };

  const uint64_t ReplaceIndex = 4;

  // Show text of base input.
  NaClObjDumpMunger BaseMunger(ARRAY_TERM(BitcodeRecords));
  EXPECT_TRUE(BaseMunger.runTest());
  EXPECT_EQ(
      "       0:0|<65532, 80, 69, 88, 69, 1, 0,|Magic Number: 'PEXE' (80, 69, "
      "88, 69)\n"
      "          | 8, 0, 17, 0, 4, 0, 2, 0, 0, |PNaCl Version: 2\n"
      "          | 0>                          |\n"
      "      16:0|1: <65535, 8, 2>             |module {  // BlockID = 8\n"
      "      24:0|  1: <65535, 17, 2>          |  types {  // BlockID = 17\n"
      "      32:0|    3: <1, 2>                |    count 2;\n"
      "      34:4|    3: <7, 32>               |    @t0 = i32;\n"
      "      37:6|    3: <3>                   |    @t1 = float;\n"
      "      39:4|  0: <65534>                 |  }\n"
      "      40:0|0: <65534>                   |}\n",
      BaseMunger.getTestResults());

  // Show that we successfully parse the base input.
  NaClParseBitcodeMunger Munger(ARRAY_TERM(BitcodeRecords));
  EXPECT_TRUE(Munger.runTest(true));
  EXPECT_EQ(
      "Successful parse!\n",
      Munger.getTestResults());

  // Show what happens when misdefining: @t1 = float"
  const uint64_t AddSelfReference[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 3, 1, Terminator
  };
  EXPECT_FALSE(Munger.runTest(ARRAY(AddSelfReference), false));
  EXPECT_EQ(
      "Error: Record doesn't have expected size or structure\n",
      Munger.getTestResults());
  EXPECT_FALSE(Munger.runTest(ARRAY(AddSelfReference), true));
  EXPECT_EQ(
      "Error(40:2): Invalid TYPE_CODE_FLOAT record\n"
      "Error: Record doesn't have expected size or structure\n",
      Munger.getTestResults());
}

} // end of namespace naclmungetest
