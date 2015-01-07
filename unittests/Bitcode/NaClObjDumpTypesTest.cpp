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

#include "llvm/ADT/STLExtras.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeMunge.h"

#include "gtest/gtest.h"

using namespace llvm;

namespace {

static const uint64_t Terminator = 0x5768798008978675LL;

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
  NaClBitcodeMunger Munger(BitcodeRecords,
                           array_lengthof(BitcodeRecords), Terminator);
  EXPECT_TRUE(Munger.runTestForAssembly("Bad type references base"));
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
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 4, 1, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "@t1 = <4 x @t1>", AddSelfReference, array_lengthof(AddSelfReference)));
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
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 4, 5, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "@t1 = <4 x @t5>", AddForwardReference,
      array_lengthof(AddForwardReference)));
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

  NaClBitcodeMunger Munger(BitcodeRecords,
                           array_lengthof(BitcodeRecords), Terminator);

  // Test case where count is correct.
  EXPECT_TRUE(Munger.runTestForAssembly("Good case"));
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
    AddBeforeIndex, NaClBitcodeMunger::AddBefore, 3, 4, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Add double type", AddDoubleType, array_lengthof(AddDoubleType)));
  EXPECT_EQ("Error(41:2): Expected 2 types but found: 3\n",
            Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t0 = i32;\n"
      "    @t1 = float;\n"
      "    @t2 = double;\n",
      Munger.getLinesWithSubstring("@t"));

  // Test case where fewer types are defined then specified by the count
  // record.
  const uint64_t DeleteI32Type[] = { 3, NaClBitcodeMunger::Remove };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Delete I32 type", DeleteI32Type, array_lengthof(DeleteI32Type)));
  EXPECT_EQ("Error(36:2): Expected 2 types but found: 1\n",
            Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t0 = float;\n",
      Munger.getLinesWithSubstring("@t"));

  // Test if we generate an error message if the count record isn't first.
  const uint64_t AddI16BeforeCount[] = {
    ReplaceIndex, NaClBitcodeMunger::AddBefore, 3,  7, 16, Terminator };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Add I16 before count", AddI16BeforeCount,
      array_lengthof(AddI16BeforeCount)));
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
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 1, Terminator };
  EXPECT_FALSE(Munger.runTestForErrors(
      "Count record empty", CountRecordEmpty,
      array_lengthof(CountRecordEmpty)));
  EXPECT_EQ("Error(32:0): Count record should have 1 argument. Found: 0\n"
            "Error(38:6): Expected 0 types but found: 2\n",
            Munger.getTestResults());

  // Test if count record has extraneous values.
  const uint64_t CountRecordTooLong[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 1, 14, 2, Terminator
  };
  EXPECT_FALSE(Munger.runTestForErrors(
      "Count record too long", CountRecordTooLong,
      array_lengthof(CountRecordTooLong)));
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

  NaClBitcodeMunger Munger(BitcodeRecords,
                           array_lengthof(BitcodeRecords), Terminator);

  // Test where void is properly specified.
  EXPECT_TRUE(Munger.runTestForAssembly("Good case"));
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
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 2, 5, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Void record too long", VoidRecordTooLong,
      array_lengthof(VoidRecordTooLong)));
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

  NaClBitcodeMunger Munger(BitcodeRecords,
                           array_lengthof(BitcodeRecords), Terminator);

  // Tests that we accept i1.
  EXPECT_TRUE(Munger.runTestForAssembly("Test type i1"));
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
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 7, 2, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type type i2", TestTypeI2, array_lengthof(TestTypeI2)));
  EXPECT_EQ(
      "Error(34:4): Integer record contains bad integer size: 2\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  // Note: Error recovery uses i32 when type size is bad.
  EXPECT_EQ(
      "    @t0 = i32;\n",
      Munger.getLinesWithSubstring("@t0"));

  // Tests that we accept i8.
  const uint64_t TestTypeI8[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 7, 8, Terminator
  };
  EXPECT_TRUE(Munger.runTest(
      "Type type i8", TestTypeI8, array_lengthof(TestTypeI8)));

  // Tests that we accept i16.
  const uint64_t TestTypeI16[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 7, 16, Terminator
  };
  EXPECT_TRUE(Munger.runTest(
      "Type type i16", TestTypeI16, array_lengthof(TestTypeI16)));

  // Tests that we accept i32.
  const uint64_t TestTypeI32[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 7, 32, Terminator
  };
  EXPECT_TRUE(Munger.runTest(
      "Type type i32", TestTypeI32, array_lengthof(TestTypeI32)));

  // Tests that we accept i64.
  const uint64_t TestTypeI64[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 7, 64, Terminator
  };
  EXPECT_TRUE(Munger.runTest(
      "Type type i64", TestTypeI64, array_lengthof(TestTypeI64)));

  // Tests that we reject i128.
  const uint64_t TestTypeI128[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 7, 128, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type type i128", TestTypeI128, array_lengthof(TestTypeI128)));
  EXPECT_EQ(
      "Error(34:4): Integer record contains bad integer size: 128\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  // Note: Error recovery uses i32 when type size is bad.
  EXPECT_EQ(
      "    @t0 = i32;\n",
      Munger.getLinesWithSubstring("@t0"));

  // Tests when not enough values are in the integer record.
  const uint64_t RecordTooShort[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 7, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Integer record too short", RecordTooShort,
      array_lengthof(RecordTooShort)));
  EXPECT_EQ(
      "Error(34:4): Integer record should have one argument. Found: 0\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  // Note: Error recovery uses i32 when type size is bad.
  EXPECT_EQ(
      "    @t0 = i32;\n",
      Munger.getLinesWithSubstring("@t0"));

  // Tests when too many values are in the integer record.
  const uint64_t RecordTooLong[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 7, 32, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForErrors(
      "Integer record too long", RecordTooLong, array_lengthof(RecordTooLong)));
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

  NaClBitcodeMunger Munger(BitcodeRecords,
                           array_lengthof(BitcodeRecords), Terminator);

  // Test that we accept the float record.
  EXPECT_TRUE(Munger.runTestForAssembly("Good case"));
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
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 3, 5, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Float record too long", FloatRecordTooLong,
      array_lengthof(FloatRecordTooLong)));
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

  NaClBitcodeMunger Munger(BitcodeRecords,
                           array_lengthof(BitcodeRecords), Terminator);

  // Test that we accept the double record.
  EXPECT_TRUE(Munger.runTestForAssembly("Good case"));
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
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 4, 5, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Double record too long", DoubleRecordTooLong,
      array_lengthof(DoubleRecordTooLong)));
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

  NaClBitcodeMunger Munger(BitcodeRecords,
                           array_lengthof(BitcodeRecords), Terminator);

  // Test correct length vector record.
  EXPECT_TRUE(Munger.runTestForAssembly("Test valid vector record"));
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
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 4, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Record too short", RecordTooShort, array_lengthof(RecordTooShort)));
  EXPECT_EQ(
      "Error(37:6): Vector record should contain two arguments. Found: 1\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = void;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test vector record too long.
  uint64_t RecordTooLong[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 4, 0, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Record too long", RecordTooLong, array_lengthof(RecordTooLong)));
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

  NaClBitcodeMunger Munger(BitcodeRecords,
                           array_lengthof(BitcodeRecords), Terminator);

  // Test that we accept <4 x i1>.
  EXPECT_TRUE(Munger.runTestForAssembly("Type <4 x i1>"));
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
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 1, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <1 x i1>", Vector1xI1, array_lengthof(Vector1xI1)));
  EXPECT_EQ(
      "Error(37:0): Vector type <1 x i1> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <1 x i1>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <2 x i1>.
  const uint64_t Vector2xI1[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 2, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <2 x i1>", Vector2xI1, array_lengthof(Vector2xI1)));
  EXPECT_EQ(
      "Error(37:0): Vector type <2 x i1> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <2 x i1>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <3 x i1>.
  const uint64_t Vector3xI1[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 3, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <3 x i1>", Vector3xI1, array_lengthof(Vector3xI1)));
  EXPECT_EQ(
      "Error(37:0): Vector type <3 x i1> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <3 x i1>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we handle <8 x i1>.
  const uint64_t Vector8xI1[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 8, 0, Terminator
  };
  EXPECT_TRUE(Munger.runTest(
      "Type <8 x i1>", Vector8xI1, array_lengthof(Vector8xI1)));

  // Test that we handle <16 x i1>.
  const uint64_t Vector16xI1[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 16, 0, Terminator
  };
  EXPECT_TRUE(Munger.runTest(
      "Type <16 x i1>", Vector16xI1, array_lengthof(Vector16xI1)));

  // Test that we reject <32 x i1>.
  const uint64_t Vector32xI1[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 32, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <32 x i1>", Vector32xI1, array_lengthof(Vector32xI1)));
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

  NaClBitcodeMunger Munger(BitcodeRecords,
                           array_lengthof(BitcodeRecords), Terminator);

  // Test that we accept <16 x i8>.
  EXPECT_TRUE(Munger.runTestForAssembly("Type <16 x i8>"));
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
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 1, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <1 x i8>", Vector1xI8, array_lengthof(Vector1xI8)));
  EXPECT_EQ(
      "Error(37:0): Vector type <1 x i8> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <1 x i8>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <2 x i8>.
  const uint64_t Vector2xI8[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 2, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <2 x i8>", Vector2xI8, array_lengthof(Vector2xI8)));
  EXPECT_EQ(
      "Error(37:0): Vector type <2 x i8> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <2 x i8>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <3 x i8>.
  const uint64_t Vector3xI8[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 3, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <3 x i8>", Vector3xI8, array_lengthof(Vector3xI8)));
  EXPECT_EQ(
      "Error(37:0): Vector type <3 x i8> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <3 x i8>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <4 x i8>.
  const uint64_t Vector4xI8[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 4, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <4 x i8>", Vector4xI8, array_lengthof(Vector4xI8)));
  EXPECT_EQ(
      "Error(37:0): Vector type <4 x i8> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <4 x i8>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <8 x i8>.
  const uint64_t Vector8xI8[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 8, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <8 x i8>", Vector8xI8, array_lengthof(Vector8xI8)));
  EXPECT_EQ(
      "Error(37:0): Vector type <8 x i8> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <8 x i8>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <32 x i8>.
  const uint64_t Vector32xI8[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 32, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <32 x i8>", Vector32xI8, array_lengthof(Vector32xI8)));
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

  NaClBitcodeMunger Munger(BitcodeRecords,
                           array_lengthof(BitcodeRecords), Terminator);

  // Test that we accept <8 x i16>.
  EXPECT_TRUE(Munger.runTestForAssembly("Type <16 x i16>"));
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
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 1, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <1 x i16>", Vector1xI16, array_lengthof(Vector1xI16)));
  EXPECT_EQ(
      "Error(37:0): Vector type <1 x i16> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <1 x i16>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <2 x i16>.
  const uint64_t Vector2xI16[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 2, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <2 x i16>", Vector2xI16, array_lengthof(Vector2xI16)));
  EXPECT_EQ(
      "Error(37:0): Vector type <2 x i16> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <2 x i16>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <3 x i16>.
  const uint64_t Vector3xI16[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 3, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <3 x i16>", Vector3xI16, array_lengthof(Vector3xI16)));
  EXPECT_EQ(
      "Error(37:0): Vector type <3 x i16> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <3 x i16>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <4 x i16>.
  const uint64_t Vector4xI16[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 4, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <4 x i16>", Vector4xI16, array_lengthof(Vector4xI16)));
  EXPECT_EQ(
      "Error(37:0): Vector type <4 x i16> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <4 x i16>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <16 x i16>.
  const uint64_t Vector16xI16[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 16, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <16 x i16>", Vector16xI16, array_lengthof(Vector16xI16)));
  EXPECT_EQ(
      "Error(37:0): Vector type <16 x i16> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <16 x i16>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <32 x i16>.
  const uint64_t Vector32xI16[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 32, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <32 x i16>", Vector32xI16, array_lengthof(Vector32xI16)));
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

  NaClBitcodeMunger Munger(BitcodeRecords,
                           array_lengthof(BitcodeRecords), Terminator);

  // Test that we accept <4 x i32>.
  EXPECT_TRUE(Munger.runTestForAssembly("Type <4 x i32>"));
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
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 1, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <1 x i32>", Vector1xI32, array_lengthof(Vector1xI32)));
  EXPECT_EQ(
      "Error(37:6): Vector type <1 x i32> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <1 x i32>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <2 x i32>.
  const uint64_t Vector2xI32[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 2, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <2 x i32>", Vector2xI32, array_lengthof(Vector2xI32)));
  EXPECT_EQ(
      "Error(37:6): Vector type <2 x i32> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <2 x i32>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <3 x i32>.
  const uint64_t Vector3xI32[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 3, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <3 x i32>", Vector3xI32, array_lengthof(Vector3xI32)));
  EXPECT_EQ(
      "Error(37:6): Vector type <3 x i32> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <3 x i32>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <8 x i32>.
  const uint64_t Vector8xI32[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 8, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <8 x i32>", Vector8xI32, array_lengthof(Vector8xI32)));
  EXPECT_EQ(
      "Error(37:6): Vector type <8 x i32> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <8 x i32>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <16 x i32>.
  const uint64_t Vector16xI32[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 16, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <16 x i32>", Vector16xI32, array_lengthof(Vector16xI32)));
  EXPECT_EQ(
      "Error(37:6): Vector type <16 x i32> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <16 x i32>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <32 x i32>.
  const uint64_t Vector32xI32[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 32, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <32 x i32>", Vector32xI32, array_lengthof(Vector32xI32)));
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

  NaClBitcodeMunger Munger(BitcodeRecords,
                           array_lengthof(BitcodeRecords), Terminator);

  // Test that we reject <1 x i64>.
  EXPECT_FALSE(Munger.runTestForAssembly("Type <1 x i64>"));
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
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 2, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <2 x i64>", Vector2xI64, array_lengthof(Vector2xI64)));
  EXPECT_EQ(
      "Error(37:6): Vector type <2 x i64> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <2 x i64>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <3 x i64>.
  const uint64_t Vector3xI64[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 3, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <3 x i64>", Vector3xI64, array_lengthof(Vector3xI64)));
  EXPECT_EQ(
      "Error(37:6): Vector type <3 x i64> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <3 x i64>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we reject <4 x i64>.
  const uint64_t Vector4xI64[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 4, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <4 x i64>", Vector4xI64, array_lengthof(Vector4xI64)));
  EXPECT_EQ(
      "Error(37:6): Vector type <4 x i64> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <4 x i64>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <8 x i64>.
  const uint64_t Vector8xI64[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 8, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <8 x i64>", Vector8xI64, array_lengthof(Vector8xI64)));
  EXPECT_EQ(
      "Error(37:6): Vector type <8 x i64> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <8 x i64>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <16 x i64>.
  const uint64_t Vector16xI64[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 16, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <16 x i64>", Vector16xI64, array_lengthof(Vector16xI64)));
  EXPECT_EQ(
      "Error(37:6): Vector type <16 x i64> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <16 x i64>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we don't handle <32 x i64>.
  const uint64_t Vector32xI64[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 32, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <32 x i64>", Vector32xI64, array_lengthof(Vector32xI64)));
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

  NaClBitcodeMunger Munger(BitcodeRecords,
                           array_lengthof(BitcodeRecords), Terminator);

  // Test that we accept <4 x float>.
  EXPECT_TRUE(Munger.runTestForAssembly("Type <4 x float>"));
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
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 1, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <1 x float>", Vector1xFloat, array_lengthof(Vector1xFloat)));
  EXPECT_EQ(
      "Error(36:2): Vector type <1 x float> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <1 x float>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we reject <2 x float>.
  const uint64_t Vector2xFloat[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 2, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <2 x float>", Vector2xFloat, array_lengthof(Vector2xFloat)));
  EXPECT_EQ(
      "Error(36:2): Vector type <2 x float> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <2 x float>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we reject <3 x float>.
  const uint64_t Vector3xFloat[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 3, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <3 x float>", Vector3xFloat, array_lengthof(Vector3xFloat)));
  EXPECT_EQ(
      "Error(36:2): Vector type <3 x float> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <3 x float>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we reject <8 x float>.
  const uint64_t Vector8xFloat[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 8, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <8 x float>", Vector8xFloat, array_lengthof(Vector8xFloat)));
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

  NaClBitcodeMunger Munger(BitcodeRecords,
                           array_lengthof(BitcodeRecords), Terminator);

  // Test that we reject <4 x double>.
  EXPECT_FALSE(Munger.runTestForAssembly("Type <4 x double>"));
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
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 1, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <1 x double>", Vector1xDouble, array_lengthof(Vector1xDouble)));
  EXPECT_EQ(
      "Error(36:2): Vector type <1 x double> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <1 x double>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we reject <2 x double>.
  const uint64_t Vector2xDouble[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 2, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <2 x double>", Vector2xDouble, array_lengthof(Vector2xDouble)));
  EXPECT_EQ(
      "Error(36:2): Vector type <2 x double> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <2 x double>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we reject <4 x double>.
  const uint64_t Vector3xDouble[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 4, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <4 x double>", Vector3xDouble, array_lengthof(Vector3xDouble)));
  EXPECT_EQ(
      "Error(36:2): Vector type <4 x double> not allowed.\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t1 = <4 x double>;\n",
      Munger.getLinesWithSubstring("@t1"));

  // Test that we reject <8 x double>.
  const uint64_t Vector8xDouble[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 12, 8, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Type <8 x double>", Vector8xDouble, array_lengthof(Vector8xDouble)));
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
  NaClBitcodeMunger Munger(BitcodeRecords,
                           array_lengthof(BitcodeRecords), Terminator);
  EXPECT_FALSE(Munger.runTestForAssembly("Type <4 x void>"));
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
  NaClBitcodeMunger Munger(BitcodeRecords,
                           array_lengthof(BitcodeRecords), Terminator);
  EXPECT_FALSE(Munger.runTestForAssembly("Type <4 x <4 x float>>"));
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

  NaClBitcodeMunger Munger(BitcodeRecords,
                           array_lengthof(BitcodeRecords), Terminator);

  // Test void() signature.
  EXPECT_TRUE(Munger.runTestForAssembly("Type void()"));
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
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 21, 0, 1, 2, 1, Terminator
  };
  EXPECT_TRUE(Munger.runTestForAssembly(
      "Function record with integer types", UsesIntegerTypes,
      array_lengthof(UsesIntegerTypes)));
  EXPECT_EQ(
      "    @t6 = i16 (i32, i16);\n",
      Munger.getLinesWithSubstring("@t6"));

  // Test using float point types for parameters and return types.
  const uint64_t UsesFloatingTypes[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 21, 0, 3, 3, 4, Terminator
  };
  EXPECT_TRUE(Munger.runTestForAssembly(
      "Function record with floating point types", UsesFloatingTypes,
      array_lengthof(UsesFloatingTypes)));
  EXPECT_EQ(
      "    @t6 = float (float, double);\n",
      Munger.getLinesWithSubstring("@t6"));

  // Test using vector types for parameters and return types.
  const uint64_t UsesVectorTypes[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 21, 0, 5, 5, Terminator
  };
  EXPECT_TRUE(Munger.runTestForAssembly(
      "Function record with vector types", UsesVectorTypes,
      array_lengthof(UsesVectorTypes)));
  EXPECT_EQ(
      "    @t6 = <4 x i32> (<4 x i32>);\n",
      Munger.getLinesWithSubstring("@t6"));

  // Test error if function record is too short.
  const uint64_t FunctionRecordTooShort[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 21, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Function record too short", FunctionRecordTooShort,
      array_lengthof(FunctionRecordTooShort)));
  EXPECT_EQ(
      "Error(48:6): Function record should contain at least 2 arguments. "
      "Found: 1\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t6 = void;\n",
      Munger.getLinesWithSubstring("@t6"));

  // Tests errror if function record specifies varargs.
  const uint64_t FunctionRecordWithVarArgs[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 21, 1, 0, Terminator
  };
  EXPECT_FALSE(
      Munger.runTestForAssembly(
          "Function record with varargs", FunctionRecordWithVarArgs,
          array_lengthof(FunctionRecordWithVarArgs)));
  EXPECT_EQ(
      "Error(48:6): Functions with variable length arguments is "
      "not supported\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t6 = void (...);\n",
      Munger.getLinesWithSubstring("@t6"));

  // Tests if void is used as a parameter type.
  const uint64_t VoidParamType[] = {
    ReplaceIndex, NaClBitcodeMunger::Replace, 3, 21, 0, 0, 0, Terminator
  };
  EXPECT_FALSE(Munger.runTestForAssembly(
      "Function record with void param type", VoidParamType,
      array_lengthof(VoidParamType)));
  EXPECT_EQ(
      "Error(48:6): Invalid type for parameter 1. Found: void. Assuming: i32\n",
      Munger.getLinesWithPrefix(ErrorPrefix));
  EXPECT_EQ(
      "    @t6 = void (i32);\n",
      Munger.getLinesWithSubstring("@t6"));

  // Tests using a function type as the return type.
  const uint64_t FunctionReturnType[] = {
    TypeCountIndex, NaClBitcodeMunger::Replace, 3, 1, 8, Terminator,
    ReplaceIndex, NaClBitcodeMunger::AddAfter, 3, 21, 0, 6, Terminator
  };
  EXPECT_FALSE(
      Munger.runTestForAssembly(
          "Function record with function return type", FunctionReturnType,
          array_lengthof(FunctionReturnType)));
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
    TypeCountIndex, NaClBitcodeMunger::Replace, 3, 1, 8, Terminator,
    ReplaceIndex, NaClBitcodeMunger::AddAfter, 3, 21, 0, 0, 6, Terminator
  };
  EXPECT_FALSE(
      Munger.runTestForAssembly(
          "Function record with function param type", FunctionParamType,
          array_lengthof(FunctionParamType)));
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
  NaClBitcodeMunger Munger(BitcodeRecords,
                           array_lengthof(BitcodeRecords), Terminator);
  EXPECT_FALSE(Munger.runTestForAssembly("Unknown types record code"));
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

} // End of anonymous namespace.
