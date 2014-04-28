//===- llvm/unittest/Bitcode/NaClObjDumpTest.cpp -------------------------===//
//     Tests objdump stream for PNaCl bitcode.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// Tests if the objdump stream for PNaCl bitcode works as expected.

#include "llvm/Bitcode/NaCl/NaClObjDumpStream.h"

#include "gtest/gtest.h"

#include <iostream>

using namespace llvm;
using namespace llvm::naclbitc;

namespace {

// Writes out the record, if non-null. Otherwise just writes comments
// and errors.
static inline void Write(ObjDumpStream &Stream, uint64_t Bit,
                         const NaClBitcodeRecordData *Record) {
  if (Record)
    Stream.Write(Bit, *Record);
  else
    Stream.Flush();
}

// Runs some simple assembly examples against the given bitcode
// record, using an objdump stream.
static void RunAssemblyExamples(
    ObjDumpStream &Stream, uint64_t Bit,
    const NaClBitcodeRecordData *Record,
    bool AddErrors) {

  // First assume no assembly.
  if (AddErrors)
    Stream.Error(Bit) << "This is an error\n";
  Write(Stream, Bit, Record);
  // Increment bit to new fictitious address, assuming Record takes 21 bits.
  Bit += 21;

  // Now a single line assembly.
  if (AddErrors)
    Stream.Error(Bit) << "Oops, an error!\n";
  Stream.Assembly() << "One line assembly.";
  Write(Stream, Bit, Record);
  // Increment bit to new fictitious address, assuming Record takes 17 bits.
  Bit += 17;

  // Now multiple line assembly.
  if (AddErrors)
    Stream.Error(Bit) << "The record looks bad\n";
  Stream.Assembly() << "Two Line\nexample assembly.";
  if (AddErrors)
    Stream.Error(Bit) << "Actually, it looks really bad\n";
  Write(Stream, Bit, Record);
}

// Runs some simple assembly examples against the given bitcode
// record, using an objdump stream. Adds a message describing the test
// and the record indent being used.
static void RunLabeledAssemblyExamples(
    const std::string TestName,
    ObjDumpStream &Stream, uint64_t Bit,
    const NaClBitcodeRecordData *Record, bool AddErrors) {
  Stream.Comments() << "Testing " << TestName << " with record indent "
                    << Stream.GetRecordIndenter().GetNumTabs() << "\n";
  Stream.Flush();
  RunAssemblyExamples(Stream, Bit, Record, AddErrors);
}

// Runs some simple assembly examples against the given bitcode record
// using an objdump stream. Adds a message describing the test
// and the record indent being used.
static std::string RunIndentedAssemblyTest(
    const std::string TestName,
    bool DumpRecords, bool DumpAssembly,
    unsigned NumRecordIndents, uint64_t Bit,
    const NaClBitcodeRecordData *Record, bool AddErrors) {
  std::string Buffer;
  raw_string_ostream BufStream(Buffer);
  ObjDumpStream DumpStream(BufStream, DumpRecords, DumpAssembly);
  for (unsigned i = 0; i < NumRecordIndents; ++i) {
    DumpStream.IncRecordIndent();
  }
  RunLabeledAssemblyExamples(TestName, DumpStream, Bit, Record, AddErrors);
  return BufStream.str();
}

// Tests effects of objdump when there isn't a record to write.
TEST(NaClObjDumpTest, NoDumpRecords) {
  EXPECT_EQ(
      "Testing NoDumpRecords(t,t) with record indent 0\n"
      "                                        |One line assembly.\n"
      "                                        |Two Line\n"
      "                                        |example assembly.\n",
      RunIndentedAssemblyTest("NoDumpRecords(t,t)",
                              true, true, 0, 11, 0, false));

  EXPECT_EQ(
      "Testing NoDumpRecords(f,t) with record indent 0\n"
      "One line assembly.\n"
      "Two Line\n"
      "example assembly.\n",
      RunIndentedAssemblyTest("NoDumpRecords(f,t)",
                              false, true, 0, 91, 0, false));

  EXPECT_EQ(
      "Testing NoDumpRecords(t,f) with record indent 0\n",
      RunIndentedAssemblyTest("NoDumpRecords(t,f)",
                              true, false, 0, 37, 0, false));


  EXPECT_EQ(
      "Testing NoDumpRecords(f,f) with record indent 0\n",
      RunIndentedAssemblyTest("NoDumpRecords(f,f)",
                              false, false, 0, 64, 0, false));
}

// Tests simple cases where there is both a record and corresponding
// assembly code.
TEST(NaClObjDumpTest, SimpleRecords) {
  NaClBitcodeRecordData Record;
  Record.Code = 5;
  Record.Values.push_back(10);
  Record.Values.push_back(15);

  EXPECT_EQ(
      "Testing SimpleRecords(t,t) with record indent 0\n"
      "       1:3 <5, 10, 15>                  |\n"
      "       4:0 <5, 10, 15>                  |One line assembly.\n"
      "       6:1 <5, 10, 15>                  |Two Line\n"
      "                                        |example assembly.\n",
      RunIndentedAssemblyTest("SimpleRecords(t,t)",
                              true, true, 0, 11, &Record, false));

  EXPECT_EQ(
      "Testing SimpleRecords(f,t) with record indent 0\n"
      "One line assembly.\n"
      "Two Line\n"
      "example assembly.\n",
      RunIndentedAssemblyTest("SimpleRecords(f,t)",
                              false, true, 0, 91, &Record, false));

  EXPECT_EQ(
      "Testing SimpleRecords(t,f) with record indent 0\n"
      "       4:5 <5, 10, 15>\n"
      "       7:2 <5, 10, 15>\n"
      "       9:3 <5, 10, 15>\n",
      RunIndentedAssemblyTest("SimpleRecords(t,f)",
                              true, false, 0, 37, &Record, false));

  EXPECT_EQ(
      "Testing SimpleRecords(f,f) with record indent 0\n",
      RunIndentedAssemblyTest("SimpleRecords(f,f)",
                              false, false, 0, 64, &Record, false));
}

// Test case where record is printed using two lines.
TEST(NaClObjDumpText, LongRecords) {
  NaClBitcodeRecordData Record;
  Record.Code = 5;
  Record.Values.push_back(static_cast<uint64_t>(-1));
  Record.Values.push_back(100);
  Record.Values.push_back(15);
  Record.Values.push_back(107056);

  EXPECT_EQ(
      "Testing LongRecords(t,t) with record indent 0\n"
      "     127:1 <5, 18446744073709551615,    |\n"
      "            100, 15, 107056>            |\n"
      "     129:6 <5, 18446744073709551615,    |One line assembly.\n"
      "            100, 15, 107056>            |\n"
      "     131:7 <5, 18446744073709551615,    |Two Line\n"
      "            100, 15, 107056>            |example assembly.\n",
      RunIndentedAssemblyTest("LongRecords(t,t)",
                              true, true, 0, 1017, &Record, false));

  EXPECT_EQ(
      "Testing LongRecords(f,t) with record indent 0\n"
      "One line assembly.\n"
      "Two Line\n"
      "example assembly.\n",
      RunIndentedAssemblyTest("LongRecords(f,t)",
                              false, true, 0, 91, &Record, false));

  EXPECT_EQ(
      "Testing LongRecords(t,f) with record indent 0\n"
      "   47073:6 <5, 18446744073709551615, 100, 15, 107056>\n"
      "   47076:3 <5, 18446744073709551615, 100, 15, 107056>\n"
      "   47078:4 <5, 18446744073709551615, 100, 15, 107056>\n",
      RunIndentedAssemblyTest("LongRecords(t,f)",
                              true, false, 0, 376590, &Record, false));

  EXPECT_EQ(
      "Testing LongRecords(f,f) with record indent 0\n",
      RunIndentedAssemblyTest("LongRecords(f,f)",
                              false, false, 0, 64564, &Record, false));
}

// Test case where comma hits boundary.
TEST(NaClObjDumpText, CommaBoundaryRecords) {
  NaClBitcodeRecordData Record;
  Record.Code = 5;
  Record.Values.push_back(static_cast<uint64_t>(-1));
  Record.Values.push_back(10);
  Record.Values.push_back(15);
  Record.Values.push_back(107056);

  EXPECT_EQ(
      "Testing CommaBoundaryRecords(t,t) with record indent 0\n"
      "     127:1 <5, 18446744073709551615, 10,|\n"
      "            15, 107056>                 |\n"
      "     129:6 <5, 18446744073709551615, 10,|One line assembly.\n"
      "            15, 107056>                 |\n"
      "     131:7 <5, 18446744073709551615, 10,|Two Line\n"
      "            15, 107056>                 |example assembly.\n",
      RunIndentedAssemblyTest("CommaBoundaryRecords(t,t)",
                              true, true, 0, 1017, &Record, false));

  EXPECT_EQ(
      "Testing CommaBoundaryRecords(f,t) with record indent 0\n"
      "One line assembly.\n"
      "Two Line\n"
      "example assembly.\n",
      RunIndentedAssemblyTest("CommaBoundaryRecords(f,t)",
                              false, true, 0, 91, &Record, false));

  EXPECT_EQ(
      "Testing CommaBoundaryRecords(t,f) with record indent 0\n"
      "   47073:6 <5, 18446744073709551615, 10, 15, 107056>\n"
      "   47076:3 <5, 18446744073709551615, 10, 15, 107056>\n"
      "   47078:4 <5, 18446744073709551615, 10, 15, 107056>\n",
      RunIndentedAssemblyTest("CommaBoundaryRecords(t,f)",
                              true, false, 0, 376590, &Record, false));

  EXPECT_EQ(
      "Testing CommaBoundaryRecords(f,f) with record indent 0\n",
      RunIndentedAssemblyTest("CommaBoundaryRecords(f,f)",
                              false, false, 0, 64564, &Record, false));
}

// Test case where comma wraps to next line.
TEST(NaClObjDumpText, CommaWrapRecords) {
  NaClBitcodeRecordData Record;
  Record.Code = 5;
  Record.Values.push_back(static_cast<uint64_t>(-1));
  Record.Values.push_back(100);
  Record.Values.push_back(15);
  Record.Values.push_back(107056);

  EXPECT_EQ(
      "Testing CommaWrapRecords(t,t) with record indent 0\n"
      "     127:1 <5, 18446744073709551615,    |\n"
      "            100, 15, 107056>            |\n"
      "     129:6 <5, 18446744073709551615,    |One line assembly.\n"
      "            100, 15, 107056>            |\n"
      "     131:7 <5, 18446744073709551615,    |Two Line\n"
      "            100, 15, 107056>            |example assembly.\n",
      RunIndentedAssemblyTest("CommaWrapRecords(t,t)",
                              true, true, 0, 1017, &Record, false));

  EXPECT_EQ(
      "Testing CommaWrapRecords(f,t) with record indent 0\n"
      "One line assembly.\n"
      "Two Line\n"
      "example assembly.\n",
      RunIndentedAssemblyTest("CommaWrapRecords(f,t)",
                              false, true, 0, 91, &Record, false));

  EXPECT_EQ(
      "Testing CommaWrapRecords(t,f) with record indent 0\n"
      "   47073:6 <5, 18446744073709551615, 100, 15, 107056>\n"
      "   47076:3 <5, 18446744073709551615, 100, 15, 107056>\n"
      "   47078:4 <5, 18446744073709551615, 100, 15, 107056>\n",
      RunIndentedAssemblyTest("CommaWrapRecords(t,f)",
                              true, false, 0, 376590, &Record, false));

  EXPECT_EQ(
      "Testing CommaWrapRecords(f,f) with record indent 0\n",
      RunIndentedAssemblyTest("CommaWrapRecords(f,f)",
                              false, false, 0, 64564, &Record, false));
}

// Test case where record is printed using more than two lines.
TEST(NaClObjDumpText, VeryLongRecords) {
  NaClBitcodeRecordData Record;
  Record.Code = 5;
  Record.Values.push_back(static_cast<uint64_t>(-1));
  Record.Values.push_back(100);
  Record.Values.push_back(15);
  Record.Values.push_back(107056);
  Record.Values.push_back(static_cast<uint64_t>(-5065));
  Record.Values.push_back(101958788);

  EXPECT_EQ(
      "Testing VeryLongRecords(t,t) with record indent 0\n"
      "     127:1 <5, 18446744073709551615,    |\n"
      "            100, 15, 107056,            |\n"
      "            18446744073709546551,       |\n"
      "            101958788>                  |\n"
      "     129:6 <5, 18446744073709551615,    |One line assembly.\n"
      "            100, 15, 107056,            |\n"
      "            18446744073709546551,       |\n"
      "            101958788>                  |\n"
      "     131:7 <5, 18446744073709551615,    |Two Line\n"
      "            100, 15, 107056,            |example assembly.\n"
      "            18446744073709546551,       |\n"
      "            101958788>                  |\n",
      RunIndentedAssemblyTest("VeryLongRecords(t,t)",
                              true, true, 0, 1017, &Record, false));

  EXPECT_EQ(
      "Testing VeryLongRecords(f,t) with record indent 0\n"
      "One line assembly.\n"
      "Two Line\n"
      "example assembly.\n",
      RunIndentedAssemblyTest("VeryLongRecords(f,t)",
                              false, true, 0, 91, &Record, false));

  EXPECT_EQ(
"Testing VeryLongRecords(t,f) with record indent 0\n"
"   47073:6 <5, 18446744073709551615, 100, 15, 107056, 18446744073709546551, \n"
"            101958788>\n"
"   47076:3 <5, 18446744073709551615, 100, 15, 107056, 18446744073709546551, \n"
"            101958788>\n"
"   47078:4 <5, 18446744073709551615, 100, 15, 107056, 18446744073709546551, \n"
"            101958788>\n",
      RunIndentedAssemblyTest("VeryLongRecords(t,f)",
                              true, false, 0, 376590, &Record, false));

  EXPECT_EQ(
      "Testing VeryLongRecords(f,f) with record indent 0\n",
      RunIndentedAssemblyTest("VeryLongRecords(f,f)",
                              false, false, 0, 64564, &Record, false));
}


// Tests effects of objdump when there isn't a record to write, but errors occur.
TEST(NaClObjDumpTest, ErrorsErrorsNoDumpRecords) {

  EXPECT_EQ(
      "Testing ErrorsNoDumpRecords(t,t) with record indent 0\n"
      "Error(1:3): This is an error\n"
      "                                        |One line assembly.\n"
      "Error(4:0): Oops, an error!\n"
      "                                        |Two Line\n"
      "                                        |example assembly.\n"
      "Error(6:1): The record looks bad\n"
      "Error(6:1): Actually, it looks really bad\n",
      RunIndentedAssemblyTest("ErrorsNoDumpRecords(t,t)",
                              true, true, 0, 11, 0, true));

  EXPECT_EQ(
      "Testing ErrorsNoDumpRecords(f,t) with record indent 0\n"
      "Error(11:3): This is an error\n"
      "One line assembly.\n"
      "Error(14:0): Oops, an error!\n"
      "Two Line\n"
      "example assembly.\n"
      "Error(16:1): The record looks bad\n"
      "Error(16:1): Actually, it looks really bad\n",
      RunIndentedAssemblyTest("ErrorsNoDumpRecords(f,t)",
                              false, true, 0, 91, 0, true));

  EXPECT_EQ(
      "Testing ErrorsNoDumpRecords(t,f) with record indent 0\n"
      "Error(4:5): This is an error\n"
      "Error(7:2): Oops, an error!\n"
      "Error(9:3): The record looks bad\n"
      "Error(9:3): Actually, it looks really bad\n",
      RunIndentedAssemblyTest("ErrorsNoDumpRecords(t,f)",
                              true, false, 0, 37, 0, true));


  EXPECT_EQ(
      "Testing ErrorsNoDumpRecords(f,f) with record indent 0\n"
      "Error(8:0): This is an error\n"
      "Error(10:5): Oops, an error!\n"
      "Error(12:6): The record looks bad\n"
      "Error(12:6): Actually, it looks really bad\n",
      RunIndentedAssemblyTest("ErrorsNoDumpRecords(f,f)",
                              false, false, 0, 64, 0, true));
}

// Test case where record is printed using two lines, but errors
// occur.
TEST(NaClObjDumpText, ErrorsLongRecords) {
  NaClBitcodeRecordData Record;
  Record.Code = 5;
  Record.Values.push_back(static_cast<uint64_t>(-1));
  Record.Values.push_back(100);
  Record.Values.push_back(15);
  Record.Values.push_back(107056);

  EXPECT_EQ(
      "Testing ErrorsLongRecords(t,t) with record indent 0\n"
      "     127:1 <5, 18446744073709551615,    |\n"
      "            100, 15, 107056>            |\n"
      "Error(127:1): This is an error\n"
      "     129:6 <5, 18446744073709551615,    |One line assembly.\n"
      "            100, 15, 107056>            |\n"
      "Error(129:6): Oops, an error!\n"
      "     131:7 <5, 18446744073709551615,    |Two Line\n"
      "            100, 15, 107056>            |example assembly.\n"
      "Error(131:7): The record looks bad\n"
      "Error(131:7): Actually, it looks really bad\n",
      RunIndentedAssemblyTest("ErrorsLongRecords(t,t)",
                              true, true, 0, 1017, &Record, true));

  EXPECT_EQ(
      "Testing ErrorsLongRecords(f,t) with record indent 0\n"
      "Error(11:3): This is an error\n"
      "One line assembly.\n"
      "Error(14:0): Oops, an error!\n"
      "Two Line\n"
      "example assembly.\n"
      "Error(16:1): The record looks bad\n"
      "Error(16:1): Actually, it looks really bad\n",
      RunIndentedAssemblyTest("ErrorsLongRecords(f,t)",
                              false, true, 0, 91, &Record, true));

  EXPECT_EQ(
      "Testing ErrorsLongRecords(t,f) with record indent 0\n"
      "   47073:6 <5, 18446744073709551615, 100, 15, 107056>\n"
      "Error(47073:6): This is an error\n"
      "   47076:3 <5, 18446744073709551615, 100, 15, 107056>\n"
      "Error(47076:3): Oops, an error!\n"
      "   47078:4 <5, 18446744073709551615, 100, 15, 107056>\n"
      "Error(47078:4): The record looks bad\n"
      "Error(47078:4): Actually, it looks really bad\n",
      RunIndentedAssemblyTest("ErrorsLongRecords(t,f)",
                              true, false, 0, 376590, &Record, true));

  EXPECT_EQ(
      "Testing ErrorsLongRecords(f,f) with record indent 0\n"
      "Error(8070:4): This is an error\n"
      "Error(8073:1): Oops, an error!\n"
      "Error(8075:2): The record looks bad\n"
      "Error(8075:2): Actually, it looks really bad\n",
      RunIndentedAssemblyTest("ErrorsLongRecords(f,f)",
                              false, false, 0, 64564, &Record, true));

}

// Test case where record is printed using more than two lines, but
// errors occur.
TEST(NaClObjDumpText, ErrorsVeryLongRecords) {
  NaClBitcodeRecordData Record;
  Record.Code = 5;
  Record.Values.push_back(static_cast<uint64_t>(-1));
  Record.Values.push_back(100);
  Record.Values.push_back(15);
  Record.Values.push_back(107056);
  Record.Values.push_back(static_cast<uint64_t>(-5065));
  Record.Values.push_back(101958788);

  EXPECT_EQ(
      "Testing ErrorsVeryLongRecords(t,t) with record indent 0\n"
      "     127:1 <5, 18446744073709551615,    |\n"
      "            100, 15, 107056,            |\n"
      "            18446744073709546551,       |\n"
      "            101958788>                  |\n"
      "Error(127:1): This is an error\n"
      "     129:6 <5, 18446744073709551615,    |One line assembly.\n"
      "            100, 15, 107056,            |\n"
      "            18446744073709546551,       |\n"
      "            101958788>                  |\n"
      "Error(129:6): Oops, an error!\n"
      "     131:7 <5, 18446744073709551615,    |Two Line\n"
      "            100, 15, 107056,            |example assembly.\n"
      "            18446744073709546551,       |\n"
      "            101958788>                  |\n"
      "Error(131:7): The record looks bad\n"
      "Error(131:7): Actually, it looks really bad\n",
      RunIndentedAssemblyTest("ErrorsVeryLongRecords(t,t)",
                              true, true, 0, 1017, &Record, true));

  EXPECT_EQ(
      "Testing ErrorsVeryLongRecords(f,t) with record indent 0\n"
      "Error(11:3): This is an error\n"
      "One line assembly.\n"
      "Error(14:0): Oops, an error!\n"
      "Two Line\n"
      "example assembly.\n"
      "Error(16:1): The record looks bad\n"
      "Error(16:1): Actually, it looks really bad\n",
      RunIndentedAssemblyTest("ErrorsVeryLongRecords(f,t)",
                              false, true, 0, 91, &Record, true));

  EXPECT_EQ(
"Testing ErrorsVeryLongRecords(t,f) with record indent 0\n"
"   47073:6 <5, 18446744073709551615, 100, 15, 107056, 18446744073709546551, \n"
"            101958788>\n"
"Error(47073:6): This is an error\n"
"   47076:3 <5, 18446744073709551615, 100, 15, 107056, 18446744073709546551, \n"
"            101958788>\n"
"Error(47076:3): Oops, an error!\n"
"   47078:4 <5, 18446744073709551615, 100, 15, 107056, 18446744073709546551, \n"
"            101958788>\n"
"Error(47078:4): The record looks bad\n"
"Error(47078:4): Actually, it looks really bad\n",
      RunIndentedAssemblyTest("ErrorsVeryLongRecords(t,f)",
                              true, false, 0, 376590, &Record, true));

  EXPECT_EQ(
      "Testing ErrorsVeryLongRecords(f,f) with record indent 0\n"
      "Error(8070:4): This is an error\n"
      "Error(8073:1): Oops, an error!\n"
      "Error(8075:2): The record looks bad\n"
      "Error(8075:2): Actually, it looks really bad\n",
      RunIndentedAssemblyTest("ErrorsVeryLongRecords(f,f)",
                              false, false, 0, 64564, &Record, true));
}

// Tests effects of objdump when there isn't a record to write, and we indent.
TEST(NaClObjDumpTest, NoDumpIndentRecords) {

  EXPECT_EQ(
      "Testing NoDumpIndentRecords(t,t) with record indent 1\n"
      "                                        |One line assembly.\n"
      "                                        |Two Line\n"
      "                                        |example assembly.\n",
      RunIndentedAssemblyTest("NoDumpIndentRecords(t,t)",
                              true, true, 1, 11, 0, false));

  EXPECT_EQ(
      "Testing NoDumpIndentRecords(t,t) with record indent 2\n"
      "                                        |One line assembly.\n"
      "                                        |Two Line\n"
      "                                        |example assembly.\n",
      RunIndentedAssemblyTest("NoDumpIndentRecords(t,t)",
                              true, true, 2, 11, 0, false));

  EXPECT_EQ(
      "Testing NoDumpIndentRecords(f,t) with record indent 1\n"
      "One line assembly.\n"
      "Two Line\n"
      "example assembly.\n",
      RunIndentedAssemblyTest("NoDumpIndentRecords(f,t)",
                              false, true, 1, 91, 0, false));

  EXPECT_EQ(
      "Testing NoDumpIndentRecords(f,t) with record indent 2\n"
      "One line assembly.\n"
      "Two Line\n"
      "example assembly.\n",
      RunIndentedAssemblyTest("NoDumpIndentRecords(f,t)",
                              false, true, 2, 91, 0, false));
  EXPECT_EQ(
      "Testing NoDumpIndentRecords(t,f) with record indent 1\n",
      RunIndentedAssemblyTest("NoDumpIndentRecords(t,f)",
                              true, false, 1, 37, 0, false));

  EXPECT_EQ(
      "Testing NoDumpIndentRecords(t,f) with record indent 2\n",
      RunIndentedAssemblyTest("NoDumpIndentRecords(t,f)",
                              true, false, 2, 37, 0, false));

  EXPECT_EQ(
      "Testing NoDumpIndentRecords(f,f) with record indent 1\n",
      RunIndentedAssemblyTest("NoDumpIndentRecords(f,f)",
                              false, false, 1, 64, 0, false));

  EXPECT_EQ(
      "Testing NoDumpIndentRecords(f,f) with record indent 2\n",
      RunIndentedAssemblyTest("NoDumpIndentRecords(f,f)",
                              false, false, 2, 64, 0, false));
}

// Tests simple cases where there is both a record and corresponding
// assembly code, and the records are indented.
TEST(NaClObjDumpTest, SimpleIndentRecords) {
  NaClBitcodeRecordData Record;
  Record.Code = 5;
  Record.Values.push_back(10);
  Record.Values.push_back(15);

  EXPECT_EQ(
      "Testing SimpleIndentRecords(t,t) with record indent 1\n"
      "       1:3   <5, 10, 15>                |\n"
      "       4:0   <5, 10, 15>                |One line assembly.\n"
      "       6:1   <5, 10, 15>                |Two Line\n"
      "                                        |example assembly.\n",
      RunIndentedAssemblyTest("SimpleIndentRecords(t,t)",
                              true, true, 1, 11, &Record, false));

  EXPECT_EQ(
      "Testing SimpleIndentRecords(t,t) with record indent 2\n"
      "       1:3     <5, 10, 15>              |\n"
      "       4:0     <5, 10, 15>              |One line assembly.\n"
      "       6:1     <5, 10, 15>              |Two Line\n"
      "                                        |example assembly.\n",
      RunIndentedAssemblyTest("SimpleIndentRecords(t,t)",
                              true, true, 2, 11, &Record, false));

  EXPECT_EQ(
      "Testing SimpleIndentRecords(f,t) with record indent 1\n"
      "One line assembly.\n"
      "Two Line\n"
      "example assembly.\n",
      RunIndentedAssemblyTest("SimpleIndentRecords(f,t)",
                              false, true, 1, 91, &Record, false));

  EXPECT_EQ(
      "Testing SimpleIndentRecords(f,t) with record indent 2\n"
      "One line assembly.\n"
      "Two Line\n"
      "example assembly.\n",
      RunIndentedAssemblyTest("SimpleIndentRecords(f,t)",
                              false, true, 2, 91, &Record, false));

  EXPECT_EQ(
      "Testing SimpleIndentRecords(t,f) with record indent 1\n"
      "       4:5   <5, 10, 15>\n"
      "       7:2   <5, 10, 15>\n"
      "       9:3   <5, 10, 15>\n",
      RunIndentedAssemblyTest("SimpleIndentRecords(t,f)",
                              true, false, 1, 37, &Record, false));

  EXPECT_EQ(
      "Testing SimpleIndentRecords(t,f) with record indent 2\n"
      "       4:5     <5, 10, 15>\n"
      "       7:2     <5, 10, 15>\n"
      "       9:3     <5, 10, 15>\n",
      RunIndentedAssemblyTest("SimpleIndentRecords(t,f)",
                              true, false, 2, 37, &Record, false));

  EXPECT_EQ(
      "Testing SimpleIndentRecords(f,f) with record indent 1\n",
      RunIndentedAssemblyTest("SimpleIndentRecords(f,f)",
                              false, false, 1, 64, &Record, false));

  EXPECT_EQ(
      "Testing SimpleIndentRecords(f,f) with record indent 2\n",
      RunIndentedAssemblyTest("SimpleIndentRecords(f,f)",
                              false, false, 2, 64, &Record, false));

}

// Test case where record is printed using more than two lines.
TEST(NaClObjDumpText, VeryLongIndentRecords) {
  NaClBitcodeRecordData Record;
  Record.Code = 5;
  Record.Values.push_back(static_cast<uint64_t>(-1));
  Record.Values.push_back(100);
  Record.Values.push_back(15);
  Record.Values.push_back(107056);
  Record.Values.push_back(static_cast<uint64_t>(-5065));
  Record.Values.push_back(101958788);

  EXPECT_EQ(
      "Testing VeryLongIndentRecords(t,t) with record indent 1\n"
      "     127:1   <5, 18446744073709551615,  |\n"
      "              100, 15, 107056,          |\n"
      "              18446744073709546551,     |\n"
      "              101958788>                |\n"
      "     129:6   <5, 18446744073709551615,  |One line assembly.\n"
      "              100, 15, 107056,          |\n"
      "              18446744073709546551,     |\n"
      "              101958788>                |\n"
      "     131:7   <5, 18446744073709551615,  |Two Line\n"
      "              100, 15, 107056,          |example assembly.\n"
      "              18446744073709546551,     |\n"
      "              101958788>                |\n",
      RunIndentedAssemblyTest("VeryLongIndentRecords(t,t)",
                              true, true, 1, 1017, &Record, false));

  EXPECT_EQ(
      "Testing VeryLongIndentRecords(t,t) with record indent 3\n"
      "     127:1       <5,                    |\n"
      "                  18446744073709551615, |\n"
      "                  100, 15, 107056,      |\n"
      "                  18446744073709546551, |\n"
      "                  101958788>            |\n"
      "     129:6       <5,                    |One line assembly.\n"
      "                  18446744073709551615, |\n"
      "                  100, 15, 107056,      |\n"
      "                  18446744073709546551, |\n"
      "                  101958788>            |\n"
      "     131:7       <5,                    |Two Line\n"
      "                  18446744073709551615, |example assembly.\n"
      "                  100, 15, 107056,      |\n"
      "                  18446744073709546551, |\n"
      "                  101958788>            |\n",
      RunIndentedAssemblyTest("VeryLongIndentRecords(t,t)",
                              true, true, 3, 1017, &Record, false));

  EXPECT_EQ(
      "Testing VeryLongIndentRecords(f,t) with record indent 1\n"
      "One line assembly.\n"
      "Two Line\n"
      "example assembly.\n",
      RunIndentedAssemblyTest("VeryLongIndentRecords(f,t)",
                              false, true, 1, 91, &Record, false));

  EXPECT_EQ(
      "Testing VeryLongIndentRecords(f,t) with record indent 2\n"
      "One line assembly.\n"
      "Two Line\n"
      "example assembly.\n",
      RunIndentedAssemblyTest("VeryLongIndentRecords(f,t)",
                              false, true, 2, 91, &Record, false));

  EXPECT_EQ(
"Testing VeryLongIndentRecords(t,f) with record indent 1\n"
"   47073:6   <5, 18446744073709551615, 100, 15, 107056, 18446744073709546551, \n"
"              101958788>\n"
"   47076:3   <5, 18446744073709551615, 100, 15, 107056, 18446744073709546551, \n"
"              101958788>\n"
"   47078:4   <5, 18446744073709551615, 100, 15, 107056, 18446744073709546551, \n"
"              101958788>\n",
      RunIndentedAssemblyTest("VeryLongIndentRecords(t,f)",
                              true, false, 1, 376590, &Record, false));


  EXPECT_EQ(
      "Testing VeryLongIndentRecords(t,f) with record indent 5\n"
      "   47073:6           <5, 18446744073709551615, 100, 15, 107056, \n"
      "                      18446744073709546551, 101958788>\n"
      "   47076:3           <5, 18446744073709551615, 100, 15, 107056, \n"
      "                      18446744073709546551, 101958788>\n"
      "   47078:4           <5, 18446744073709551615, 100, 15, 107056, \n"
      "                      18446744073709546551, 101958788>\n",
      RunIndentedAssemblyTest("VeryLongIndentRecords(t,f)",
                              true, false, 5, 376590, &Record, false));

  EXPECT_EQ(
      "Testing VeryLongIndentRecords(f,f) with record indent 1\n",
      RunIndentedAssemblyTest("VeryLongIndentRecords(f,f)",
                              false, false, 1, 64564, &Record, false));

  EXPECT_EQ(
      "Testing VeryLongIndentRecords(f,f) with record indent 2\n",
      RunIndentedAssemblyTest("VeryLongIndentRecords(f,f)",
                              false, false, 2, 64564, &Record, false));
}

// Tests that Clustering doesn't effect (intraline) indenting.
TEST(NaClObjDumpTest, ClusterIndentInteraction) {
  std::string Buffer;
  raw_string_ostream BufStream(Buffer);
  ObjDumpStream Stream(BufStream, true, true);

  TextFormatter Formatter(Stream.Assembly(), 40, "  ");
  TokenTextDirective Comma(&Formatter, ",");
  SpaceTextDirective Space(&Formatter);
  OpenTextDirective OpenParen(&Formatter, "(");
  CloseTextDirective CloseParen(&Formatter, ")");
  StartClusteringDirective StartCluster(&Formatter);
  FinishClusteringDirective FinishCluster(&Formatter);
  EndlineTextDirective Endline(&Formatter);

  Formatter.Tokens() << "begin" << Space;
  // Generates text on single line, setting indent at "(".
  Formatter.Tokens()
      << StartCluster << "SomeReasonablylongText" << OpenParen << FinishCluster;
  // Generates a long cluster that should move to the next line.
  Formatter.Tokens()
      << StartCluster << "ThisIsBoring" << Space
      << "VeryBoring" << Space << "longggggggggggggggggg"
      << Space << "Example" << Comma << FinishCluster;
  Formatter.Tokens() << CloseParen << Comma << Endline;
  Stream.Flush();
  EXPECT_EQ(
"                                        |begin SomeReasonablylongText(\n"
"                                        |                    ThisIsBoring \n"
"                                        |                    VeryBoring \n"
"                                        |                    longggggggggggggggggg\n"
"                                        |                    Example,),\n",
BufStream.str());
}


}
