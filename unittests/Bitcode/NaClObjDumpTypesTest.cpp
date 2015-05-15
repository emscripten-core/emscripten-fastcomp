//===- llvm/unittest/Bitcode/NaClObjDumpTypesTest.cpp ---------------------===//
//     Tests objdump stream for PNaCl bitcode.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// Tests record errors in the types block when dumping PNaCl bitcode.

#include "NaClMungeTest.h"

using namespace llvm;

namespace naclmungetest {

static const char ErrorPrefix[] = "Error";

// Tests what happens when a type refers to a not-yet defined type.
TEST(NaClObjDumpTypesTest, BadTypeReferences) {
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

  // Show base input.
  NaClObjDumpMunger Munger(ARRAY_TERM(BitcodeRecords));
  EXPECT_TRUE(Munger.runTestForAssembly());
  EXPECT_EQ(
      "module {  // BlockID = 8\n"
      "  types {  // BlockID = 17\n"
      "    count 2;\n"
      "    @t0 = i32;\n"
      "    @t1 = float;\n"
      "  }\n"
      "}\n",
      Munger.getTestResults());

  // Show what happens when defining: @t1 = <4 x @t1>
  const uint64_t AddSelfReference[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 4, 1, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(AddSelfReference)));
  // Note: Because @t1 is not defined until after this instruction,
  // the initial lookup of @t1 in <4 x @t1> is not found. To error
  // recover, type "void" is returned as the type of @t1.
  EXPECT_EQ("Error(37:6): Can't find definition for @t1\n"
            "Error(37:6): Vectors can only be defined on primitive types."
            " Found void. Assuming i32 instead.\n",
            Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <4 x i32>;\n"
      "Error(37:6): Can't find definition for @t1\n",
      Munger.getLinesWithSubstring("@t1"));

  // Show what happens when defining: @t1 = <4 x @t5>
  const uint64_t AddForwardReference[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 4, 5, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(AddForwardReference)));
  // Note: Because @t5 is not defined, type "void" is used to error recover.
  EXPECT_EQ(
      "Error(37:6): Can't find definition for @t5\n"
      "Error(37:6): Vectors can only be defined on primitive types."
      " Found void. Assuming i32 instead.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <4 x i32>;\n",
      Munger.getLinesWithSubstring("@t1"));
}

// Tests handling of the count record in the types block.
TEST(NaClObjDumpTypesTest, TestCountRecord) {
  const uint64_t BitcodeRecords[] = {
    1, 65535, 8, 2, Terminator,
    1, 65535, 17, 2, Terminator,
    3, 1, 2, Terminator,
    3, 7, 32, Terminator,
    3, 3, Terminator,
    0, 65534, Terminator,
    0, 65534, Terminator
  };

  const uint64_t AddBeforeIndex = 5;
  const uint64_t ReplaceIndex = 2;

  NaClObjDumpMunger Munger(ARRAY_TERM(BitcodeRecords));

  // Test case where count is correct.
  EXPECT_TRUE(Munger.runTestForAssembly());
  EXPECT_EQ(
      "module {  // BlockID = 8\n"
      "  types {  // BlockID = 17\n"
      "    count 2;\n"
      "    @t0 = i32;\n"
      "    @t1 = float;\n"
      "  }\n"
      "}\n",
      Munger.getTestResults());

  // Test case where more types are defined then specified by the
  // count record.
  const uint64_t AddDoubleType[] = {
    AddBeforeIndex, NaClMungedBitcode::AddBefore, 3, 4, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(AddDoubleType)));
  EXPECT_EQ("Error(41:2): Expected 2 types but found: 3\n",
            Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t0 = i32;\n"
      "    @t1 = float;\n"
      "    @t2 = double;\n",
      Munger.getLinesWithSubstring("@t"));

  // Test case where fewer types are defined then specified by the count
  // record.
  const uint64_t DeleteI32Type[] = { 3, NaClMungedBitcode::Remove };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(DeleteI32Type)));
  EXPECT_EQ("Error(36:2): Expected 2 types but found: 1\n",
            Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t0 = float;\n",
      Munger.getLinesWithSubstring("@t"));

  // Test if we generate an error message if the count record isn't first.
  const uint64_t AddI16BeforeCount[] = {
    ReplaceIndex, NaClMungedBitcode::AddBefore, 3,  7, 16, Terminator };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(AddI16BeforeCount)));
  EXPECT_EQ(
      "module {  // BlockID = 8\n"
      "  types {  // BlockID = 17\n"
      "    @t0 = i16;\n"
      "    count 2;\n"
      "Error(34:4): Count record not first record of types block\n"
      "    @t1 = i32;\n"
      "    @t2 = float;\n"
      "  }\n"
      "Error(42:0): Expected 2 types but found: 3\n"
      "}\n",
      Munger.getTestResults());

  // Test if count record doesn't contain enough elements.
  const uint64_t CountRecordEmpty[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 1, Terminator };
  EXPECT_FALSE(Munger.runTestForErrors(ARRAY(CountRecordEmpty)));
  EXPECT_EQ("Error(32:0): Count record should have 1 argument. Found: 0\n"
            "Error(38:6): Expected 0 types but found: 2\n",
            Munger.getTestResults());

  // Test if count record has extraneous values.
  const uint64_t CountRecordTooLong[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 1, 14, 2, Terminator
  };
  EXPECT_FALSE(Munger.runTestForErrors(ARRAY(CountRecordTooLong)));
  EXPECT_EQ("Error(32:0): Count record should have 1 argument. Found: 2\n"
            "Error(40:2): Expected 0 types but found: 2\n",
            Munger.getTestResults());
}

