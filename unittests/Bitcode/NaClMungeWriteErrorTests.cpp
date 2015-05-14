//===- llvm/unittest/Bitcode/NaClMungeWriteErrorTests.cpp -----------------===//
//     Tests parser for PNaCl bitcode instructions.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// Tests write errors for munged bitcode.

#include "llvm/ADT/STLExtras.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeMunge.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeParser.h"
#include "llvm/Bitcode/NaCl/NaClLLVMBitCodes.h"

#include "gtest/gtest.h"

using namespace llvm;

namespace {

// Test list of bitcode records.
static const uint64_t Terminator = 0x5768798008978675LL;
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

// Expected output when bitcode records are dumped.
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

// Edit to change void type with an illegal abbreviation index.
const uint64_t VoidTypeIndex = 3; // Index for "@t0 = void".
const uint64_t AbbrevIndex4VoidTypeEdit[] = {
  VoidTypeIndex, NaClMungedBitcode::Replace,
  4, naclbitc::TYPE_CODE_VOID, Terminator,
};

// Edit to add local abbreviation for "ret void", and then use on that
// instruction.
const uint64_t RetVoidIndex = 9; // return void;
const uint64_t UseLocalRetVoidAbbrevEdits[] = {
  RetVoidIndex, NaClMungedBitcode::AddBefore,
    2, naclbitc::BLK_CODE_DEFINE_ABBREV, 1, 1,
       naclbitc::FUNC_CODE_INST_RET, Terminator,
  RetVoidIndex, NaClMungedBitcode::Replace,
    4, naclbitc::FUNC_CODE_INST_RET, Terminator
};

#define ARRAY_ARGS(Records) Records, array_lengthof(Records)

#define ARRAY_ARGS_TERM(Records) ARRAY_ARGS(Records), Terminator

std::string stringify(NaClBitcodeMunger &Munger) {
  std::string Buffer;
  raw_string_ostream StrBuf(Buffer);
  Munger.getMungedBitcode().print(StrBuf);
  return StrBuf.str();
}

// Show that we can dump the bitcode records
TEST(NaClMungeWriteErrorTests, DumpBitcodeRecords) {
  NaClObjDumpMunger Munger(ARRAY_ARGS_TERM(BitcodeRecords));
  EXPECT_TRUE(Munger.runTest());
  EXPECT_EQ(ExpectedDump, Munger.getTestResults());
}

// Show that by default, one can't write a bad abbreviation index.
TEST(NaClMungeWriteErrorTests, CantWriteBadAbbrevIndex) {
  NaClWriteMunger Munger(ARRAY_ARGS_TERM(BitcodeRecords));
  EXPECT_FALSE(Munger.runTest(ARRAY_ARGS(AbbrevIndex4VoidTypeEdit)));
  EXPECT_EQ(
      "Error (Block 17): Uses illegal abbreviation index: 4: [2]\n"
      "Error: Unable to generate bitcode file due to write errors\n",
      Munger.getTestResults());
}

// Show that we can't write more local abbreviations than specified in
// the corresponding enclosing block.
TEST(NaClMungeWriteErrorTests, CantWriteTooManyLocalAbbreviations) {
  NaClWriteMunger Munger(ARRAY_ARGS_TERM(BitcodeRecords));
  Munger.munge(ARRAY_ARGS(UseLocalRetVoidAbbrevEdits));
  EXPECT_EQ(
      "       1: [65535, 8, 2]\n"
      "         1: [65535, 17, 3]\n"
      "           3: [1, 2]\n"
      "           3: [2]\n"
      "           3: [21, 0, 0]\n"
      "         0: [65534]\n"
      "         3: [8, 1, 0, 0, 0]\n"
      "         1: [65535, 12, 2]\n"
      "           3: [1, 1]\n"
      "           2: [65533, 1, 1, 10]\n"
      "           4: [10]\n"
      "         0: [65534]\n"
      "       0: [65534]\n",
      stringify(Munger));

  EXPECT_FALSE(Munger.runTest());
  EXPECT_EQ(
      "Error (Block 12): Exceeds abbreviation index limit of 3: 2: [65533,"
      " 1, 1, 10]\n"
      "Error: Unable to generate bitcode file due to write errors\n",
      Munger.getTestResults());
}

// Show what happens when there are more enter blocks then exit blocks.
TEST(NaClMungeWriteErrorTests, CantWriteTooManyEnterBlocks) {
  NaClWriteMunger Munger(ARRAY_ARGS_TERM(BitcodeRecords));
  // Remove all but first two records (i.e. two enter blocks).
  NaClMungedBitcode &MungedBitcode = Munger.getMungedBitcode();
  for (size_t i = 2; i < MungedBitcode.getBaseRecords().size(); ++i) {
    MungedBitcode.remove(i);
  }

  EXPECT_FALSE(Munger.runTest());
  EXPECT_EQ(
      "Error (Block 17): Missing close block.\n"
      "Error (Block 8): Missing close block.\n"
      "Error: Unable to generate bitcode file due to write errors\n",
      Munger.getTestResults());
}

// Show what happens when there are fewer enter blocks than exit
// blocks.
TEST(NaClMungeWriteErrorTests, CantWriteTooManyExitBlocks) {
  NaClWriteMunger Munger(ARRAY_ARGS_TERM(BitcodeRecords));
  // Add two exit blocks.
  NaClMungedBitcode &MungedBitcode = Munger.getMungedBitcode();
  NaClRecordVector Values;
  NaClBitcodeAbbrevRecord Record(0, naclbitc::BLK_CODE_EXIT, Values);
  for (size_t i = 0; i < 2; ++i)
    MungedBitcode.addAfter(MungedBitcode.getBaseRecords().size() - 1, Record);

  EXPECT_FALSE(Munger.runTest());
  EXPECT_EQ(
      "Error (Block unknown): Extraneous exit block: 0: [65534]\n"
      "Error: Unable to generate bitcode file due to write errors\n",
      Munger.getTestResults());
}

// Show that an error occurs when writing a bitcode record that isn't
// in any block.
TEST(NaClMungeWriteErrorTests, CantWriteRecordOutsideBlock) {
  NaClWriteMunger Munger(ARRAY_ARGS_TERM(BitcodeRecords));
  NaClMungedBitcode &MungedBitcode = Munger.getMungedBitcode();
  NaClRecordVector Values;
  Values.push_back(4);
  NaClBitcodeAbbrevRecord Record(naclbitc::UNABBREV_RECORD,
                                 naclbitc::MODULE_CODE_VERSION,
                                 Values);

  MungedBitcode.addAfter(MungedBitcode.getBaseRecords().size() - 1, Record);
  EXPECT_FALSE(Munger.runTest());
  EXPECT_EQ(
      "Error (Block unknown): Record outside block: 3: [1, 4]\n"
      "Error: Unable to generate bitcode file due to write errors\n",
      Munger.getTestResults());
}

// Show that no error occurs if we write out the maximum allowable
// block abbreviation index bit limit.
TEST(NaClMungerWriteErrorTests, CanWriteBlockWithMaxLimit) {
  // Replace initial block enter with maximum bit size.
  const uint64_t Edit[] = {
    0, NaClMungedBitcode::Replace,
    1, naclbitc::BLK_CODE_ENTER, naclbitc::MODULE_BLOCK_ID,
       naclbitc::MaxAbbrevWidth, Terminator
  };
  NaClWriteMunger Munger(ARRAY_ARGS_TERM(BitcodeRecords));
  EXPECT_TRUE(Munger.runTest(ARRAY_ARGS(Edit)));
  EXPECT_EQ(
      "       1: [65535, 8, 32]\n"
      "         1: [65535, 17, 3]\n"
      "           3: [1, 2]\n"
      "           3: [2]\n"
      "           3: [21, 0, 0]\n"
      "         0: [65534]\n"
      "         3: [8, 1, 0, 0, 0]\n"
      "         1: [65535, 12, 2]\n"
      "           3: [1, 1]\n"
      "           3: [10]\n"
      "         0: [65534]\n"
      "       0: [65534]\n",
      Munger.getTestResults());
}

// Show that an error occurs if the block abbreviation index bit limit is
// greater than the maximum allowable.
TEST(NaClMungerWriteErrorTests, CantWriteBlockWithBadBitLimit) {
  // Replace initial block enter with value out of range.
  const uint64_t Edit[] = {
    0, NaClMungedBitcode::Replace,
    1, naclbitc::BLK_CODE_ENTER, naclbitc::MODULE_BLOCK_ID,
       naclbitc::MaxAbbrevWidth + 1, Terminator
  };
  NaClWriteMunger Munger(ARRAY_ARGS_TERM(BitcodeRecords));
  EXPECT_FALSE(Munger.runTest(ARRAY_ARGS(Edit)));
  EXPECT_EQ(
      "Error (Block unknown): Block index bit limit 33 invalid. Must be in"
      " [2..32]: 1: [65535, 8, 33]\n"
      "Error: Unable to generate bitcode file due to write errors\n",
      Munger.getTestResults());
}

// Show that we can't write an enter block with a very large block id.
TEST(NaClMungerWriteErrorTests, CantWriteBlockWithLargeBlockID) {
  // Replace initial block enter with value out of range.
  const uint64_t Edit[] = {
    0, NaClMungedBitcode::Replace,
    1, naclbitc::BLK_CODE_ENTER, (uint64_t)1 << 33, 2, Terminator
  };
  NaClWriteMunger Munger(ARRAY_ARGS_TERM(BitcodeRecords));
  EXPECT_FALSE(Munger.runTest(ARRAY_ARGS(Edit)));
  EXPECT_EQ(
      "Error (Block unknown): Block id must be <= 4294967295: 1:"
      " [65535, 8589934592, 2]\n"
      "Error: Unable to generate bitcode file due to write errors\n",
      Munger.getTestResults());
}

// Show that writing successfully writes out an illegal abbreviation
// index, and then the parser fails to parse that illegal abbreviation.
TEST(MyNaClMungerWriteErrorTests, DieOnWriteBadAbbreviationIndex) {
  NaClWriteMunger Munger(ARRAY_ARGS_TERM(BitcodeRecords));
  Munger.setWriteBadAbbrevIndex(true);
  Munger.setRunAsDeathTest(true);
  EXPECT_DEATH(
      Munger.runTest(ARRAY_ARGS(AbbrevIndex4VoidTypeEdit)),
      ".*"
      // Report problem while writing.
      "Error \\(Block 17\\)\\: Uses illegal abbreviation index\\: 4\\: \\[2\\]"
      ".*"
      // Corresponding error while parsing.
      "Fatal\\(35\\:0)\\: Invalid abbreviation \\# 4 defined for record"
      ".*"
      // Output of report_fatal_error.
      "LLVM ERROR\\: Unable to continue"
      ".*");
}

// Show that error recovery works when writing an illegal abbreviation
// index. Show success by parsing fixed bitcode.
TEST(NaClMungeWriteErrorTests, RecoverWhenParsingBadAbbrevIndex) {
  NaClParseBitcodeMunger Munger(ARRAY_ARGS_TERM(BitcodeRecords));
  Munger.setTryToRecoverOnWrite(true);
  EXPECT_TRUE(
      Munger.runTest(ARRAY_ARGS(AbbrevIndex4VoidTypeEdit), true));
  EXPECT_EQ(
      "Error (Block 17): Uses illegal abbreviation index: 4: [2]\n"
      "Successful parse!\n",
      Munger.getTestResults());
}

// Show that error recovery works when writing an illegal abbreviation
// index.  Show success by Dumping fixed bitcode.
TEST(NaClMungeWriteErrorTests, RecoverWhenParsingBadAbbreviationIndex) {
  NaClObjDumpMunger Munger(ARRAY_ARGS_TERM(BitcodeRecords));
  Munger.setTryToRecoverOnWrite(true);
  EXPECT_TRUE(Munger.runTest(ARRAY_ARGS(AbbrevIndex4VoidTypeEdit)));
  std::string Results(
      "Error (Block 17): Uses illegal abbreviation index: 4: [2]\n");
  Results.append(ExpectedDump);
  EXPECT_EQ(Results, Munger.getTestResults());
}

// Show that error recovery works when writing too many locally
// defined abbreviations for the corresponding number of bits defined
// in the corresponding enter block. Show success by dumping the fixed
// bitcode.
TEST(NaClMungeWriteErrorTests, RecoverTooManyLocalAbbreviations) {
  NaClObjDumpMunger Munger(ARRAY_ARGS_TERM(BitcodeRecords));
  Munger.setTryToRecoverOnWrite(true);
  Munger.munge(ARRAY_ARGS(UseLocalRetVoidAbbrevEdits));

  EXPECT_TRUE(Munger.runTest());
  std::string Results(
      "Error (Block 12): Exceeds abbreviation index limit of 3: 2:"
      " [65533, 1, 1, 10]\n"
      "Error (Block 12): Uses illegal abbreviation index: 4: [10]\n");
  Results.append(ExpectedDump);
  EXPECT_EQ(
      Results,
      Munger.getTestResults());
}

// Show that error recovery works when writing and there are more
// enter blocks than exit blocks. Show success by dumping fixed
// bitcode.
TEST(NaClMungeWriteErrorTests, RecoverTooManyEnterBlocks) {
  NaClObjDumpMunger Munger(ARRAY_ARGS_TERM(BitcodeRecords));
  // Remove all but first two records (i.e. two enter blocks).
  NaClMungedBitcode &MungedBitcode = Munger.getMungedBitcode();
  for (size_t i = 2; i < MungedBitcode.getBaseRecords().size(); ++i) {
    MungedBitcode.remove(i);
  }

  Munger.setTryToRecoverOnWrite(true);
  EXPECT_TRUE(Munger.runTest());
  EXPECT_EQ(
      "Error (Block 17): Missing close block.\n"
      "Error (Block 8): Missing close block.\n"
      "       0:0|<65532, 80, 69, 88, 69, 1, 0,|Magic Number: 'PEXE' (80, 69,"
      " 88, 69)\n"
      "          | 8, 0, 17, 0, 4, 0, 2, 0, 0, |PNaCl Version: 2\n"
      "          | 0>                          |\n"
      "      16:0|1: <65535, 8, 2>             |module {  // BlockID = 8\n"
      "      24:0|  1: <65535, 17, 3>          |  types {  // BlockID = 17\n"
      "      32:0|  0: <65534>                 |  }\n"
      "      36:0|0: <65534>                   |}\n",
      Munger.getTestResults());
}

// Show that error recovery works when writing and there are fewer
// enter blocks than exit blocks. Show success by dumping the fixed
// bitcode.
TEST(NaClMungeWriteErrorTests, RecoverTooManyExitBlocks) {
  NaClObjDumpMunger Munger(ARRAY_ARGS_TERM(BitcodeRecords));
  // Add two exit blocks.
  NaClMungedBitcode &MungedBitcode = Munger.getMungedBitcode();
  NaClRecordVector Values;
  NaClBitcodeAbbrevRecord Record(0, naclbitc::BLK_CODE_EXIT, Values);
  for (size_t i = 0; i < 2; ++i)
    MungedBitcode.addAfter(MungedBitcode.getBaseRecords().size() - 1, Record);

  Munger.setTryToRecoverOnWrite(true);
  EXPECT_TRUE(Munger.runTest());
  std::string Results(
      "Error (Block unknown): Extraneous exit block: 0: [65534]\n"
      "Error (Block unknown): Extraneous exit block: 0: [65534]\n");
  Results.append(ExpectedDump);
  EXPECT_EQ(
      Results,
      Munger.getTestResults());
}

// Show that error recovery works when writing a bitcode record that
// isn't in any block. Show success by showing fixed bitcode records.
TEST(NaClMungeWriteErrorTests, RecoverWriteRecordOutsideBlock) {
  NaClWriteMunger Munger(ARRAY_ARGS_TERM(BitcodeRecords));
  NaClMungedBitcode &MungedBitcode = Munger.getMungedBitcode();
  NaClRecordVector Values;
  Values.push_back(4);
  NaClBitcodeAbbrevRecord Record(naclbitc::UNABBREV_RECORD,
                                 naclbitc::MODULE_CODE_VERSION,
                                 Values);
  MungedBitcode.addAfter(MungedBitcode.getBaseRecords().size() - 1, Record);

  Munger.setTryToRecoverOnWrite(true);
  EXPECT_TRUE(Munger.runTest());
  EXPECT_EQ(
      "Error (Block unknown): Record outside block: 3: [1, 4]\n"
      "Error (Block unknown): Missing close block.\n"
      "       1: [65535, 8, 2]\n"
      "         1: [65535, 17, 3]\n"
      "           3: [1, 2]\n"
      "           3: [2]\n"
      "           3: [21, 0, 0]\n"
      "         0: [65534]\n"
      "         3: [8, 1, 0, 0, 0]\n"
      "         1: [65535, 12, 2]\n"
      "           3: [1, 1]\n"
      "           3: [10]\n"
      "         0: [65534]\n"
      "       0: [65534]\n"
      "       1: [65535, 4294967295, 3]\n"
      "         3: [1, 4]\n"
      "       0: [65534]\n",
      Munger.getTestResults());
}

} // end of anonymous namespace.
