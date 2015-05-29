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

#include "NaClMungeTest.h"

#include "llvm/Bitcode/NaCl/NaClBitcodeParser.h"
#include "llvm/Bitcode/NaCl/NaClLLVMBitCodes.h"

#include "gtest/gtest.h"

using namespace llvm;

namespace naclmungetest {

// Test list of bitcode records.
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

// Indices to records in bitcode.
const uint64_t VoidTypeIndex = 3; // Index for "@t0 = void".
const uint64_t RetVoidIndex = 9; // return void;
const uint64_t LastExitBlockIndex = 11;

// Expected output when bitcode records are dumped.
const char *ExpectedDumpedBitcode =
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

const char *UnableToContinue =
      "Error: Unable to generate bitcode file due to write errors\n";

const char *NoErrorRecoveryMessages = "";

// Runs write munging tests on BitcodeRecords with the given Edits. It
// then parses the written bitcode.  ErrorMessages is the expected
// error messages logged by the write munging, when no error recovery
// is allowed. ErrorRecoveryMessages are messages, in addition to
// ErrorMessages, when the writer applies error recovery.
void CheckParseEdits(const uint64_t *Edits, size_t EditsSize,
                     std::string ErrorMessages,
                     std::string ErrorRecoveryMessages) {
  NaClParseBitcodeMunger Munger(ARRAY_TERM(BitcodeRecords));
  EXPECT_FALSE(Munger.runTest(Edits, EditsSize, true));
  std::string BadResults(ErrorMessages);
  BadResults.append(UnableToContinue);
  EXPECT_EQ(BadResults, Munger.getTestResults());

  Munger.setTryToRecoverOnWrite(true);
  EXPECT_TRUE(Munger.runTest(Edits, EditsSize, true));
  std::string GoodResults(ErrorMessages);
  GoodResults.append(ErrorRecoveryMessages);
  GoodResults.append("Successful parse!\n");
  EXPECT_EQ(GoodResults, Munger.getTestResults());
}

// Same as CheckParseEdits, but also runs the bitcode dumper on the
// written bitcode records. DumpedBitcode is the expected dumped
// bitcode.
void CheckDumpEdits(const uint64_t *Edits, size_t EditsSize,
                    std::string ErrorMessages,
                    std::string ErrorRecoveryMessages,
                    std::string DumpedBitcode) {
  NaClObjDumpMunger Munger(ARRAY_TERM(BitcodeRecords));
  EXPECT_FALSE(Munger.runTest(Edits, EditsSize));
  std::string BadResults(ErrorMessages);
  BadResults.append(UnableToContinue);
  EXPECT_EQ(BadResults, Munger.getTestResults());

  Munger.setTryToRecoverOnWrite(true);
  EXPECT_TRUE(Munger.runTest(Edits, EditsSize));
  std::string GoodResults(ErrorMessages);
  GoodResults.append(ErrorRecoveryMessages);
  GoodResults.append(DumpedBitcode);
  EXPECT_EQ(GoodResults, Munger.getTestResults());

  // Verify that we can also parse the bitcode.
  CheckParseEdits(Edits, EditsSize, ErrorMessages, ErrorRecoveryMessages);
}

// Same as ExpectedDumpedBitcode, but is just the records dumped by the
// simpler write munger.
const char *ExpectedRecords =
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
    "       0: [65534]\n";

// Same as CheckParseEdits, but run the simpler write munger instead
// of the bitcode parser. Records is the records dumped by the write
// munger. This should be used in cases where the written munged
// records is not valid bitcode.
void CheckWriteEdits(const uint64_t *Edits, size_t EditsSize,
                     std::string ExpectedErrorMessage,
                     std::string ErrorRecoveryMessages,
                     std::string Records) {
  NaClWriteMunger Munger(ARRAY_TERM(BitcodeRecords));
  EXPECT_FALSE(Munger.runTest(Edits, EditsSize));
  std::string BadResults(ExpectedErrorMessage);
  BadResults.append(UnableToContinue);
  EXPECT_EQ(BadResults, Munger.getTestResults());

  Munger.setTryToRecoverOnWrite(true);
  EXPECT_TRUE(Munger.runTest(Edits, EditsSize));
  std::string GoodResults(ExpectedErrorMessage);
  GoodResults.append(ErrorRecoveryMessages);
  GoodResults.append(Records);
  EXPECT_EQ(GoodResults, Munger.getTestResults());
}

// Show that we can dump the bitcode records
TEST(NaClMungeWriteErrorTests, DumpBitcodeRecords) {
  NaClObjDumpMunger Munger(ARRAY_TERM(BitcodeRecords));
  EXPECT_TRUE(Munger.runTest());
  EXPECT_EQ(ExpectedDumpedBitcode, Munger.getTestResults());
}

// Edit to change void type with an illegal abbreviation index.
const uint64_t AbbrevIndex4VoidTypeEdit[] = {
  VoidTypeIndex, NaClMungedBitcode::Replace,
  4, naclbitc::TYPE_CODE_VOID, Terminator,
};

// Show that by default, one can't write a bad abbreviation index.
TEST(NaClMungeWriteErrorTests, CantWriteBadAbbrevIndex) {
  CheckDumpEdits(
      ARRAY(AbbrevIndex4VoidTypeEdit),
      "Error (Block 17): Uses illegal abbreviation index: 4: [2]\n",
      NoErrorRecoveryMessages,
      ExpectedDumpedBitcode);
}

// Show that writing out an illegal abbreviation index, causes the
// parser to fail.
TEST(MyNaClMungerWriteErrorTests, DieOnWriteBadAbbreviationIndex) {
  NaClWriteMunger Munger(ARRAY_TERM(BitcodeRecords));
  Munger.setWriteBadAbbrevIndex(true);
  Munger.setRunAsDeathTest(true);
  EXPECT_DEATH(
      Munger.runTest(ARRAY(AbbrevIndex4VoidTypeEdit)),
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

// Show what happens when we use more local abbreviations than specified in the
// corresponding enclosing block.
TEST(NaClMungeWriteErrorTests, CantWriteTooManyLocalAbbreviations) {
  // Edit to add local abbreviation for "ret void", and then use on that
  // instruction.
  const uint64_t UseLocalRetVoidAbbrevEdits[] = {
    RetVoidIndex, NaClMungedBitcode::AddBefore,
    2, naclbitc::BLK_CODE_DEFINE_ABBREV, 1, 1,
    naclbitc::FUNC_CODE_INST_RET, Terminator,
    RetVoidIndex, NaClMungedBitcode::Replace,
    4, naclbitc::FUNC_CODE_INST_RET, Terminator
  };

  CheckDumpEdits(
      ARRAY(UseLocalRetVoidAbbrevEdits),
      "Error (Block 12): Uses illegal abbreviation index: 4: [10]\n",
      NoErrorRecoveryMessages,
      "       0:0|<65532, 80, 69, 88, 69, 1, 0,|Magic Number: 'PEXE'"
      " (80, 69, 88, 69)\n"
      "          | 8, 0, 17, 0, 4, 0, 2, 0, 0, |PNaCl Version: 2\n"
      "          | 0>                          |\n"
      "      16:0|1: <65535, 8, 2>             |module {  // BlockID = 8\n"
      "      24:0|  1: <65535, 17, 3>          |  types {  // BlockID = 17\n"
      "      32:0|    3: <1, 2>                |    count 2;\n"
      "      34:5|    3: <2>                   |    @t0 = void;\n"
      "      36:4|    3: <21, 0, 0>            |    @t1 = void ();\n"
      "      39:7|  0: <65534>                 |  }\n"
      "      44:0|  3: <8, 1, 0, 0, 0>         |  define external void @f0();\n"
      // Block only specifies 2 bits for abbreviations (i.e. limit = 3).
      "      48:6|  1: <65535, 12, 2>          |  function void @f0() {  \n"
      "          |                             |                   // BlockID"
      " = 12\n"
      "      56:0|    3: <1, 1>                |    blocks 1;\n"
      // Added abbreviation. Defines abbreviation index 4.
      "      58:4|    2: <65533, 1, 1, 10>     |    %a0 = abbrev <10>;\n"
      "          |                             |  %b0:\n"
      // Repaired abbreviation index of 4 (now 3).
      "      60:4|    3: <10>                  |    ret void;\n"
      "      62:2|  0: <65534>                 |  }\n"
      "      64:0|0: <65534>                   |}\n");
}

// Show what happens when there are more enter blocks then exit blocks.
TEST(NaClMungeWriteErrorTests, CantWriteTooManyEnterBlocks) {
  // Remove all records except the first two records in BitcodeRecords.
  const uint64_t Edits[] = {
    2, NaClMungedBitcode::Remove,
    3, NaClMungedBitcode::Remove,
    4, NaClMungedBitcode::Remove,
    5, NaClMungedBitcode::Remove,
    6, NaClMungedBitcode::Remove,
    7, NaClMungedBitcode::Remove,
    8, NaClMungedBitcode::Remove,
    9, NaClMungedBitcode::Remove,
    10, NaClMungedBitcode::Remove,
    11, NaClMungedBitcode::Remove
  };
  CheckDumpEdits(
      ARRAY(Edits),
      "Error (Block 17): Missing close block.\n"
      "Error (Block 8): Missing close block.\n",
      NoErrorRecoveryMessages,
      "       0:0|<65532, 80, 69, 88, 69, 1, 0,|Magic Number: 'PEXE' (80, 69,"
      " 88, 69)\n"
      "          | 8, 0, 17, 0, 4, 0, 2, 0, 0, |PNaCl Version: 2\n"
      "          | 0>                          |\n"
      "      16:0|1: <65535, 8, 2>             |module {  // BlockID = 8\n"
      "      24:0|  1: <65535, 17, 3>          |  types {  // BlockID = 17\n"
      "      32:0|  0: <65534>                 |  }\n"
      "      36:0|0: <65534>                   |}\n");
}

// Show what happens when there are fewer enter blocks than exit
// blocks.
TEST(NaClMungeWriteErrorTests, CantWriteTooManyExitBlocks) {
  // Add two blocks to the end of BitcodeRecords.
  const uint64_t Edits[] = {
    LastExitBlockIndex, NaClMungedBitcode::AddAfter,
    naclbitc::END_BLOCK, naclbitc::BLK_CODE_EXIT, Terminator,
    LastExitBlockIndex, NaClMungedBitcode::AddAfter,
    naclbitc::END_BLOCK, naclbitc::BLK_CODE_EXIT, Terminator
  };
  CheckDumpEdits(
      ARRAY(Edits),
      "Error (Block unknown): Extraneous exit block: 0: [65534]\n",
      "Error (Block unknown): Extraneous exit block: 0: [65534]\n",
      ExpectedDumpedBitcode);
}

// Show that an error occurs when writing a bitcode record that isn't
// in any block.
TEST(NaClMungeWriteErrorTests, CantWriteRecordOutsideBlock) {
  const uint64_t Edit[] = {
    LastExitBlockIndex, NaClMungedBitcode::AddAfter,
    naclbitc::UNABBREV_RECORD, naclbitc::MODULE_CODE_VERSION,  4, Terminator
  };
  std::string Records(ExpectedRecords);
  Records.append(
      "       1: [65535, 4294967295, 3]\n"
      "         3: [1, 4]\n"
      "       0: [65534]\n");
  CheckWriteEdits(
      ARRAY(Edit),
      "Error (Block unknown): Record outside block: 3: [1, 4]\n",
      "Error (Block unknown): Missing close block.\n",
      Records);
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
  NaClWriteMunger Munger(ARRAY_TERM(BitcodeRecords));
  EXPECT_TRUE(Munger.runTest(ARRAY(Edit)));
  EXPECT_EQ(
      "       1: [65535, 8, 32]\n" // Max abbreviation bit limit (32).
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
  CheckDumpEdits(
      ARRAY(Edit),
      "Error (Block unknown): Block index bit limit 33 invalid. Must be in"
      " [2..32]: 1: [65535, 8, 33]\n",
      "",
      "       0:0|<65532, 80, 69, 88, 69, 1, 0,|Magic Number: 'PEXE' (80, 69,"
      " 88, 69)\n"
      "          | 8, 0, 17, 0, 4, 0, 2, 0, 0, |PNaCl Version: 2\n"
      "          | 0>                          |\n"
      // Corrected bitsize from 33 to 32.
      "      16:0|1: <65535, 8, 32>            |module {  // BlockID = 8\n"
      "      24:0|  1: <65535, 17, 3>          |  types {  // BlockID = 17\n"
      "      36:0|    3: <1, 2>                |    count 2;\n"
      "      38:5|    3: <2>                   |    @t0 = void;\n"
      "      40:4|    3: <21, 0, 0>            |    @t1 = void ();\n"
      "      43:7|  0: <65534>                 |  }\n"
      "      48:0|  3: <8, 1, 0, 0, 0>         |  define external void @f0();\n"
      "      56:4|  1: <65535, 12, 2>          |  function void @f0() {  \n"
      "          |                             |                   // BlockID"
      " = 12\n"
      "      68:0|    3: <1, 1>                |    blocks 1;\n"
      "          |                             |  %b0:\n"
      "      70:4|    3: <10>                  |    ret void;\n"
      "      72:2|  0: <65534>                 |  }\n"
      "      76:0|0: <65534>                   |}\n");
}

// Show that we can't write an enter block with a very large block id.
TEST(NaClMungerWriteErrorTests, CantWriteBlockWithLargeBlockID) {
  // Replace initial block enter with value out of range.
  const uint64_t Edit[] = {
    0, NaClMungedBitcode::Replace,
    1, naclbitc::BLK_CODE_ENTER, (uint64_t)1 << 33, 2, Terminator
  };
  CheckWriteEdits(
      ARRAY(Edit),
      "Error (Block unknown): Block id must be <= 4294967295: 1:"
      " [65535, 8589934592, 2]\n",
      "",
      // Note that the maximum block ID is used for recovery.
      "       1: [65535, 4294967295, 2]\n"
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
      "       0: [65534]\n");
}

// Show that we check that the abbreviation actually applies to the
// record associated with that abbreviation. Also shows that we repair
// the problem by applying the default abbreviation instead.
TEST(NaClMungeWriteErrorsTests, TestMismatchedAbbreviation) {
  // Create edits to:
  // 1) Expand the number of abbreviation index bits for the block from 2 to 3.
  // 2) Introduce the incorrect abbreviation for the return instruction.
  //    i.e. [9] instead of [10].
  // 3) Apply the bad abbreviation to record "ret"
  const uint64_t FunctionEnterIndex = 7;
  const uint64_t Edits[] {
    // Upped abbreviation index bits to 3
    FunctionEnterIndex, NaClMungedBitcode::Replace,
        1, naclbitc::BLK_CODE_ENTER, naclbitc::FUNCTION_BLOCK_ID, 3, Terminator,
    // abbrev 4: [9]
    RetVoidIndex, NaClMungedBitcode::AddBefore,
        2, naclbitc::BLK_CODE_DEFINE_ABBREV, 1, 1,
        naclbitc::FUNC_CODE_INST_RET - 1, Terminator,
    // "ret" with bad abbreviation (4).
    RetVoidIndex, NaClMungedBitcode::Replace,
        4, naclbitc::FUNC_CODE_INST_RET, Terminator
        };

  CheckDumpEdits(
      ARRAY(Edits),
      "Error (Block 12): Abbreviation doesn't apply to record: 4: [10]\n",
      "",
      "       0:0|<65532, 80, 69, 88, 69, 1, 0,|Magic Number: 'PEXE' (80, 69,"
      " 88, 69)\n"
      "          | 8, 0, 17, 0, 4, 0, 2, 0, 0, |PNaCl Version: 2\n"
      "          | 0>                          |\n"
      "      16:0|1: <65535, 8, 2>             |module {  // BlockID = 8\n"
      "      24:0|  1: <65535, 17, 3>          |  types {  // BlockID = 17\n"
      "      32:0|    3: <1, 2>                |    count 2;\n"
      "      34:5|    3: <2>                   |    @t0 = void;\n"
      "      36:4|    3: <21, 0, 0>            |    @t1 = void ();\n"
      "      39:7|  0: <65534>                 |  }\n"
      "      44:0|  3: <8, 1, 0, 0, 0>         |  define external void @f0();\n"
      // Upped abbreviation index bits to 3
      "      48:6|  1: <65535, 12, 3>          |  function void @f0() {  \n"
      "          |                             |                   // BlockID"
      " = 12\n"
      "      56:0|    3: <1, 1>                |    blocks 1;\n"
      // added abbrev 4: [9]
      "      58:5|    2: <65533, 1, 1, 9>      |    %a0 = abbrev <9>;\n"
      "          |                             |  %b0:\n"
      // Implicit repair of abbreviation index (from 4 to 3: the default abbrev)
      "      60:6|    3: <10>                  |    ret void;\n"
      "      62:5|  0: <65534>                 |  }\n"
      "      64:0|0: <65534>                   |}\n");
}

// Show that we recognize when an abbreviation definition record is
// malformed.  Also show that we repair the problem by removing the
// definition.
TEST(NaClMungeWriteErrorsTests, TestWritingMalformedAbbreviation) {
  // Create edits to:
  // 1) Expand the number of abbreviation index bits for the block from 2 to 3.
  // 2) Leave out the "literal" operand encoding out.
  const uint64_t FunctionEnterIndex = 7;
  const uint64_t Edits[] {
    FunctionEnterIndex, NaClMungedBitcode::Replace,   // Set Abbrev bits = 3
        1, naclbitc::BLK_CODE_ENTER, naclbitc::FUNCTION_BLOCK_ID, 3, Terminator,
    RetVoidIndex, NaClMungedBitcode::AddBefore,
        // Bad abbreviation! Intentionally leave out "literal" operand: 1
        2, naclbitc::BLK_CODE_DEFINE_ABBREV, 1, // 1,
        naclbitc::FUNC_CODE_INST_RET, Terminator,
        };

  CheckDumpEdits(
      ARRAY(Edits),
      "Error (Block 12): Bad abbreviation operand encoding 10:"
      " 2: [65533, 1, 10]\n",
      "",
      "       0:0|<65532, 80, 69, 88, 69, 1, 0,|Magic Number: 'PEXE' (80, 69,"
      " 88, 69)\n"
      "          | 8, 0, 17, 0, 4, 0, 2, 0, 0, |PNaCl Version: 2\n"
      "          | 0>                          |\n"
      "      16:0|1: <65535, 8, 2>             |module {  // BlockID = 8\n"
      "      24:0|  1: <65535, 17, 3>          |  types {  // BlockID = 17\n"
      "      32:0|    3: <1, 2>                |    count 2;\n"
      "      34:5|    3: <2>                   |    @t0 = void;\n"
      "      36:4|    3: <21, 0, 0>            |    @t1 = void ();\n"
      "      39:7|  0: <65534>                 |  }\n"
      "      44:0|  3: <8, 1, 0, 0, 0>         |  define external void @f0();\n"
      // Edit to change number of abbrev bits to 3.
      "      48:6|  1: <65535, 12, 3>          |  function void @f0() {  \n"
      "          |                             |                   // BlockID"
      " = 12\n"
      "      56:0|    3: <1, 1>                |    blocks 1;\n"
      "          |                             |  %b0:\n"
      "      58:5|    3: <10>                  |    ret void;\n"
      "      60:4|  0: <65534>                 |  }\n"
      "      64:0|0: <65534>                   |}\n");
}

// Show how we deal with additional abbreviations defined for a block,
// once a bad abbreviation definition record is found. That is, we
// remove all succeeding abbreviations definitions for that block. In
// addition, any record refering to a remove abbreviation is changed
// to use the default abbreviation.
TEST(NaClMungedWriteErrorTests, TestRemovingAbbrevWithMultAbbrevs) {
  NaClWriteMunger Munger(ARRAY_TERM(BitcodeRecords));
  const uint64_t FunctionEnterIndex = 7;
  const uint64_t Edits[] {
    FunctionEnterIndex, NaClMungedBitcode::Replace,   // Set Abbrev bits = 3
        1, naclbitc::BLK_CODE_ENTER, naclbitc::FUNCTION_BLOCK_ID, 3, Terminator,
    RetVoidIndex, NaClMungedBitcode::AddBefore,  // bad abbreviation!
        2, naclbitc::BLK_CODE_DEFINE_ABBREV, 1, // 1,
        naclbitc::FUNC_CODE_INST_RET - 1, Terminator,
    RetVoidIndex, NaClMungedBitcode::AddBefore,  // good abbreviation to ignore.
        2, naclbitc::BLK_CODE_DEFINE_ABBREV, 1, 1,
        naclbitc::FUNC_CODE_INST_RET, Terminator,
    RetVoidIndex, NaClMungedBitcode::Replace,  // reference to good abreviation.
        5, naclbitc::FUNC_CODE_INST_RET, Terminator
        };

  CheckDumpEdits(
      ARRAY(Edits),
      "Error (Block 12): Bad abbreviation operand encoding 9:"
      " 2: [65533, 1, 9]\n",
      "Error (Block 12): Ignoring abbreviation: 2: [65533, 1, 1, 10]\n"
      "Error (Block 12): Uses illegal abbreviation index: 5: [10]\n",
      "       0:0|<65532, 80, 69, 88, 69, 1, 0,|Magic Number: 'PEXE' (80, 69,"
      " 88, 69)\n"
      "          | 8, 0, 17, 0, 4, 0, 2, 0, 0, |PNaCl Version: 2\n"
      "          | 0>                          |\n"
      "      16:0|1: <65535, 8, 2>             |module {  // BlockID = 8\n"
      "      24:0|  1: <65535, 17, 3>          |  types {  // BlockID = 17\n"
      "      32:0|    3: <1, 2>                |    count 2;\n"
      "      34:5|    3: <2>                   |    @t0 = void;\n"
      "      36:4|    3: <21, 0, 0>            |    @t1 = void ();\n"
      "      39:7|  0: <65534>                 |  }\n"
      "      44:0|  3: <8, 1, 0, 0, 0>         |  define external void @f0();\n"
      // Edit to change number of abbrev bits to 3.
      "      48:6|  1: <65535, 12, 3>          |  function void @f0() {  \n"
      "          |                             |                   // BlockID"
      " = 12\n"
      "      56:0|    3: <1, 1>                |    blocks 1;\n"
      "          |                             |  %b0:\n"
      "      58:5|    3: <10>                  |    ret void;\n"
      "      60:4|  0: <65534>                 |  }\n"
      "      64:0|0: <65534>                   |}\n");
}

// Show that inserting an abbreviation with a bad fixed width is dealt with.
TEST(NaClMungeWriteErrorTests, InvalidFixedAbbreviationSize) {
  // Insert bad abbreviation Fixed(36) into type block.
  assert(36 > naclbitc::MaxAbbrevWidth);
  const uint64_t Edit[] = {
    VoidTypeIndex, NaClMungedBitcode::AddBefore,
    naclbitc::DEFINE_ABBREV, naclbitc::BLK_CODE_DEFINE_ABBREV, 1,
    0, NaClBitCodeAbbrevOp::Fixed, 36, Terminator
  };
  CheckDumpEdits(
      ARRAY(Edit),
      "Error (Block 17): Invalid abbreviation Fixed(36) in: 2: [65533, 1, 0,"
      " 1, 36]\n",
      NoErrorRecoveryMessages,
      ExpectedDumpedBitcode);
}

// Show that inserting an abbreviation with a bad vbr width is dealt with.
TEST(NaClMungeWriteErrorTests, InvalidVbrAbbreviationSize) {
  // Insert bad abbreviation Vbr(36) into type block.
  assert(36 > naclbitc::MaxAbbrevWidth);
  const uint64_t Edit[] = {
    VoidTypeIndex, NaClMungedBitcode::AddBefore,
    naclbitc::DEFINE_ABBREV, naclbitc::BLK_CODE_DEFINE_ABBREV, 1,
    0, NaClBitCodeAbbrevOp::VBR, 36, Terminator
  };
  CheckDumpEdits(
      ARRAY(Edit),
      "Error (Block 17): Invalid abbreviation VBR(36) in: 2: [65533, 1, 0,"
      " 2, 36]\n",
      NoErrorRecoveryMessages,
      ExpectedDumpedBitcode);
}

// Show that the array operator can't appear last.
TEST(NaClMungeWriteErrorTests, InvalidArrayAbbreviationLast) {
  const uint64_t Edit[] = {
    VoidTypeIndex, NaClMungedBitcode::AddBefore,
    naclbitc::DEFINE_ABBREV, naclbitc::BLK_CODE_DEFINE_ABBREV, 1,
    0, NaClBitCodeAbbrevOp::Array, Terminator
  };
  CheckDumpEdits(
      ARRAY(Edit),
      "Error (Block 17): Array abbreviation must be second to last: 2: [65533,"
      " 1, 0, 3]\n",
      NoErrorRecoveryMessages,
      ExpectedDumpedBitcode);
}

// Show that the array operator can't appear before the second to last
// operand.
TEST(NaClMungeWriteErrorTests, InvalidArrayAbbreviationTooEarly) {
  const uint64_t Edit[] = {
    VoidTypeIndex, NaClMungedBitcode::AddBefore,
    naclbitc::DEFINE_ABBREV, naclbitc::BLK_CODE_DEFINE_ABBREV, 3,
    0, NaClBitCodeAbbrevOp::Array,  // array
    1, 15,                          // lit(15)
    1, 10,                          // lit(10)
    Terminator
  };
  CheckDumpEdits(
      ARRAY(Edit),
      "Error (Block 17): Array abbreviation must be second to last: 2: [65533,"
      " 3, 0, 3, 1, 15, 1, 10]\n",
      NoErrorRecoveryMessages,
      ExpectedDumpedBitcode);
}

// Show that the array operator can't appear as last two operators.
TEST(NaClMungeWriteErrorTests, InvalidArrayAbbreviationLastTwo) {
  const uint64_t Edit[] = {
    VoidTypeIndex, NaClMungedBitcode::AddBefore,
    naclbitc::DEFINE_ABBREV, naclbitc::BLK_CODE_DEFINE_ABBREV, 2,
    0, NaClBitCodeAbbrevOp::Array,  // array
    0, NaClBitCodeAbbrevOp::Array,  // array
    Terminator
  };
  CheckDumpEdits(
      ARRAY(Edit),
      "Error (Block 17): Array abbreviation must be second to last: 2: [65533,"
      " 2, 0, 3, 0, 3]\n",
      NoErrorRecoveryMessages,
      ExpectedDumpedBitcode);
}

// Show what happens when an abbreviation is specified to only contain
// one operator, but is then followed with more than one operator.
TEST(NaClMungeWriteErrorTests, SpecifiesTooFewOperands) {
  const uint64_t Edit[] = {
    VoidTypeIndex, NaClMungedBitcode::AddBefore,
    // Note: 1 at end of next line specified that the abbreviation
    // should only have one operator.
    naclbitc::DEFINE_ABBREV, naclbitc::BLK_CODE_DEFINE_ABBREV, 1,
    1, 10,                          // lit(10)
    1, 15,                          // lit(15)
    Terminator
  };
  CheckDumpEdits(
      ARRAY(Edit),
      "Error (Block 17): Error: Too many values for number of operands (1):"
      " 2: [65533, 1, 1, 10, 1, 15]\n",
      NoErrorRecoveryMessages,
      ExpectedDumpedBitcode);
}

// Show that the code checks if specifies too many operands for an
// abbreviation, based on the record size.
TEST(NaClMungeWriteErrorTests, SpecifiesTooManyOperands) {
  // Insert bad abbreviation Vbr(36) into type block.
  const uint64_t Edit[] = {
    VoidTypeIndex, NaClMungedBitcode::AddBefore,
    naclbitc::DEFINE_ABBREV, naclbitc::BLK_CODE_DEFINE_ABBREV, 3,
    1, 10,                          // lit(10)
    1, 15,                          // lit(15)
    Terminator
  };
  CheckDumpEdits(
      ARRAY(Edit),
      "Error (Block 17): Malformed abbreviation found: 2: [65533, 3, 1, 10,"
      " 1, 15]\n",
      NoErrorRecoveryMessages,
      ExpectedDumpedBitcode);
}

} // end of namespace naclmungetest