// Tests handling of the void record in the types block.
TEST(NaClObjDumpTypesTest, TestVoidRecord) {
  const uint64_t BitcodeRecords[] = {
    1, 65535, 8, 2, Terminator,
    1, 65535, 17, 2, Terminator,
    3, 1, 1, Terminator,
    3, 2, Terminator,
    0, 65534, Terminator,
    0, 65534, Terminator
  };

  const uint64_t ReplaceIndex = 3;

  NaClObjDumpMunger Munger(ARRAY_TERM(BitcodeRecords));

  // Test where void is properly specified.
  EXPECT_TRUE(Munger.runTestForAssembly());
  EXPECT_EQ(
      "module {  // BlockID = 8\n"
      "  types {  // BlockID = 17\n"
      "    count 1;\n"
      "    @t0 = void;\n"
      "  }\n"
      "}\n",
      Munger.getTestResults());

  // Test where void record has extraneous values.
  const uint64_t VoidRecordTooLong[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 2, 5, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(VoidRecordTooLong)));
  EXPECT_EQ("Error(34:4): Void record shouldn't have arguments. Found: 1\n",
            Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t0 = void;\n",
      Munger.getLinesWithSubstring("@t0"));
}

// Tests handling of integer records in the types block.
TEST(NaClObjDumpTypesTest, TestIntegerRecord) {
  const uint64_t BitcodeRecords[] = {
    1, 65535, 8, 2, Terminator,
    1, 65535, 17, 2, Terminator,
    3, 1, 1, Terminator,
    3, 7, 1,  Terminator,
    0, 65534, Terminator,
    0, 65534, Terminator
  };

  const uint64_t ReplaceIndex = 3;

  NaClObjDumpMunger Munger(ARRAY_TERM(BitcodeRecords));

  // Tests that we accept i1.
  EXPECT_TRUE(Munger.runTestForAssembly());
  EXPECT_EQ(
      "module {  // BlockID = 8\n"
      "  types {  // BlockID = 17\n"
      "    count 1;\n"
      "    @t0 = i1;\n"
      "  }\n"
      "}\n",
      Munger.getTestResults());

  // Tests that we reject i2.
  const uint64_t TestTypeI2[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 7, 2, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(TestTypeI2)));
  EXPECT_EQ(
      "Error(34:4): Integer record contains bad integer size: 2\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  // Note: Error recovery uses i32 when type size is bad.
  EXPECT_EQ(
      "    @t0 = i32;\n",
      Munger.getLinesWithSubstring("@t0"));

  // Tests that we accept i8.
  const uint64_t TestTypeI8[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 7, 8, Terminator
  };
  EXPECT_TRUE(Munger.runTest(ARRAY(TestTypeI8)));

  // Tests that we accept i16.
  const uint64_t TestTypeI16[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 7, 16, Terminator
  };
  EXPECT_TRUE(Munger.runTest(ARRAY(TestTypeI16)));

  // Tests that we accept i32.
  const uint64_t TestTypeI32[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 7, 32, Terminator
  };
  EXPECT_TRUE(Munger.runTest(ARRAY(TestTypeI32)));

  // Tests that we accept i64.
  const uint64_t TestTypeI64[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 7, 64, Terminator
  };
  EXPECT_TRUE(Munger.runTest(ARRAY(TestTypeI64)));

  // Tests that we reject i128.
  const uint64_t TestTypeI128[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 7, 128, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(TestTypeI128)));
  EXPECT_EQ(
      "Error(34:4): Integer record contains bad integer size: 128\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  // Note: Error recovery uses i32 when type size is bad.
  EXPECT_EQ(
      "    @t0 = i32;\n",
      Munger.getLinesWithSubstring("@t0"));

  // Tests when not enough values are in the integer record.
  const uint64_t RecordTooShort[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 7, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(RecordTooShort)));
  EXPECT_EQ(
      "Error(34:4): Integer record should have one argument. Found: 0\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  // Note: Error recovery uses i32 when type size is bad.
  EXPECT_EQ(
      "    @t0 = i32;\n",
      Munger.getLinesWithSubstring("@t0"));

  // Tests when too many values are in the integer record.
  const uint64_t RecordTooLong[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 7, 32, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForErrors(ARRAY(RecordTooLong)));
  EXPECT_EQ(
      "Error(34:4): Integer record should have one argument. Found: 2\n",
      Munger.getTestResults());
}

