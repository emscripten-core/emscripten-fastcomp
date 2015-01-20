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

#include "llvm/ADT/STLExtras.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeMunge.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeParser.h"
#include "llvm/Bitcode/NaCl/NaClLLVMBitCodes.h"

#include "gtest/gtest.h"

using namespace llvm;

namespace {

static const uint64_t Terminator = 0x5768798008978675LL;

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
  NaClObjDumpMunger DumpMunger(BitcodeRecords,
                               array_lengthof(BitcodeRecords), Terminator);
  EXPECT_FALSE(DumpMunger.runTestForAssembly("Nonexistant call arg"));
  EXPECT_EQ(
      "module {  // BlockID = 8\n"
      "  types {  // BlockID = 17\n"
      "    count 3;\n"
      "    @t0 = i32;\n"
      "    @t1 = void;\n"
      "    @t2 = void (i32, i32);\n"
      "  }\n"
      "  declare external void @f0(i32, i32);\n"
      "  define external void @f1(i32, i32);\n"
      "  function void @f1(i32 %p0, i32 %p1) {  // BlockID = 12\n"
      "    blocks 1;\n"
      "  %b0:\n"
      "    call void @f0(i32 %p0, i32 @f0);\n"
      "Error(66:4): Invalid relative value id: 100 (Must be <= 4)\n"
      "    ret void;\n"
      "  }\n"
      "}\n",
      DumpMunger.getTestResults());

