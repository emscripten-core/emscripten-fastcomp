//===- llvm/unittest/Bitcode/NaClAbbrevErrorTests.cpp ---------------------===//
//     Tests parser for PNaCl bitcode instructions.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// Tests errors on bad abbreviation index.

#include "llvm/ADT/STLExtras.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeMunge.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeParser.h"
#include "llvm/Bitcode/NaCl/NaClLLVMBitCodes.h"

#include "gtest/gtest.h"

using namespace llvm;

namespace {

static const uint64_t Terminator = 0x5768798008978675LL;

/// Test if we handle badly defined abbreviation indices.
TEST(MyDeathNaClAbbrevErrorTests, BadAbbreviationIndex) {
  const uint64_t BitcodeRecords[] = {
    1, naclbitc::BLK_CODE_ENTER, naclbitc::MODULE_BLOCK_ID, 2, Terminator,
    1, naclbitc::BLK_CODE_ENTER, naclbitc::TYPE_BLOCK_ID_NEW, 3, Terminator,
    3, naclbitc::TYPE_CODE_NUMENTRY, 2, Terminator,
    3, naclbitc::TYPE_CODE_VOID, Terminator,
    3, naclbitc::TYPE_CODE_FUNCTION, 0, 0, Terminator,
    0, naclbitc::BLK_CODE_EXIT, Terminator,
    3, naclbitc::MODULE_CODE_FUNCTION, 1, 0, 0, 0, Terminator,
    1, naclbitc::BLK_CODE_ENTER, naclbitc::FUNCTION_BLOCK_ID, 2, Terminator,
    3, naclbitc::FUNC_CODE_DECLAREBLOCKS, 1, Terminator,
    3, naclbitc::FUNC_CODE_INST_RET, Terminator,
    0, naclbitc::BLK_CODE_EXIT, Terminator,
    0, naclbitc::BLK_CODE_EXIT, Terminator
  };

  const char* ExpectedDump =
      "       0:0|<65532, 80, 69, 88, 69, 1, 0,|Magic Number: 'PEXE' (80, 69, "
      "88, 69)\n"
      "          | 8, 0, 17, 0, 4, 0, 2, 0, 0, |PNaCl Version: 2\n"
      "          | 0>                          |\n"
      "      16:0|1: <65535, 8, 2>             |module {  // BlockID = 8\n"
      "      24:0|  1: <65535, 17, 3>          |  types {  // BlockID = 17\n"
      "      32:0|    3: <1, 2>                |    count 2;\n"
      "      34:5|    3: <2>                   |    @t0 = void;\n"
      "      36:4|    3: <21, 0, 0>            |    @t1 = void ();\n"
      "      39:7|  0: <65534>                 |  }\n"
      "      44:0|  3: <8, 1, 0, 0, 0>         |  define external void @f0();\n"
      "      48:6|  1: <65535, 12, 2>          |  function void @f0() {  \n"
      "          |                             |                   // BlockID "
      "= 12\n"
      "      56:0|    3: <1, 1>                |    blocks 1;\n"
      "          |                             |  %b0:\n"
      "      58:4|    3: <10>                  |    ret void;\n"
      "      60:2|  0: <65534>                 |  }\n"
      "      64:0|0: <65534>                   |}\n"
      ;

  // Show that we can parse this code.
  NaClObjDumpMunger DumpMunger(BitcodeRecords,
                               array_lengthof(BitcodeRecords), Terminator);

  EXPECT_TRUE(DumpMunger.runTest("BadAbbreviationIndex assembly"));
  EXPECT_EQ(ExpectedDump, DumpMunger.getTestResults());

  // Shows what happens when we change the abbreviation index to an
  // illegal value.
  const uint64_t ReplaceIndex = 3; // Index for TYPE_CODE_VOID;
  const uint64_t AbbrevIndex4[] = {
    ReplaceIndex, NaClMungedBitcode::Replace,
    4, naclbitc::TYPE_CODE_VOID, Terminator,
  };

  // Show that by default, one can't write a bad abbreviation index.
  {
    NaClObjDumpMunger DumpMunger(BitcodeRecords,
                                 array_lengthof(BitcodeRecords), Terminator);
    DumpMunger.setRunAsDeathTest(true);
    EXPECT_DEATH(
        DumpMunger.runTest("Bad abbreviation index 4",
                           AbbrevIndex4, array_lengthof(AbbrevIndex4)),
        ".*Error \\(Block 17\\)\\: Uses illegal abbreviation index\\:"
        "        4\\: \\[2\\].*");
  }

  // Show that the corresponding error is generated when reading
  // bitcode with a bad abbreviation index.
  {
    NaClObjDumpMunger DumpMunger(BitcodeRecords,
                                 array_lengthof(BitcodeRecords), Terminator);
    DumpMunger.setRunAsDeathTest(true);
    DumpMunger.setWriteBadAbbrevIndex(true);
    EXPECT_DEATH(
      DumpMunger.runTest("Bad abbreviation index 4",
                         AbbrevIndex4, array_lengthof(AbbrevIndex4)),
      ".*Error \\(Block 17\\)\\: Uses illegal abbreviation index\\:"
      "        4\\: \\[2\\].*"
      ".*Fatal\\(35\\:0\\)\\: Invalid abbreviation \\# 4 defined for record.*");
  }

  // Test that bitcode reader reports problem correctly.
  {
    NaClParseBitcodeMunger Munger(BitcodeRecords,
                                  array_lengthof(BitcodeRecords), Terminator);
    Munger.setRunAsDeathTest(true);
    Munger.setWriteBadAbbrevIndex(true);
    EXPECT_DEATH(
      Munger.runTest("Bad abbreviation index",
                     AbbrevIndex4, array_lengthof(AbbrevIndex4), true),
      ".*Error \\(Block 17\\)\\: Uses illegal abbreviation index\\:"
      "        4\\: \\[2\\].*"
      ".*Fatal\\(35\\:0\\)\\: Invalid abbreviation \\# 4 defined for record.*");
  }

  // Show that error recovery works when dumping bitcode.
  DumpMunger.setTryToRecoverOnWrite(true);
  EXPECT_TRUE(
      DumpMunger.runTest("Bad abbreviation index 4",
                         AbbrevIndex4, array_lengthof(AbbrevIndex4)));
  std::string Results(
      "Error (Block 17): Uses illegal abbreviation index:        4: [2]\n");
  Results.append(ExpectedDump);
  EXPECT_EQ(Results, DumpMunger.getTestResults());

  // Show that error recovery works when parsing bitcode.
  NaClParseBitcodeMunger Munger(BitcodeRecords,
                                array_lengthof(BitcodeRecords), Terminator);
  Munger.setTryToRecoverOnWrite(true);
  EXPECT_TRUE(
      Munger.runTest("Bad abbreviation index 4",
                     AbbrevIndex4, array_lengthof(AbbrevIndex4), true));
  EXPECT_EQ(
      "Error (Block 17): Uses illegal abbreviation index:        4: [2]\n"
      "Successful parse!\n",
      Munger.getTestResults());
}

} // end of anonymous namespace.