// Tests handling of the float record in the types block.
TEST(NaClObjDumpTypesTest, TestFloatRecord) {
  const uint64_t BitcodeRecords[] = {
    1, 65535, 8, 2, Terminator,
    1, 65535, 17, 2, Terminator,
    3, 1, 1, Terminator,
    3, 3,  Terminator,
    0, 65534, Terminator,
    0, 65534, Terminator
  };

  const uint64_t ReplaceIndex = 3;

  NaClObjDumpMunger Munger(ARRAY_TERM(BitcodeRecords));

  // Test that we accept the float record.
  EXPECT_TRUE(Munger.runTestForAssembly());
  EXPECT_EQ(
      "module {  // BlockID = 8\n"
      "  types {  // BlockID = 17\n"
      "    count 1;\n"
      "    @t0 = float;\n"
      "  }\n"
      "}\n",
      Munger.getTestResults());

  // Test error for float record that has extraneous values.
  const uint64_t FloatRecordTooLong[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 3, 5, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(FloatRecordTooLong)));
  EXPECT_EQ(
      "Error(34:4): Float record shoudn't have arguments. Found: 1\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t0 = float;\n",
      Munger.getLinesWithSubstring("@t"));
}

// Tests handling of the double record in the types block.
TEST(NaClObjDumpTypesTest, TestDoubleRecord) {
  const uint64_t BitcodeRecords[] = {
    1, 65535, 8, 2, Terminator,
    1, 65535, 17, 2, Terminator,
    3, 1, 1, Terminator,
    3, 4, Terminator,
    0, 65534, Terminator,
    0, 65534, Terminator
  };

  const uint64_t ReplaceIndex = 3;

  NaClObjDumpMunger Munger(ARRAY_TERM(BitcodeRecords));

  // Test that we accept the double record.
  EXPECT_TRUE(Munger.runTestForAssembly());
  EXPECT_EQ(
      "module {  // BlockID = 8\n"
      "  types {  // BlockID = 17\n"
      "    count 1;\n"
      "    @t0 = double;\n"
      "  }\n"
      "}\n",
      Munger.getTestResults());

  // Test error for double record that has extraneous values.
  const uint64_t DoubleRecordTooLong[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 4, 5, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(DoubleRecordTooLong)));
  EXPECT_EQ(
      "Error(34:4): Double record shound't have arguments. Found: 1\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t0 = double;\n",
      Munger.getLinesWithSubstring("@t"));
}

// Test vector records of the wrong size.
TEST(NaClObjDumpTypesTest, TestVectorRecordLength) {
  const uint64_t BitcodeRecords[] = {
    1, 65535, 8, 2, Terminator,
    1, 65535, 17, 2, Terminator,
    3, 1, 2, Terminator,
    3, 7, 32, Terminator,
    3, 12, 4, 0, Terminator,
    0, 65534, Terminator,
    0, 65534, Terminator
  };

  const uint64_t ReplaceIndex = 4;

  NaClObjDumpMunger Munger(ARRAY_TERM(BitcodeRecords));

  // Test correct length vector record.
  EXPECT_TRUE(Munger.runTestForAssembly());
  EXPECT_EQ(
      "module {  // BlockID = 8\n"
      "  types {  // BlockID = 17\n"
      "    count 2;\n"
      "    @t0 = i32;\n"
      "    @t1 = <4 x i32>;\n"
      "  }\n"
      "}\n",
      Munger.getTestResults());

  // Test vector record too short.
  uint64_t RecordTooShort[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 4, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(RecordTooShort)));
  EXPECT_EQ(
      "Error(37:6): Vector record should contain two arguments. Found: 1\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = void;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test vector record too long.
  uint64_t RecordTooLong[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 4, 0, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(RecordTooLong)));
  EXPECT_EQ(
      "Error(37:6): Vector record should contain two arguments. Found: 3\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = void;\n",
      Munger.getLinesWithSubstring("@t1"));
}

