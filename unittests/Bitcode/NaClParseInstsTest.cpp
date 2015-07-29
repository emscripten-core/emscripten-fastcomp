//===- llvm/unittest/Bitcode/NaClParseInstsTest.cpp ----------------------===//
//     Tests parser for PNaCl bitcode instructions.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// Tests record errors in the function block when parsing PNaCl bitcode.

// TODO(kschimpf) Add more tests.


#include "NaClMungeTest.h"

#include "llvm/Bitcode/NaCl/NaClLLVMBitCodes.h"

using namespace llvm;

namespace naclmungetest {

// Note: alignment stored as 0 or log2(Alignment)+1.
uint64_t getEncAlignPower(unsigned Power) {
  return Power + 1;
}
uint64_t getEncAlignZero() { return 0; }

/// Test how we report a call arg that refers to nonexistent call argument
TEST(NaClParseInstsTest, NonexistantCallArg) {
  const uint64_t BitcodeRecords[] = {
    1, naclbitc::BLK_CODE_ENTER, naclbitc::MODULE_BLOCK_ID, 2, Terminator,
    1, naclbitc::BLK_CODE_ENTER, naclbitc::TYPE_BLOCK_ID_NEW, 2, Terminator,
    3, naclbitc::TYPE_CODE_NUMENTRY, 3, Terminator,
    3, naclbitc::TYPE_CODE_INTEGER, 32, Terminator,
    3, naclbitc::TYPE_CODE_VOID, Terminator,
    3, naclbitc::TYPE_CODE_FUNCTION, 0, 1, 0, 0, Terminator,
    0, naclbitc::BLK_CODE_EXIT, Terminator,
    3, naclbitc::MODULE_CODE_FUNCTION, 2, 0, 1, 0, Terminator,
    3, naclbitc::MODULE_CODE_FUNCTION, 2, 0, 0, 0, Terminator,
    1, naclbitc::BLK_CODE_ENTER, naclbitc::FUNCTION_BLOCK_ID, 2, Terminator,
    3, naclbitc::FUNC_CODE_DECLAREBLOCKS, 1, Terminator,
    // Note: 100 is a bad value index in next line.
    3, naclbitc::FUNC_CODE_INST_CALL, 0, 4, 2, 100, Terminator,
    3, naclbitc::FUNC_CODE_INST_RET, Terminator,
    0, naclbitc::BLK_CODE_EXIT, Terminator,
    0, naclbitc::BLK_CODE_EXIT, Terminator
  };

  // Show text of base input.
  NaClObjDumpMunger DumpMunger(ARRAY_TERM(BitcodeRecords));
  EXPECT_FALSE(DumpMunger.runTest());
  EXPECT_EQ(
      "       0:0|<65532, 80, 69, 88, 69, 1, 0,|Magic Number: 'PEXE' (80, 69, "
      "88, 69)\n"
      "          | 8, 0, 17, 0, 4, 0, 2, 0, 0, |PNaCl Version: 2\n"
      "          | 0>                          |\n"
      "      16:0|1: <65535, 8, 2>             |module {  // BlockID = 8\n"
      "      24:0|  1: <65535, 17, 2>          |  types {  // BlockID = 17\n"
      "      32:0|    3: <1, 3>                |    count 3;\n"
      "      34:4|    3: <7, 32>               |    @t0 = i32;\n"
      "      37:6|    3: <2>                   |    @t1 = void;\n"
      "      39:4|    3: <21, 0, 1, 0, 0>      |    @t2 = void (i32, i32);\n"
      "      44:2|  0: <65534>                 |  }\n"
      "      48:0|  3: <8, 2, 0, 1, 0>         |  declare external void @f0(i32"
      ", i32);\n"
      "      52:6|  3: <8, 2, 0, 0, 0>         |  define external void @f1(i32,"
      " i32);\n"
      "      57:4|  1: <65535, 12, 2>          |  function void @f1(i32 %p0, "
      "i32 %p1) {\n"
      "          |                             |                    // BlockID "
      "= 12\n"
      "      64:0|    3: <1, 1>                |    blocks 1;\n"
      "          |                             |  %b0:\n"
      "      66:4|    3: <34, 0, 4, 2, 100>    |    call void @f0(i32 %p0, i32"
      " @f0);\n"
      "Error(66:4): Invalid relative value id: 100 (Must be <= 4)\n"
      "      72:6|    3: <10>                  |    ret void;\n"
      "      74:4|  0: <65534>                 |  }\n"
      "      76:0|0: <65534>                   |}\n",
      DumpMunger.getTestResults());

  NaClParseBitcodeMunger Munger(ARRAY_TERM(BitcodeRecords));
  EXPECT_FALSE(Munger.runTest(true));
  EXPECT_EQ(
      "Error(72:6): Invalid call argument: Index 1\n"
      "Error: Invalid value in record\n",
      Munger.getTestResults());
}

/// Test how we recognize alignments in alloca instructions.
TEST(NaClParseInstsTests, BadAllocaAlignment) {
  const uint64_t BitcodeRecords[] = {
    1, naclbitc::BLK_CODE_ENTER, naclbitc::MODULE_BLOCK_ID, 2, Terminator,
    1, naclbitc::BLK_CODE_ENTER, naclbitc::TYPE_BLOCK_ID_NEW, 2, Terminator,
    3, naclbitc::TYPE_CODE_NUMENTRY, 4, Terminator,
    3, naclbitc::TYPE_CODE_INTEGER, 32, Terminator,
    3, naclbitc::TYPE_CODE_VOID, Terminator,
    3, naclbitc::TYPE_CODE_FUNCTION, 0, 1, 0, Terminator,
    3, naclbitc::TYPE_CODE_INTEGER, 8, Terminator,
    0, naclbitc::BLK_CODE_EXIT, Terminator,
    3, naclbitc::MODULE_CODE_FUNCTION, 2, 0, 0, 0, Terminator,
    1, naclbitc::BLK_CODE_ENTER, naclbitc::FUNCTION_BLOCK_ID, 2, Terminator,
    3, naclbitc::FUNC_CODE_DECLAREBLOCKS, 1, Terminator,
    3, naclbitc::FUNC_CODE_INST_ALLOCA, 1, getEncAlignPower(0), Terminator,
    3, naclbitc::FUNC_CODE_INST_RET, Terminator,
    0, naclbitc::BLK_CODE_EXIT, Terminator,
    0, naclbitc::BLK_CODE_EXIT, Terminator
  };

  const uint64_t ReplaceIndex = 11; // index for FUNC_CODE_INST_ALLOCA

  // Show text when alignment is 1.
  NaClObjDumpMunger DumpMunger(ARRAY_TERM(BitcodeRecords));
  EXPECT_TRUE(DumpMunger.runTest());
  EXPECT_EQ(
      "       0:0|<65532, 80, 69, 88, 69, 1, 0,|Magic Number: 'PEXE' (80, 69, "
      "88, 69)\n"
      "          | 8, 0, 17, 0, 4, 0, 2, 0, 0, |PNaCl Version: 2\n"
      "          | 0>                          |\n"
      "      16:0|1: <65535, 8, 2>             |module {  // BlockID = 8\n"
      "      24:0|  1: <65535, 17, 2>          |  types {  // BlockID = 17\n"
      "      32:0|    3: <1, 4>                |    count 4;\n"
      "      34:4|    3: <7, 32>               |    @t0 = i32;\n"
      "      37:6|    3: <2>                   |    @t1 = void;\n"
      "      39:4|    3: <21, 0, 1, 0>         |    @t2 = void (i32);\n"
      "      43:4|    3: <7, 8>                |    @t3 = i8;\n"
      "      46:0|  0: <65534>                 |  }\n"
      "      48:0|  3: <8, 2, 0, 0, 0>         |  define external void @f0(i32"
      ");\n"
      "      52:6|  1: <65535, 12, 2>          |  function void @f0(i32 %p0) {"
      "  \n"
      "          |                             |                   // BlockID "
      "= 12\n"
      "      60:0|    3: <1, 1>                |    blocks 1;\n"
      "          |                             |  %b0:\n"
      "      62:4|    3: <19, 1, 1>            |    %v0 = alloca i8, i32 %p0, "
      "align 1;\n"
      "      65:6|    3: <10>                  |    ret void;\n"
      "      67:4|  0: <65534>                 |  }\n"
      "      68:0|0: <65534>                   |}\n",
      DumpMunger.getTestResults());
  NaClParseBitcodeMunger Munger(ARRAY_TERM(BitcodeRecords));
  EXPECT_TRUE(Munger.runTest(true));

  // Show what happens when changing alignment to 0.
  const uint64_t Align0[] = {
    ReplaceIndex, NaClMungedBitcode::Replace,
    3, naclbitc::FUNC_CODE_INST_ALLOCA, 1, getEncAlignZero(), Terminator,
  };
  EXPECT_TRUE(Munger.runTest(ARRAY(Align0), true));
  EXPECT_TRUE(DumpMunger.runTestForAssembly(ARRAY(Align0)));
  EXPECT_EQ(
      "    %v0 = alloca i8, i32 %p0, align 0;\n",
      DumpMunger.getLinesWithSubstring("alloca"));

  // Show what happens when changing alignment to 2**30.
  const uint64_t Align30[] = {
    ReplaceIndex, NaClMungedBitcode::Replace,
    3, naclbitc::FUNC_CODE_INST_ALLOCA, 1, getEncAlignPower(30), Terminator,
  };
  EXPECT_FALSE(Munger.runTest(ARRAY(Align30), true));
  EXPECT_EQ(
      "Error(65:6): Alignment can't be greater than 2**29. Found: 2**30\n"
      "Error: Invalid value in record\n",
      Munger.getTestResults());
  EXPECT_FALSE(DumpMunger.runTestForAssembly(ARRAY(Align30)));
  EXPECT_EQ(
      "    %v0 = alloca i8, i32 %p0, align 0;\n",
      DumpMunger.getLinesWithSubstring("alloca"));
  EXPECT_EQ(
      "Error(62:4): Alignment can't be greater than 2**29. Found: 2**30\n",
      DumpMunger.getLinesWithSubstring("Error"));

  // Show what happens when changing alignment to 2**29.
  const uint64_t Align29[] = {
    ReplaceIndex, NaClMungedBitcode::Replace,
    3, naclbitc::FUNC_CODE_INST_ALLOCA, 1, getEncAlignPower(29), Terminator,
  };
  EXPECT_TRUE(Munger.runTest(ARRAY(Align29), true));
  EXPECT_EQ(
      "Successful parse!\n",
      Munger.getTestResults());
  EXPECT_TRUE(DumpMunger.runTestForAssembly(ARRAY(Align29)));
  EXPECT_EQ(
      "    %v0 = alloca i8, i32 %p0, align 536870912;\n",
      DumpMunger.getLinesWithSubstring("alloca"));
}

// Test how we recognize alignments in load instructions.
TEST(NaClParseInstsTests, BadLoadAlignment) {
  const uint64_t BitcodeRecords[] = {
    1, naclbitc::BLK_CODE_ENTER, naclbitc::MODULE_BLOCK_ID, 2, Terminator,
    1, naclbitc::BLK_CODE_ENTER, naclbitc::TYPE_BLOCK_ID_NEW, 2, Terminator,
    3, naclbitc::TYPE_CODE_NUMENTRY, 2, Terminator,
    3, naclbitc::TYPE_CODE_INTEGER, 32, Terminator,
    3, naclbitc::TYPE_CODE_FUNCTION, 0, 0, 0, Terminator,
    0, naclbitc::BLK_CODE_EXIT, Terminator,
    3, naclbitc::MODULE_CODE_FUNCTION, 1, 0, 0, 0, Terminator,
    1, naclbitc::BLK_CODE_ENTER, naclbitc::FUNCTION_BLOCK_ID, 2, Terminator,
    3, naclbitc::FUNC_CODE_DECLAREBLOCKS, 1, Terminator,
    3, naclbitc::FUNC_CODE_INST_LOAD, 1, getEncAlignPower(0), 0, Terminator,
    3, naclbitc::FUNC_CODE_INST_RET,  1, Terminator,
    0, naclbitc::BLK_CODE_EXIT, Terminator,
    0, naclbitc::BLK_CODE_EXIT, Terminator
  };

  const uint64_t ReplaceIndex = 9; // index for FUNC_CODE_INST_LOAD

  // Show text when alignment is 1.
  NaClObjDumpMunger DumpMunger(ARRAY_TERM(BitcodeRecords));
  EXPECT_TRUE(DumpMunger.runTest());
  EXPECT_EQ(
      "       0:0|<65532, 80, 69, 88, 69, 1, 0,|Magic Number: 'PEXE' (80, 69, "
      "88, 69)\n"
      "          | 8, 0, 17, 0, 4, 0, 2, 0, 0, |PNaCl Version: 2\n"
      "          | 0>                          |\n"
      "      16:0|1: <65535, 8, 2>             |module {  // BlockID = 8\n"
      "      24:0|  1: <65535, 17, 2>          |  types {  // BlockID = 17\n"
      "      32:0|    3: <1, 2>                |    count 2;\n"
      "      34:4|    3: <7, 32>               |    @t0 = i32;\n"
      "      37:6|    3: <21, 0, 0, 0>         |    @t1 = i32 (i32);\n"
      "      41:6|  0: <65534>                 |  }\n"
      "      44:0|  3: <8, 1, 0, 0, 0>         |  define external i32 @f0(i32"
      ");\n"
      "      48:6|  1: <65535, 12, 2>          |  function i32 @f0(i32 %p0) {"
      "  \n"
      "          |                             |                   // BlockID "
      "= 12\n"
      "      56:0|    3: <1, 1>                |    blocks 1;\n"
      "          |                             |  %b0:\n"
      "      58:4|    3: <20, 1, 1, 0>         |    %v0 = load i32* %p0, "
      "align 1;\n"
      "      62:4|    3: <10, 1>               |    ret i32 %v0;\n"
      "      65:0|  0: <65534>                 |  }\n"
      "      68:0|0: <65534>                   |}\n",
      DumpMunger.getTestResults());
  NaClParseBitcodeMunger Munger(ARRAY_TERM(BitcodeRecords));
  EXPECT_TRUE(Munger.runTest(true));

  // Show what happens when changing alignment to 0.
  const uint64_t Align0[] = {
    ReplaceIndex, NaClMungedBitcode::Replace,
    3, naclbitc::FUNC_CODE_INST_LOAD, 1, getEncAlignZero(), 0, Terminator,
  };
  // Note: Correct alignment is not checked by Munger (i.e. the PNaCl
  // bitcode reader). It is checked later by the PNaCl ABI checker in
  // pnacl-llc. On the other hand, the DumpMunger checks alignment for
  // loads while parsing.
  EXPECT_TRUE(Munger.runTest(ARRAY(Align0), true));
  EXPECT_FALSE(DumpMunger.runTestForAssembly(ARRAY(Align0)));
  EXPECT_EQ(
      "    %v0 = load i32* %p0, align 0;\n"
      "Error(58:4): load: Illegal alignment for i32. Expects: 1\n",
      DumpMunger.getLinesWithSubstring("load"));

  // Show what happens when changing alignment to 4.
  const uint64_t Align4[] = {
    ReplaceIndex, NaClMungedBitcode::Replace,
    3, naclbitc::FUNC_CODE_INST_LOAD, 1, getEncAlignPower(2), 0, Terminator,
  };
  EXPECT_TRUE(Munger.runTest(ARRAY(Align4), true));
  EXPECT_FALSE(DumpMunger.runTestForAssembly(ARRAY(Align4)));
  EXPECT_EQ(
      "    %v0 = load i32* %p0, align 4;\n"
      "Error(58:4): load: Illegal alignment for i32. Expects: 1\n",
      DumpMunger.getLinesWithSubstring("load"));

  // Show what happens when changing alignment to 2**29.
  const uint64_t Align29[] = {
    ReplaceIndex, NaClMungedBitcode::Replace,
    3, naclbitc::FUNC_CODE_INST_LOAD, 1, getEncAlignPower(29), 0, Terminator,
  };
  EXPECT_TRUE(Munger.runTest(ARRAY(Align29), true));
  EXPECT_FALSE(DumpMunger.runTestForAssembly(ARRAY(Align29)));
  EXPECT_EQ(
      "    %v0 = load i32* %p0, align 536870912;\n"
      "Error(58:4): load: Illegal alignment for i32. Expects: 1\n",
      DumpMunger.getLinesWithSubstring("load"));

  // Show what happens when changing alignment to 2**30.
  const uint64_t Align30[] = {
    ReplaceIndex, NaClMungedBitcode::Replace,
    3, naclbitc::FUNC_CODE_INST_LOAD, 1, getEncAlignPower(30), 0, Terminator,
  };
  EXPECT_FALSE(Munger.runTest(ARRAY(Align30), true));
  EXPECT_EQ(
      "Error(62:4): Alignment can't be greater than 2**29. Found: 2**30\n"
      "Error: Invalid value in record\n",
      Munger.getTestResults());
  EXPECT_FALSE(DumpMunger.runTestForAssembly(ARRAY(Align30)));
  EXPECT_EQ(
      "    %v0 = load i32* %p0, align 0;\n"
      "Error(58:4): load: Illegal alignment for i32. Expects: 1\n",
      DumpMunger.getLinesWithSubstring("load"));
}

// Test how we recognize alignments in store instructions.
TEST(NaClParseInstsTests, BadStoreAlignment) {
  const uint64_t BitcodeRecords[] = {
    1, naclbitc::BLK_CODE_ENTER, naclbitc::MODULE_BLOCK_ID, 2, Terminator,
    1, naclbitc::BLK_CODE_ENTER, naclbitc::TYPE_BLOCK_ID_NEW, 2, Terminator,
    3, naclbitc::TYPE_CODE_NUMENTRY, 3, Terminator,
    3, naclbitc::TYPE_CODE_FLOAT, Terminator,
    3, naclbitc::TYPE_CODE_INTEGER, 32, Terminator,
    3, naclbitc::TYPE_CODE_FUNCTION, 0, 0, 1, 0, Terminator,
    0, naclbitc::BLK_CODE_EXIT, Terminator,
    3, naclbitc::MODULE_CODE_FUNCTION, 2, 0, 0, 0, Terminator,
    1, naclbitc::BLK_CODE_ENTER, naclbitc::FUNCTION_BLOCK_ID, 2, Terminator,
    3, naclbitc::FUNC_CODE_DECLAREBLOCKS, 1, Terminator,
    3, naclbitc::FUNC_CODE_INST_STORE, 2, 1, getEncAlignPower(0), Terminator,
    3, naclbitc::FUNC_CODE_INST_RET, 1, Terminator,
    0, naclbitc::BLK_CODE_EXIT, Terminator,
    0, naclbitc::BLK_CODE_EXIT, Terminator
  };

  const uint64_t ReplaceIndex = 10; // index for FUNC_CODE_INST_STORE

  // Show text when alignment is 1.
  NaClObjDumpMunger DumpMunger(ARRAY_TERM(BitcodeRecords));
  EXPECT_TRUE(DumpMunger.runTest());
  EXPECT_EQ(
      "       0:0|<65532, 80, 69, 88, 69, 1, 0,|Magic Number: 'PEXE' (80, 69, "
      "88, 69)\n"
      "          | 8, 0, 17, 0, 4, 0, 2, 0, 0, |PNaCl Version: 2\n"
      "          | 0>                          |\n"
      "      16:0|1: <65535, 8, 2>             |module {  // BlockID = 8\n"
      "      24:0|  1: <65535, 17, 2>          |  types {  // BlockID = 17\n"
      "      32:0|    3: <1, 3>                |    count 3;\n"
      "      34:4|    3: <3>                   |    @t0 = float;\n"
      "      36:2|    3: <7, 32>               |    @t1 = i32;\n"
      "      39:4|    3: <21, 0, 0, 1, 0>      |    @t2 = float (i32, float);\n"
      "      44:2|  0: <65534>                 |  }\n"
      "      48:0|  3: <8, 2, 0, 0, 0>         |  define external \n"
      "          |                             |      float @f0(i32, float);\n"
      "      52:6|  1: <65535, 12, 2>          |  function \n"
      "          |                             |      float @f0(i32 %p0, float "
      "%p1) {  \n"
      "          |                             |                   // BlockID "
      "= 12\n"
      "      60:0|    3: <1, 1>                |    blocks 1;\n"
      "          |                             |  %b0:\n"
      "      62:4|    3: <24, 2, 1, 1>         |    store float %p1, float* "
      "%p0, \n"
      "          |                             |        align 1;\n"
      "      66:4|    3: <10, 1>               |    ret float %p1;\n"
      "      69:0|  0: <65534>                 |  }\n"
      "      72:0|0: <65534>                   |}\n",
      DumpMunger.getTestResults());
  NaClParseBitcodeMunger Munger(ARRAY_TERM(BitcodeRecords));
  EXPECT_TRUE(Munger.runTest(true));

  // Show what happens when changing alignment to 0.
  const uint64_t Align0[] = {
    ReplaceIndex, NaClMungedBitcode::Replace,
    3, naclbitc::FUNC_CODE_INST_STORE, 2, 1, getEncAlignZero(), Terminator,
  };
  // Note: Correct alignment is not checked by Munger (i.e. the PNaCl
  // bitcode reader). It is checked later by the PNaCl ABI checker in
  // pnacl-llc. On the other hand, the DumpMunger checks alignment for
  // stores while parsing.
  EXPECT_TRUE(Munger.runTest(ARRAY(Align0), true));
  EXPECT_FALSE(DumpMunger.runTestForAssembly(ARRAY(Align0)));
  EXPECT_EQ(
      "    store float %p1, float* %p0, align 0;\n"
      "Error(62:4): store: Illegal alignment for float. Expects: 1 or 4\n",
      DumpMunger.getLinesWithSubstring("store"));

  // Show what happens when changing alignment to 4.
  const uint64_t Align4[] = {
    ReplaceIndex, NaClMungedBitcode::Replace,
    3, naclbitc::FUNC_CODE_INST_STORE, 2, 1, getEncAlignPower(2), Terminator,
  };
  EXPECT_TRUE(Munger.runTest(ARRAY(Align4), true));
  EXPECT_TRUE(DumpMunger.runTestForAssembly(ARRAY(Align4)));

  // Show what happens when changing alignment to 8.
  const uint64_t Align8[] = {
    ReplaceIndex, NaClMungedBitcode::Replace,
    3, naclbitc::FUNC_CODE_INST_STORE, 2, 1, getEncAlignPower(3), Terminator,
  };
  EXPECT_TRUE(Munger.runTest(ARRAY(Align8), true));
  EXPECT_FALSE(DumpMunger.runTestForAssembly(ARRAY(Align8)));
  EXPECT_EQ(
      "    store float %p1, float* %p0, align 8;\n"
      "Error(62:4): store: Illegal alignment for float. Expects: 1 or 4\n",
      DumpMunger.getLinesWithSubstring("store"));

  // Show what happens when changing alignment to 2**29.
  const uint64_t Align29[] = {
    ReplaceIndex, NaClMungedBitcode::Replace,
    3, naclbitc::FUNC_CODE_INST_STORE, 2, 1, getEncAlignPower(29), Terminator,
  };
  EXPECT_TRUE(Munger.runTest(ARRAY(Align29), true));
  EXPECT_FALSE(DumpMunger.runTestForAssembly(ARRAY(Align29)));
  EXPECT_EQ(
      "    store float %p1, float* %p0, align 536870912;\n"
      "Error(62:4): store: Illegal alignment for float. Expects: 1 or 4\n",
      DumpMunger.getLinesWithSubstring("store"));

  // Show what happens when changing alignment to 2**30.
  const uint64_t Align30[] = {
    ReplaceIndex, NaClMungedBitcode::Replace,
    3, naclbitc::FUNC_CODE_INST_STORE, 2, 1, getEncAlignPower(30), Terminator,
  };
  EXPECT_FALSE(Munger.runTest(ARRAY(Align30), true));
  EXPECT_EQ(
      "Error(66:4): Alignment can't be greater than 2**29. Found: 2**30\n"
      "Error: Invalid value in record\n",
      Munger.getTestResults());
  EXPECT_FALSE(DumpMunger.runTestForAssembly(ARRAY(Align30)));
  EXPECT_EQ(
      "    store float %p1, float* %p0, align 0;\n"
      "Error(62:4): store: Illegal alignment for float. Expects: 1 or 4\n",
      DumpMunger.getLinesWithSubstring("store"));
}

} // end of namespace naclmungetest