  NaClParseBitcodeMunger Munger(BitcodeRecords,
                                array_lengthof(BitcodeRecords), Terminator);
  EXPECT_FALSE(Munger.runTest("Nonexistant call arg", true));
  EXPECT_EQ(
      "Error: (56:6) Invalid call argument: Index 1\n"
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
  NaClObjDumpMunger DumpMunger(BitcodeRecords,
                               array_lengthof(BitcodeRecords), Terminator);
  EXPECT_TRUE(DumpMunger.runTestForAssembly("BadAllocaAlignment"));
  EXPECT_EQ(
      "module {  // BlockID = 8\n"
      "  types {  // BlockID = 17\n"
      "    count 4;\n"
      "    @t0 = i32;\n"
      "    @t1 = void;\n"
      "    @t2 = void (i32);\n"
      "    @t3 = i8;\n"
      "  }\n"
      "  define external void @f0(i32);\n"
      "  function void @f0(i32 %p0) {  // BlockID = 12\n"
      "    blocks 1;\n"
      "  %b0:\n"
      "    %v0 = alloca i8, i32 %p0, align 1;\n"
      "    ret void;\n"
      "  }\n"
      "}\n",
      DumpMunger.getTestResults());
  NaClParseBitcodeMunger Munger(BitcodeRecords,
                                array_lengthof(BitcodeRecords), Terminator);
  EXPECT_TRUE(Munger.runTest("BadAllocaAlignment", true));

  // Show what happens when changing alignment to 0.
  const uint64_t Align0[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace,
    3, naclbitc::FUNC_CODE_INST_ALLOCA, 1, getEncAlignZero(), Terminator,
  };
  EXPECT_TRUE(Munger.runTest(
      "BadAllocaAlignment-0", Align0, array_lengthof(Align0), true));
  EXPECT_TRUE(DumpMunger.runTestForAssembly(
      "BadAllocaAlignment-0", Align0, array_lengthof(Align0)));
  EXPECT_EQ(
      "    %v0 = alloca i8, i32 %p0, align 0;\n",
      DumpMunger.getLinesWithSubstring("alloca"));

  // Show what happens when changing alignment to 2**30.
  const uint64_t Align30[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace,
    3, naclbitc::FUNC_CODE_INST_ALLOCA, 1, getEncAlignPower(30), Terminator,
  };
  EXPECT_FALSE(Munger.runTest(
      "BadAllocaAlignment-30", Align30, array_lengthof(Align30), true));
  EXPECT_EQ(
      "Error: (49:6) Alignment can't be greater than 2**29. Found: 2**30\n"
      "Error: Invalid value in record\n",
      Munger.getTestResults());
  EXPECT_FALSE(DumpMunger.runTestForAssembly(
      "BadAllocaAlignment-30", Align30, array_lengthof(Align30)));
  EXPECT_EQ(
      "    %v0 = alloca i8, i32 %p0, align 0;\n",
      DumpMunger.getLinesWithSubstring("alloca"));
  EXPECT_EQ(
      "Error(62:4): Alignment can't be greater than 2**29. Found: 2**30\n",
      DumpMunger.getLinesWithSubstring("Error"));

  // Show what happens when changing alignment to 2**29.
  const uint64_t Align29[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace,
    3, naclbitc::FUNC_CODE_INST_ALLOCA, 1, getEncAlignPower(29), Terminator,
  };
  EXPECT_TRUE(Munger.runTest(
      "BadAllocaAlignment-29", Align29, array_lengthof(Align29), true));
  EXPECT_EQ(
      "Successful parse!\n",
      Munger.getTestResults());
  EXPECT_TRUE(DumpMunger.runTestForAssembly(
      "BadAllocaAlignment-29", Align29, array_lengthof(Align29)));
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
  NaClObjDumpMunger DumpMunger(BitcodeRecords,
                               array_lengthof(BitcodeRecords), Terminator);
  EXPECT_TRUE(DumpMunger.runTestForAssembly("BadLoadAlignment-1"));
  EXPECT_EQ(
      "module {  // BlockID = 8\n"
      "  types {  // BlockID = 17\n"
      "    count 2;\n"
      "    @t0 = i32;\n"
      "    @t1 = i32 (i32);\n"
      "  }\n"
      "  define external i32 @f0(i32);\n"
      "  function i32 @f0(i32 %p0) {  // BlockID = 12\n"
      "    blocks 1;\n"
      "  %b0:\n"
      "    %v0 = load i32* %p0, align 1;\n"
      "    ret i32 %v0;\n"
      "  }\n"
      "}\n",
      DumpMunger.getTestResults());
  NaClParseBitcodeMunger Munger(BitcodeRecords,
                                array_lengthof(BitcodeRecords), Terminator);
  EXPECT_TRUE(Munger.runTest("BadLoadAlignment", true));

  // Show what happens when changing alignment to 0.
  const uint64_t Align0[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace,
    3, naclbitc::FUNC_CODE_INST_LOAD, 1, getEncAlignZero(), 0, Terminator,
  };
  // Note: Correct alignment is not checked by Munger (i.e. the PNaCl
  // bitcode reader). It is checked later by the PNaCl ABI checker in
  // pnacl-llc. On the other hand, the DumpMunger checks alignment for
  // loads while parsing.
  EXPECT_TRUE(Munger.runTest(
      "BadLoadAlignment-0", Align0, array_lengthof(Align0), true));
  EXPECT_FALSE(DumpMunger.runTestForAssembly(
      "BadLoadAlignment-0", Align0, array_lengthof(Align0)));
  EXPECT_EQ(
      "    %v0 = load i32* %p0, align 0;\n"
      "Error(58:4): load: Illegal alignment for i32. Expects: 1\n",
      DumpMunger.getLinesWithSubstring("load"));

  // Show what happens when changing alignment to 4.
  const uint64_t Align4[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace,
    3, naclbitc::FUNC_CODE_INST_LOAD, 1, getEncAlignPower(2), 0, Terminator,
  };
  EXPECT_TRUE(Munger.runTest(
      "BadLoadAlignment-4", Align4, array_lengthof(Align4), true));
  EXPECT_FALSE(DumpMunger.runTestForAssembly(
      "BadLoadAlignment-4", Align4, array_lengthof(Align4)));
  EXPECT_EQ(
      "    %v0 = load i32* %p0, align 4;\n"
      "Error(58:4): load: Illegal alignment for i32. Expects: 1\n",
      DumpMunger.getLinesWithSubstring("load"));

  // Show what happens when changing alignment to 2**29.
  const uint64_t Align29[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace,
    3, naclbitc::FUNC_CODE_INST_LOAD, 1, getEncAlignPower(29), 0, Terminator,
  };
  EXPECT_TRUE(Munger.runTest(
      "BadLoadAlignment-29", Align29, array_lengthof(Align29), true));
  EXPECT_FALSE(DumpMunger.runTestForAssembly(
      "BadLoadAlignment-29", Align29, array_lengthof(Align29)));
  EXPECT_EQ(
      "    %v0 = load i32* %p0, align 536870912;\n"
      "Error(58:4): load: Illegal alignment for i32. Expects: 1\n",
      DumpMunger.getLinesWithSubstring("load"));

  // Show what happens when changing alignment to 2**30.
  const uint64_t Align30[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace,
    3, naclbitc::FUNC_CODE_INST_LOAD, 1, getEncAlignPower(30), 0, Terminator,
  };
  EXPECT_FALSE(Munger.runTest(
      "BadLoadAlignment-30", Align30, array_lengthof(Align30), true));
  EXPECT_EQ(
      "Error: (46:4) Alignment can't be greater than 2**29. Found: 2**30\n"
      "Error: Invalid value in record\n",
      Munger.getTestResults());
  EXPECT_FALSE(DumpMunger.runTestForAssembly(
      "BadLoadAlignment-30", Align30, array_lengthof(Align30)));
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
  NaClObjDumpMunger DumpMunger(BitcodeRecords,
                               array_lengthof(BitcodeRecords), Terminator);
  EXPECT_TRUE(DumpMunger.runTestForAssembly("BadStoreAlignment"));
  EXPECT_EQ(
      "module {  // BlockID = 8\n"
      "  types {  // BlockID = 17\n"
      "    count 3;\n"
      "    @t0 = float;\n"
      "    @t1 = i32;\n"
      "    @t2 = float (i32, float);\n"
      "  }\n"
      "  define external float @f0(i32, float);\n"
      "  function float @f0(i32 %p0, float %p1) {  // BlockID = 12\n"
      "    blocks 1;\n"
      "  %b0:\n"
      "    store float %p1, float* %p0, align 1;\n"
      "    ret float %p1;\n"
      "  }\n"
      "}\n",
      DumpMunger.getTestResults());
  NaClParseBitcodeMunger Munger(BitcodeRecords,
                                array_lengthof(BitcodeRecords), Terminator);
  EXPECT_TRUE(Munger.runTest("BadStoreAlignment", true));

  // Show what happens when changing alignment to 0.
  const uint64_t Align0[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace,
    3, naclbitc::FUNC_CODE_INST_STORE, 2, 1, getEncAlignZero(), Terminator,
  };
  // Note: Correct alignment is not checked by Munger (i.e. the PNaCl
  // bitcode reader). It is checked later by the PNaCl ABI checker in
  // pnacl-llc. On the other hand, the DumpMunger checks alignment for
  // stores while parsing.
  EXPECT_TRUE(Munger.runTest(
      "BadStoreAlignment-0", Align0, array_lengthof(Align0), true));
  EXPECT_FALSE(DumpMunger.runTestForAssembly(
      "BadStoreAlignment-0", Align0, array_lengthof(Align0)));
  EXPECT_EQ(
      "    store float %p1, float* %p0, align 0;\n"
      "Error(62:4): store: Illegal alignment for float. Expects: 1 or 4\n",
      DumpMunger.getLinesWithSubstring("store"));

  // Show what happens when changing alignment to 4.
  const uint64_t Align4[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace,
    3, naclbitc::FUNC_CODE_INST_STORE, 2, 1, getEncAlignPower(2), Terminator,
  };
  EXPECT_TRUE(Munger.runTest(
      "BadStoreAlignment-4", Align4, array_lengthof(Align4), true));
  EXPECT_TRUE(DumpMunger.runTestForAssembly(
      "BadStoreAlignment-4", Align4, array_lengthof(Align4)));

  // Show what happens when changing alignment to 8.
  const uint64_t Align8[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace,
    3, naclbitc::FUNC_CODE_INST_STORE, 2, 1, getEncAlignPower(3), Terminator,
  };
  EXPECT_TRUE(Munger.runTest(
      "BadStoreAlignment-8", Align8, array_lengthof(Align8), true));
  EXPECT_FALSE(DumpMunger.runTestForAssembly(
      "BadStoreAlignment-8", Align8, array_lengthof(Align8)));
  EXPECT_EQ(
      "    store float %p1, float* %p0, align 8;\n"
      "Error(62:4): store: Illegal alignment for float. Expects: 1 or 4\n",
      DumpMunger.getLinesWithSubstring("store"));

  // Show what happens when changing alignment to 2**29.
  const uint64_t Align29[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace,
    3, naclbitc::FUNC_CODE_INST_STORE, 2, 1, getEncAlignPower(29), Terminator,
  };
  EXPECT_TRUE(Munger.runTest(
      "BadStoreAlignment-29", Align29, array_lengthof(Align29), true));
  EXPECT_FALSE(DumpMunger.runTestForAssembly(
      "BadStoreAlignment-29", Align29, array_lengthof(Align29)));
  EXPECT_EQ(
      "    store float %p1, float* %p0, align 536870912;\n"
      "Error(62:4): store: Illegal alignment for float. Expects: 1 or 4\n",
      DumpMunger.getLinesWithSubstring("store"));

  // Show what happens when changing alignment to 2**30.
  const uint64_t Align30[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace,
    3, naclbitc::FUNC_CODE_INST_STORE, 2, 1, getEncAlignPower(30), Terminator,
  };
  EXPECT_FALSE(Munger.runTest(
      "BadStoreAlignment-30", Align30, array_lengthof(Align30), true));
  EXPECT_EQ(
      "Error: (50:4) Alignment can't be greater than 2**29. Found: 2**30\n"
      "Error: Invalid value in record\n",
      Munger.getTestResults());
  EXPECT_FALSE(DumpMunger.runTestForAssembly(
      "BadStoreAlignment-30", Align30, array_lengthof(Align30)));
  EXPECT_EQ(
      "    store float %p1, float* %p0, align 0;\n"
      "Error(62:4): store: Illegal alignment for float. Expects: 1 or 4\n",
      DumpMunger.getLinesWithSubstring("store"));
}

} // end of anonamous namespace.