// Test i1 vector records in the types block.
TEST(NaClObjDumpTypesTest, TestI1VectorRecord) {
  const uint64_t BitcodeRecords[] = {
    1, 65535, 8, 2, Terminator,
    1, 65535, 17, 2, Terminator,
    3, 1, 2, Terminator,
    3, 7, 1, Terminator,
    3, 12, 4, 0, Terminator,
    0, 65534, Terminator,
    0, 65534, Terminator
  };

  const uint64_t ReplaceIndex = 4;

  NaClObjDumpMunger Munger(ARRAY_TERM(BitcodeRecords));

  // Test that we accept <4 x i1>.
  EXPECT_TRUE(Munger.runTestForAssembly());
  EXPECT_EQ(
      "module {  // BlockID = 8\n"
      "  types {  // BlockID = 17\n"
      "    count 2;\n"
      "    @t0 = i1;\n"
      "    @t1 = <4 x i1>;\n"
      "  }\n"
      "}\n",
      Munger.getTestResults());

  // Test that we don't handle <1 x i1>.
  const uint64_t Vector1xI1[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 1, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector1xI1)));
  EXPECT_EQ(
      "Error(37:0): Vector type <1 x i1> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <1 x i1>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <2 x i1>.
  const uint64_t Vector2xI1[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 2, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector2xI1)));
  EXPECT_EQ(
      "Error(37:0): Vector type <2 x i1> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <2 x i1>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <3 x i1>.
  const uint64_t Vector3xI1[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 3, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector3xI1)));
  EXPECT_EQ(
      "Error(37:0): Vector type <3 x i1> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <3 x i1>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we handle <8 x i1>.
  const uint64_t Vector8xI1[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 8, 0, Terminator
  };
  EXPECT_TRUE(Munger.runTest(ARRAY(Vector8xI1)));

  // Test that we handle <16 x i1>.
  const uint64_t Vector16xI1[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 16, 0, Terminator
  };
  EXPECT_TRUE(Munger.runTest(ARRAY(Vector16xI1)));

  // Test that we reject <32 x i1>.
  const uint64_t Vector32xI1[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 32, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector32xI1)));
  EXPECT_EQ(
      "Error(37:0): Vector type <32 x i1> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <32 x i1>;\n",
      Munger.getLinesWithSubstring("@t1"));
}

// Test i8 vector records in the types block.
TEST(NaClObjDumpTypesTest, TestI8VectorRecord) {
  const uint64_t BitcodeRecords[] = {
    1, 65535, 8, 2, Terminator,
    1, 65535, 17, 2, Terminator,
    3, 1, 2, Terminator,
    3, 7, 8, Terminator,
    3, 12, 16, 0, Terminator,
    0, 65534, Terminator,
    0, 65534, Terminator
  };

  const uint64_t ReplaceIndex = 4;

  NaClObjDumpMunger Munger(ARRAY_TERM(BitcodeRecords));

  // Test that we accept <16 x i8>.
  EXPECT_TRUE(Munger.runTestForAssembly());
  EXPECT_EQ(
      "module {  // BlockID = 8\n"
      "  types {  // BlockID = 17\n"
      "    count 2;\n"
      "    @t0 = i8;\n"
      "    @t1 = <16 x i8>;\n"
      "  }\n"
      "}\n",
      Munger.getTestResults());

  // Test that we reject <1 x i8>.
  const uint64_t Vector1xI8[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 1, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector1xI8)));
  EXPECT_EQ(
      "Error(37:0): Vector type <1 x i8> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <1 x i8>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <2 x i8>.
  const uint64_t Vector2xI8[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 2, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector2xI8)));
  EXPECT_EQ(
      "Error(37:0): Vector type <2 x i8> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <2 x i8>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <3 x i8>.
  const uint64_t Vector3xI8[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 3, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector3xI8)));
  EXPECT_EQ(
      "Error(37:0): Vector type <3 x i8> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <3 x i8>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <4 x i8>.
  const uint64_t Vector4xI8[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 4, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector4xI8)));
  EXPECT_EQ(
      "Error(37:0): Vector type <4 x i8> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <4 x i8>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <8 x i8>.
  const uint64_t Vector8xI8[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 8, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector8xI8)));
  EXPECT_EQ(
      "Error(37:0): Vector type <8 x i8> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <8 x i8>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <32 x i8>.
  const uint64_t Vector32xI8[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 32, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector32xI8)));
  EXPECT_EQ(
      "Error(37:0): Vector type <32 x i8> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <32 x i8>;\n",
      Munger.getLinesWithSubstring("@t1"));
}

