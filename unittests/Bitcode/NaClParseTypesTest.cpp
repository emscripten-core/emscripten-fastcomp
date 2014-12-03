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

#include "llvm/ADT/STLExtras.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeMunge.h"

#include "gtest/gtest.h"

using namespace llvm;

namespace {

static const uint64_t Terminator = 0x5768798008978675LL;

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
  NaClObjDumpMunger BaseMunger(BitcodeRecords,
                               array_lengthof(BitcodeRecords), Terminator);
  EXPECT_TRUE(BaseMunger.runTestForAssembly("Bad type references base"));
  EXPECT_EQ(
      "module {  // BlockID = 8\n"
      "  types {  // BlockID = 17\n"
      "    count 2;\n"
      "    @t0 = i32;\n"
      "    @t1 = float;\n"
      "  }\n"
      "}\n",
      BaseMunger.getTestResults());

  // Show that we successfully parse the base input.
  NaClParseBitcodeMunger Munger(BitcodeRecords,
                                array_lengthof(BitcodeRecords), Terminator);
  EXPECT_TRUE(Munger.runTest("base parse", true));
  EXPECT_EQ(
      "Successful parse!\n",
      Munger.getTestResults());

  // Show what happens when misdefining: @t1 = float"
  const uint64_t AddSelfReference[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 3, 1, Terminator
  };
  EXPECT_FALSE(Munger.runTest(
      "@t1 = float(1)",
      AddSelfReference, array_lengthof(AddSelfReference),
      false));
  EXPECT_EQ(
      "Error: Record doesn't have expected size or structure\n",
      Munger.getTestResults());
  EXPECT_FALSE(Munger.runTest(
      "@t1 = float(1)",
      AddSelfReference, array_lengthof(AddSelfReference),
      true));
  EXPECT_EQ(
      "Error: Invalid TYPE_CODE_FLOAT record\n"
      "Error: Record doesn't have expected size or structure\n",
      Munger.getTestResults());
}

} // end of anonamous namespace.