// Test i16 vector records in the types block.
TEST(NaClObjDumpTypesTest, TestI16VectorRecord) {
  const uint64_t BitcodeRecords[] = {
    1, 65535, 8, 2, Terminator,
    1, 65535, 17, 2, Terminator,
    3, 1, 2, Terminator,
    3, 7, 16, Terminator,
    3, 12, 8, 0, Terminator,
    0, 65534, Terminator,
    0, 65534, Terminator
  };

  const uint64_t ReplaceIndex = 4;

  NaClObjDumpMunger Munger(ARRAY_TERM(BitcodeRecords));

  // Test that we accept <8 x i16>.
  EXPECT_TRUE(Munger.runTestForAssembly());
  EXPECT_EQ(
      "module {  // BlockID = 8\n"
      "  types {  // BlockID = 17\n"
      "    count 2;\n"
      "    @t0 = i16;\n"
      "    @t1 = <8 x i16>;\n"
      "  }\n"
      "}\n",
      Munger.getTestResults());

  // Test that we reject <1 x i16>.
  const uint64_t Vector1xI16[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 1, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector1xI16)));
  EXPECT_EQ(
      "Error(37:0): Vector type <1 x i16> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <1 x i16>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <2 x i16>.
  const uint64_t Vector2xI16[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 2, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector2xI16)));
  EXPECT_EQ(
      "Error(37:0): Vector type <2 x i16> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <2 x i16>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <3 x i16>.
  const uint64_t Vector3xI16[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 3, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector3xI16)));
  EXPECT_EQ(
      "Error(37:0): Vector type <3 x i16> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <3 x i16>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <4 x i16>.
  const uint64_t Vector4xI16[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 4, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector4xI16)));
  EXPECT_EQ(
      "Error(37:0): Vector type <4 x i16> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <4 x i16>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <16 x i16>.
  const uint64_t Vector16xI16[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 16, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector16xI16)));
  EXPECT_EQ(
      "Error(37:0): Vector type <16 x i16> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <16 x i16>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <32 x i16>.
  const uint64_t Vector32xI16[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 32, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector32xI16)));
  EXPECT_EQ(
      "Error(37:0): Vector type <32 x i16> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <32 x i16>;\n",
      Munger.getLinesWithSubstring("@t1"));
}

// Test i32 vector records in the types block.
TEST(NaClObjDumpTypesTest, TestI32VectorRecord) {
  const uint64_t BitcodeRecords[] = {
    1, 65535, 8, 2, Terminator,
    1, 65535, 17, 2, Terminator,
    3, 1, 2, Terminator,
    3, 7, 32, Terminator,
    3, 12, 4, 0, Terminator,
    0, 65534, Terminator,
    0, 65534, Terminator
  };

  const uint64_t ReplaceIndex = 4;

  NaClObjDumpMunger Munger(ARRAY_TERM(BitcodeRecords));

  // Test that we accept <4 x i32>.
  EXPECT_TRUE(Munger.runTestForAssembly());
  EXPECT_EQ(
      "module {  // BlockID = 8\n"
      "  types {  // BlockID = 17\n"
      "    count 2;\n"
      "    @t0 = i32;\n"
      "    @t1 = <4 x i32>;\n"
      "  }\n"
      "}\n",
      Munger.getTestResults());

  // Test that we reject <1 x i32>.
  const uint64_t Vector1xI32[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 1, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector1xI32)));
  EXPECT_EQ(
      "Error(37:6): Vector type <1 x i32> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <1 x i32>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <2 x i32>.
  const uint64_t Vector2xI32[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 2, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector2xI32)));
  EXPECT_EQ(
      "Error(37:6): Vector type <2 x i32> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <2 x i32>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <3 x i32>.
  const uint64_t Vector3xI32[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 3, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector3xI32)));
  EXPECT_EQ(
      "Error(37:6): Vector type <3 x i32> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <3 x i32>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <8 x i32>.
  const uint64_t Vector8xI32[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 8, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector8xI32)));
  EXPECT_EQ(
      "Error(37:6): Vector type <8 x i32> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <8 x i32>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <16 x i32>.
  const uint64_t Vector16xI32[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 16, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector16xI32)));
  EXPECT_EQ(
      "Error(37:6): Vector type <16 x i32> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <16 x i32>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <32 x i32>.
  const uint64_t Vector32xI32[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 32, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector32xI32)));
  EXPECT_EQ(
      "Error(37:6): Vector type <32 x i32> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <32 x i32>;\n",
      Munger.getLinesWithSubstring("@t1"));
}

// Test i64 vector types.
TEST(NaClObjDumpTypesTest, TestI64VectorRecord) {
  const uint64_t BitcodeRecords[] = {
    1, 65535, 8, 2, Terminator,
    1, 65535, 17, 2, Terminator,
    3, 1, 2, Terminator,
    3, 7, 64, Terminator,
    3, 12, 1, 0, Terminator,
    0, 65534, Terminator,
    0, 65534, Terminator
  };

  const uint64_t ReplaceIndex = 4;

  NaClObjDumpMunger Munger(ARRAY_TERM(BitcodeRecords));

  // Test that we reject <1 x i64>.
  EXPECT_FALSE(Munger.runTestForAssembly());
  EXPECT_EQ(
      "module {  // BlockID = 8\n"
      "  types {  // BlockID = 17\n"
      "    count 2;\n"
      "    @t0 = i64;\n"
      "    @t1 = <1 x i64>;\n"
      "Error(37:6): Vector type <1 x i64> not allowed.\n"
      "  }\n"
      "}\n",
      Munger.getTestResults());

  // Test that we don't handle <2 x i64>.
  const uint64_t Vector2xI64[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 2, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector2xI64)));
  EXPECT_EQ(
      "Error(37:6): Vector type <2 x i64> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <2 x i64>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <3 x i64>.
  const uint64_t Vector3xI64[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 3, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector3xI64)));
  EXPECT_EQ(
      "Error(37:6): Vector type <3 x i64> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <3 x i64>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we reject <4 x i64>.
  const uint64_t Vector4xI64[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 4, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector4xI64)));
  EXPECT_EQ(
      "Error(37:6): Vector type <4 x i64> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <4 x i64>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <8 x i64>.
  const uint64_t Vector8xI64[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 8, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector8xI64)));
  EXPECT_EQ(
      "Error(37:6): Vector type <8 x i64> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <8 x i64>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <16 x i64>.
  const uint64_t Vector16xI64[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 16, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector16xI64)));
  EXPECT_EQ(
      "Error(37:6): Vector type <16 x i64> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <16 x i64>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <32 x i64>.
  const uint64_t Vector32xI64[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 32, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector32xI64)));
  EXPECT_EQ(
      "Error(37:6): Vector type <32 x i64> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <32 x i64>;\n",
      Munger.getLinesWithSubstring("@t1"));
}

// Test handling of float vector types.
TEST(NaClObjDumpTypesTest, TestFloatVectorRecord) {
  const uint64_t BitcodeRecords[] = {
    1, 65535, 8, 2, Terminator,
    1, 65535, 17, 2, Terminator,
    3, 1, 2, Terminator,
    3, 3, Terminator,
    3, 12, 4, 0, Terminator,
    0, 65534, Terminator,
    0, 65534, Terminator
  };

  const uint64_t ReplaceIndex = 4;

  NaClObjDumpMunger Munger(ARRAY_TERM(BitcodeRecords));

  // Test that we accept <4 x float>.
  EXPECT_TRUE(Munger.runTestForAssembly());
  EXPECT_EQ(
      "module {  // BlockID = 8\n"
      "  types {  // BlockID = 17\n"
      "    count 2;\n"
      "    @t0 = float;\n"
      "    @t1 = <4 x float>;\n"
      "  }\n"
      "}\n",
      Munger.getTestResults());

  // Test that we reject <1 x float>.
  const uint64_t Vector1xFloat[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 1, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector1xFloat)));
  EXPECT_EQ(
      "Error(36:2): Vector type <1 x float> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <1 x float>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we reject <2 x float>.
  const uint64_t Vector2xFloat[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 2, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector2xFloat)));
  EXPECT_EQ(
      "Error(36:2): Vector type <2 x float> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <2 x float>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we reject <3 x float>.
  const uint64_t Vector3xFloat[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 3, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector3xFloat)));
  EXPECT_EQ(
      "Error(36:2): Vector type <3 x float> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <3 x float>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we reject <8 x float>.
  const uint64_t Vector8xFloat[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 8, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector8xFloat)));
  EXPECT_EQ(
      "Error(36:2): Vector type <8 x float> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <8 x float>;\n",
      Munger.getLinesWithSubstring("@t1"));
}

// Test handling of double vector types.
TEST(NaClObjDumpTypesTest, TestDoubleVectorRecord) {
  const uint64_t BitcodeRecords[] = {
    1, 65535, 8, 2, Terminator,
    1, 65535, 17, 2, Terminator,
    3, 1, 2, Terminator,
    3, 4, Terminator,
    3, 12, 4, 0, Terminator,
    0, 65534, Terminator,
    0, 65534, Terminator
  };

  const uint64_t ReplaceIndex = 4;

  NaClObjDumpMunger Munger(ARRAY_TERM(BitcodeRecords));

  // Test that we reject <4 x double>.
  EXPECT_FALSE(Munger.runTestForAssembly());
  EXPECT_EQ(
      "module {  // BlockID = 8\n"
      "  types {  // BlockID = 17\n"
      "    count 2;\n"
      "    @t0 = double;\n"
      "    @t1 = <4 x double>;\n"
      "Error(36:2): Vector type <4 x double> not allowed.\n"
      "  }\n"
      "}\n",
      Munger.getTestResults());

  // Test that we reject <1 x double>.
  const uint64_t Vector1xDouble[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 1, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector1xDouble)));
  EXPECT_EQ(
      "Error(36:2): Vector type <1 x double> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <1 x double>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we reject <2 x double>.
  const uint64_t Vector2xDouble[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 2, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector2xDouble)));
  EXPECT_EQ(
      "Error(36:2): Vector type <2 x double> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <2 x double>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we reject <4 x double>.
  const uint64_t Vector3xDouble[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 4, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector3xDouble)));
  EXPECT_EQ(
      "Error(36:2): Vector type <4 x double> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <4 x double>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we reject <8 x double>.
  const uint64_t Vector8xDouble[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 12, 8, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(Vector8xDouble)));
  EXPECT_EQ(
      "Error(36:2): Vector type <8 x double> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <8 x double>;\n",
      Munger.getLinesWithSubstring("@t1"));
}

// Tests that we don't accept vectors of type void.
TEST(NaClObjDumpTypesTest, TestVoidVectorRecord) {
  const uint64_t BitcodeRecords[] = {
    1, 65535, 8, 2, Terminator,
    1, 65535, 17, 2, Terminator,
    3, 1, 2, Terminator,
    3, 2, Terminator,
    3, 12, 4, 0, Terminator,
    0, 65534, Terminator,
    0, 65534, Terminator
  };
  NaClObjDumpMunger Munger(ARRAY_TERM(BitcodeRecords));
  EXPECT_FALSE(Munger.runTestForAssembly());
  EXPECT_EQ(
      "module {  // BlockID = 8\n"
      "  types {  // BlockID = 17\n"
      "    count 2;\n"
      "    @t0 = void;\n"
      "    @t1 = <4 x i32>;\n"
      "Error(36:2): Vectors can only be defined on primitive types. "
      "Found void. Assuming i32 instead.\n"
      "  }\n"
      "}\n",
      Munger.getTestResults());
}

// Tests that we don't allow vectors of vectors.
TEST(NaClObjDumpTypesTest, TestNestedVectorRecord) {
  const uint64_t BitcodeRecords[] = {
    1, 65535, 8, 2, Terminator,
    1, 65535, 17, 2, Terminator,
    3, 1, 3, Terminator,
    3, 3, Terminator,
    3, 12, 4, 0, Terminator,
    3, 12, 4, 1, Terminator,
    0, 65534, Terminator,
    0, 65534, Terminator
  };
  NaClObjDumpMunger Munger(ARRAY_TERM(BitcodeRecords));
  EXPECT_FALSE(Munger.runTestForAssembly());
  EXPECT_EQ(
      "module {  // BlockID = 8\n"
      "  types {  // BlockID = 17\n"
      "    count 3;\n"
      "    @t0 = float;\n"
      "    @t1 = <4 x float>;\n"
      "    @t2 = <4 x i32>;\n"
      "Error(39:4): Vectors can only be defined on primitive types. "
      "Found <4 x float>. Assuming i32 instead.\n"
      "  }\n"
      "}\n",
      Munger.getTestResults());
}

// Test handling of the function record in the types block.
TEST(NaClObjDumpTypesTest, TestFunctionRecord) {
  const uint64_t BitcodeRecords[] = {
    1, 65535, 8, 2, Terminator,
    1, 65535, 17, 2, Terminator,
    3, 1, 7, Terminator,
    3, 2, Terminator,
    3, 7, 16, Terminator,
    3, 7, 32, Terminator,
    3, 3, Terminator,
    3, 4, Terminator,
    3, 12, 4, 2, Terminator,
    3, 21, 0, 0, Terminator,
    0, 65534, Terminator,
    0, 65534, Terminator
  };

  const uint64_t TypeCountIndex = 2;
  const uint64_t ReplaceIndex = 9;

  NaClObjDumpMunger Munger(ARRAY_TERM(BitcodeRecords));

  // Test void() signature.
  EXPECT_TRUE(Munger.runTestForAssembly());
  EXPECT_EQ(
      "module {  // BlockID = 8\n"
      "  types {  // BlockID = 17\n"
      "    count 7;\n"
      "    @t0 = void;\n"
      "    @t1 = i16;\n"
      "    @t2 = i32;\n"
      "    @t3 = float;\n"
      "    @t4 = double;\n"
      "    @t5 = <4 x i32>;\n"
      "    @t6 = void ();\n"
      "  }\n}\n",
      Munger.getTestResults());
  EXPECT_EQ(
      "    @t6 = void ();\n",
      Munger.getLinesWithSubstring("@t6"));

  // Tests using integers for parameters and return types.
  const uint64_t UsesIntegerTypes[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 21, 0, 1, 2, 1, Terminator
  };
  EXPECT_TRUE(Munger.runTestForAssembly(ARRAY(UsesIntegerTypes)));
  EXPECT_EQ(
      "    @t6 = i16 (i32, i16);\n",
      Munger.getLinesWithSubstring("@t6"));

  // Test using float point types for parameters and return types.
  const uint64_t UsesFloatingTypes[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 21, 0, 3, 3, 4, Terminator
  };
  EXPECT_TRUE(Munger.runTestForAssembly(ARRAY(UsesFloatingTypes)));
  EXPECT_EQ(
      "    @t6 = float (float, double);\n",
      Munger.getLinesWithSubstring("@t6"));

  // Test using vector types for parameters and return types.
  const uint64_t UsesVectorTypes[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 21, 0, 5, 5, Terminator
  };
  EXPECT_TRUE(Munger.runTestForAssembly(ARRAY(UsesVectorTypes)));
  EXPECT_EQ(
      "    @t6 = <4 x i32> (<4 x i32>);\n",
      Munger.getLinesWithSubstring("@t6"));

  // Test error if function record is too short.
  const uint64_t FunctionRecordTooShort[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 21, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(FunctionRecordTooShort)));
  EXPECT_EQ(
      "Error(48:6): Function record should contain at least 2 arguments. "
      "Found: 1\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t6 = void;\n",
      Munger.getLinesWithSubstring("@t6"));

  // Tests errror if function record specifies varargs.
  const uint64_t FunctionRecordWithVarArgs[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 21, 1, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(FunctionRecordWithVarArgs)));
  EXPECT_EQ(
      "Error(48:6): Functions with variable length arguments is "
      "not supported\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t6 = void (...);\n",
      Munger.getLinesWithSubstring("@t6"));

  // Tests if void is used as a parameter type.
  const uint64_t VoidParamType[] = {
    ReplaceIndex, NaClMungedBitcode::Replace, 3, 21, 0, 0, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(VoidParamType)));
  EXPECT_EQ(
      "Error(48:6): Invalid type for parameter 1. Found: void. Assuming: i32\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t6 = void (i32);\n",
      Munger.getLinesWithSubstring("@t6"));

  // Tests using a function type as the return type.
  const uint64_t FunctionReturnType[] = {
    TypeCountIndex, NaClMungedBitcode::Replace, 3, 1, 8, Terminator,
    ReplaceIndex, NaClMungedBitcode::AddAfter, 3, 21, 0, 6, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(FunctionReturnType)));
  EXPECT_EQ(
      "Error(52:0): Invalid return type. Found: void (). Assuming: i32\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t6 = void ();\n",
      Munger.getLinesWithSubstring("@t6"));
  EXPECT_EQ(
      "    @t7 = i32 ();\n",
      Munger.getLinesWithSubstring("@t7"));

  // Tests using a function type as a parameter type.
  const uint64_t FunctionParamType[] = {
    TypeCountIndex, NaClMungedBitcode::Replace, 3, 1, 8, Terminator,
    ReplaceIndex, NaClMungedBitcode::AddAfter, 3, 21, 0, 0, 6, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(ARRAY(FunctionParamType)));
  EXPECT_EQ(
      "Error(52:0): Invalid type for parameter 1. Found: void (). "
      "Assuming: i32\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t6 = void ();\n",
      Munger.getLinesWithSubstring("@t6"));
  EXPECT_EQ(
      "    @t7 = void (i32);\n",
      Munger.getLinesWithSubstring("@t7"));
}

// Tests how we report unknown record codes in the types block.
TEST(NaClObjDumpTypesTest, TestUnknownTypesRecordCode) {
  const uint64_t BitcodeRecords[] = {
    1, 65535, 8, 2, Terminator,
    1, 65535, 17, 2, Terminator,
    3, 1, 1, Terminator,
    3, 10, Terminator,
    0, 65534, Terminator,
    0, 65534, Terminator
  };
  NaClObjDumpMunger Munger(ARRAY_TERM(BitcodeRecords));
  EXPECT_FALSE(Munger.runTestForAssembly());
  EXPECT_EQ(
      "module {  // BlockID = 8\n"
      "  types {  // BlockID = 17\n"
      "    count 1;\n"
      "Error(34:4): Unknown record code in types block. Found: 10\n"
      "  }\n"
      "Error(36:2): Expected 1 types but found: 0\n"
      "}\n",
      Munger.getTestResults());
}

} // End of namespace naclmungetest
