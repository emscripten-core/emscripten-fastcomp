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
                         const NaClBitcodeRecordData *Record,
                         int32_t AbbrevIndex) {
  if (Record)
    Stream.Write(Bit, *Record, AbbrevIndex);
  else
    Stream.Flush();
}

// Runs some simple assembly examples against the given bitcode
// record, using an objdump stream.
static void RunAssemblyExamples(
    ObjDumpStream &Stream, uint64_t Bit,
    const NaClBitcodeRecordData *Record,
    int32_t AbbrevIndex,
    bool AddErrors) {

  // First assume no assembly.
  if (AddErrors)
    Stream.Error(Bit) << "This is an error\n";
  Write(Stream, Bit, Record, AbbrevIndex);
  // Increment bit to new fictitious address, assuming Record takes 21 bits.
  Bit += 21;

  // Now a single line assembly.
  if (AddErrors)
    Stream.Error(Bit) << "Oops, an error!\n";
  Stream.Assembly() << "One line assembly.";
  Write(Stream, Bit, Record, AbbrevIndex);
  // Increment bit to new fictitious address, assuming Record takes 17 bits.
  Bit += 17;

  // Now multiple line assembly.
  if (AddErrors)
    Stream.Error(Bit) << "The record looks bad\n";
  Stream.Assembly() << "Two Line\nexample assembly.";
  if (AddErrors)
    Stream.Error(Bit) << "Actually, it looks really bad\n";
  Write(Stream, Bit, Record, AbbrevIndex);
}

// Runs some simple assembly examples against the given bitcode record
// using an objdump stream. Adds a message describing the test
// and the record indent being used.
static std::string RunIndentedAssemblyWithAbbrevTest(
    bool DumpRecords, bool DumpAssembly,
    unsigned NumRecordIndents, uint64_t Bit,
    const NaClBitcodeRecordData *Record, int32_t AbbrevIndex, bool AddErrors) {
  std::string Buffer;
  raw_string_ostream BufStream(Buffer);
  ObjDumpStream DumpStream(BufStream, DumpRecords, DumpAssembly);
  for (unsigned i = 0; i < NumRecordIndents; ++i) {
    DumpStream.IncRecordIndent();
  }
  RunAssemblyExamples(DumpStream, Bit, Record, AbbrevIndex, AddErrors);
  return BufStream.str();
}

// Runs some simple assembly examples against the given bitcode record
// using an objdump stream. Adds a message describing the test
// and the record indent being used. Assumes no abbreviation index
// is associated with the record.
static std::string RunIndentedAssemblyTest(
    bool DumpRecords, bool DumpAssembly,
    unsigned NumRecordIndents, uint64_t Bit,
    const NaClBitcodeRecordData *Record, bool AddErrors) {
  return
      RunIndentedAssemblyWithAbbrevTest(
          DumpRecords, DumpAssembly, NumRecordIndents, Bit, Record,
          naclbitc::ABBREV_INDEX_NOT_SPECIFIED, AddErrors);
}

// Tests effects of objdump when there isn't a record to write.
TEST(NaClObjDumpTest, NoDumpRecords) {
  EXPECT_EQ(
      "          |                             |One line assembly.\n"
      "          |                             |Two Line\n"
      "          |                             |example assembly.\n",
      RunIndentedAssemblyTest(true, true, 0, 11, 0, false));

  EXPECT_EQ(
      "One line assembly.\n"
      "Two Line\n"
      "example assembly.\n",
      RunIndentedAssemblyTest(false, true, 0, 91, 0, false));

  EXPECT_EQ(
      "",
      RunIndentedAssemblyTest(true, false, 0, 37, 0, false));


  EXPECT_EQ(
      "",
      RunIndentedAssemblyTest(false, false, 0, 64, 0, false));
}

// Tests simple cases where there is both a record and corresponding
// assembly code.
TEST(NaClObjDumpTest, SimpleRecords) {
  NaClBitcodeRecordData Record;
  Record.Code = 5;
  Record.Values.push_back(10);
  Record.Values.push_back(15);

  EXPECT_EQ(
      "       1:3|<5, 10, 15>                  |\n"
      "       4:0|<5, 10, 15>                  |One line assembly.\n"
      "       6:1|<5, 10, 15>                  |Two Line\n"
      "          |                             |example assembly.\n",
      RunIndentedAssemblyTest(true, true, 0, 11, &Record, false));

  EXPECT_EQ(
      "One line assembly.\n"
      "Two Line\n"
      "example assembly.\n",
      RunIndentedAssemblyTest(false, true, 0, 91, &Record, false));

  EXPECT_EQ(
      "       4:5|<5, 10, 15>\n"
      "       7:2|<5, 10, 15>\n"
      "       9:3|<5, 10, 15>\n",
      RunIndentedAssemblyTest(true, false, 0, 37, &Record, false));

  EXPECT_EQ(
      "",
      RunIndentedAssemblyTest(false, false, 0, 64, &Record, false));
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
      "     127:1|<5, 18446744073709551615,    |\n"
      "          | 100, 15, 107056>            |\n"
      "     129:6|<5, 18446744073709551615,    |One line assembly.\n"
      "          | 100, 15, 107056>            |\n"
      "     131:7|<5, 18446744073709551615,    |Two Line\n"
      "          | 100, 15, 107056>            |example assembly.\n",
      RunIndentedAssemblyTest(true, true, 0, 1017, &Record, false));

  EXPECT_EQ(
      "One line assembly.\n"
      "Two Line\n"
      "example assembly.\n",
      RunIndentedAssemblyTest(false, true, 0, 91, &Record, false));

  EXPECT_EQ(
      "   47073:6|<5, 18446744073709551615, 100, 15, 107056>\n"
      "   47076:3|<5, 18446744073709551615, 100, 15, 107056>\n"
      "   47078:4|<5, 18446744073709551615, 100, 15, 107056>\n",
      RunIndentedAssemblyTest(true, false, 0, 376590, &Record, false));

  EXPECT_EQ(
      "",
      RunIndentedAssemblyTest(false, false, 0, 64564, &Record, false));
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
      "     127:1|<5, 18446744073709551615, 10,|\n"
      "          | 15, 107056>                 |\n"
      "     129:6|<5, 18446744073709551615, 10,|One line assembly.\n"
      "          | 15, 107056>                 |\n"
      "     131:7|<5, 18446744073709551615, 10,|Two Line\n"
      "          | 15, 107056>                 |example assembly.\n",
      RunIndentedAssemblyTest(true, true, 0, 1017, &Record, false));

  EXPECT_EQ(
      "One line assembly.\n"
      "Two Line\n"
      "example assembly.\n",
      RunIndentedAssemblyTest(false, true, 0, 91, &Record, false));

  EXPECT_EQ(
      "   47073:6|<5, 18446744073709551615, 10, 15, 107056>\n"
      "   47076:3|<5, 18446744073709551615, 10, 15, 107056>\n"
      "   47078:4|<5, 18446744073709551615, 10, 15, 107056>\n",
      RunIndentedAssemblyTest(true, false, 0, 376590, &Record, false));

  EXPECT_EQ(
      "",
      RunIndentedAssemblyTest(false, false, 0, 64564, &Record, false));
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
      "     127:1|<5, 18446744073709551615,    |\n"
      "          | 100, 15, 107056>            |\n"
      "     129:6|<5, 18446744073709551615,    |One line assembly.\n"
      "          | 100, 15, 107056>            |\n"
      "     131:7|<5, 18446744073709551615,    |Two Line\n"
      "          | 100, 15, 107056>            |example assembly.\n",
      RunIndentedAssemblyTest(true, true, 0, 1017, &Record, false));

  EXPECT_EQ(
      "One line assembly.\n"
      "Two Line\n"
      "example assembly.\n",
      RunIndentedAssemblyTest(false, true, 0, 91, &Record, false));

  EXPECT_EQ(
      "   47073:6|<5, 18446744073709551615, 100, 15, 107056>\n"
      "   47076:3|<5, 18446744073709551615, 100, 15, 107056>\n"
      "   47078:4|<5, 18446744073709551615, 100, 15, 107056>\n",
      RunIndentedAssemblyTest(true, false, 0, 376590, &Record, false));

  EXPECT_EQ(
      "",
      RunIndentedAssemblyTest(false, false, 0, 64564, &Record, false));
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
      "     127:1|<5, 18446744073709551615,    |\n"
      "          | 100, 15, 107056,            |\n"
      "          | 18446744073709546551,       |\n"
      "          | 101958788>                  |\n"
      "     129:6|<5, 18446744073709551615,    |One line assembly.\n"
      "          | 100, 15, 107056,            |\n"
      "          | 18446744073709546551,       |\n"
      "          | 101958788>                  |\n"
      "     131:7|<5, 18446744073709551615,    |Two Line\n"
      "          | 100, 15, 107056,            |example assembly.\n"
      "          | 18446744073709546551,       |\n"
      "          | 101958788>                  |\n",
      RunIndentedAssemblyTest(true, true, 0, 1017, &Record, false));

  EXPECT_EQ(
      "One line assembly.\n"
      "Two Line\n"
      "example assembly.\n",
      RunIndentedAssemblyTest(false, true, 0, 91, &Record, false));

  EXPECT_EQ(
"   47073:6|<5, 18446744073709551615, 100, 15, 107056, 18446744073709546551, \n"
"          | 101958788>\n"
"   47076:3|<5, 18446744073709551615, 100, 15, 107056, 18446744073709546551, \n"
"          | 101958788>\n"
"   47078:4|<5, 18446744073709551615, 100, 15, 107056, 18446744073709546551, \n"
"          | 101958788>\n",
      RunIndentedAssemblyTest(true, false, 0, 376590, &Record, false));

  EXPECT_EQ(
      "",
      RunIndentedAssemblyTest(false, false, 0, 64564, &Record, false));
}


// Tests effects of objdump when there isn't a record to write, but errors occur.
TEST(NaClObjDumpTest, ErrorsErrorsNoDumpRecords) {

  EXPECT_EQ(
      "Error(1:3): This is an error\n"
      "          |                             |One line assembly.\n"
      "Error(4:0): Oops, an error!\n"
      "          |                             |Two Line\n"
      "          |                             |example assembly.\n"
      "Error(6:1): The record looks bad\n"
      "Error(6:1): Actually, it looks really bad\n",
      RunIndentedAssemblyTest(true, true, 0, 11, 0, true));

  EXPECT_EQ(
      "Error(11:3): This is an error\n"
      "One line assembly.\n"
      "Error(14:0): Oops, an error!\n"
      "Two Line\n"
      "example assembly.\n"
      "Error(16:1): The record looks bad\n"
      "Error(16:1): Actually, it looks really bad\n",
      RunIndentedAssemblyTest(false, true, 0, 91, 0, true));

  EXPECT_EQ(
      "Error(4:5): This is an error\n"
      "Error(7:2): Oops, an error!\n"
      "Error(9:3): The record looks bad\n"
      "Error(9:3): Actually, it looks really bad\n",
      RunIndentedAssemblyTest(true, false, 0, 37, 0, true));


  EXPECT_EQ(
      "Error(8:0): This is an error\n"
      "Error(10:5): Oops, an error!\n"
      "Error(12:6): The record looks bad\n"
      "Error(12:6): Actually, it looks really bad\n",
      RunIndentedAssemblyTest(false, false, 0, 64, 0, true));
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
      "     127:1|<5, 18446744073709551615,    |\n"
      "          | 100, 15, 107056>            |\n"
      "Error(127:1): This is an error\n"
      "     129:6|<5, 18446744073709551615,    |One line assembly.\n"
      "          | 100, 15, 107056>            |\n"
      "Error(129:6): Oops, an error!\n"
      "     131:7|<5, 18446744073709551615,    |Two Line\n"
      "          | 100, 15, 107056>            |example assembly.\n"
      "Error(131:7): The record looks bad\n"
      "Error(131:7): Actually, it looks really bad\n",
      RunIndentedAssemblyTest(true, true, 0, 1017, &Record, true));

  EXPECT_EQ(
      "Error(11:3): This is an error\n"
      "One line assembly.\n"
      "Error(14:0): Oops, an error!\n"
      "Two Line\n"
      "example assembly.\n"
      "Error(16:1): The record looks bad\n"
      "Error(16:1): Actually, it looks really bad\n",
      RunIndentedAssemblyTest(false, true, 0, 91, &Record, true));

  EXPECT_EQ(
      "   47073:6|<5, 18446744073709551615, 100, 15, 107056>\n"
      "Error(47073:6): This is an error\n"
      "   47076:3|<5, 18446744073709551615, 100, 15, 107056>\n"
      "Error(47076:3): Oops, an error!\n"
      "   47078:4|<5, 18446744073709551615, 100, 15, 107056>\n"
      "Error(47078:4): The record looks bad\n"
      "Error(47078:4): Actually, it looks really bad\n",
      RunIndentedAssemblyTest(true, false, 0, 376590, &Record, true));

  EXPECT_EQ(
      "Error(8070:4): This is an error\n"
      "Error(8073:1): Oops, an error!\n"
      "Error(8075:2): The record looks bad\n"
      "Error(8075:2): Actually, it looks really bad\n",
      RunIndentedAssemblyTest(false, false, 0, 64564, &Record, true));

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
      "     127:1|<5, 18446744073709551615,    |\n"
      "          | 100, 15, 107056,            |\n"
      "          | 18446744073709546551,       |\n"
      "          | 101958788>                  |\n"
      "Error(127:1): This is an error\n"
      "     129:6|<5, 18446744073709551615,    |One line assembly.\n"
      "          | 100, 15, 107056,            |\n"
      "          | 18446744073709546551,       |\n"
      "          | 101958788>                  |\n"
      "Error(129:6): Oops, an error!\n"
      "     131:7|<5, 18446744073709551615,    |Two Line\n"
      "          | 100, 15, 107056,            |example assembly.\n"
      "          | 18446744073709546551,       |\n"
      "          | 101958788>                  |\n"
      "Error(131:7): The record looks bad\n"
      "Error(131:7): Actually, it looks really bad\n",
      RunIndentedAssemblyTest(true, true, 0, 1017, &Record, true));

  EXPECT_EQ(
      "Error(11:3): This is an error\n"
      "One line assembly.\n"
      "Error(14:0): Oops, an error!\n"
      "Two Line\n"
      "example assembly.\n"
      "Error(16:1): The record looks bad\n"
      "Error(16:1): Actually, it looks really bad\n",
      RunIndentedAssemblyTest(false, true, 0, 91, &Record, true));

  EXPECT_EQ(
"   47073:6|<5, 18446744073709551615, 100, 15, 107056, 18446744073709546551, \n"
"          | 101958788>\n"
"Error(47073:6): This is an error\n"
"   47076:3|<5, 18446744073709551615, 100, 15, 107056, 18446744073709546551, \n"
"          | 101958788>\n"
"Error(47076:3): Oops, an error!\n"
"   47078:4|<5, 18446744073709551615, 100, 15, 107056, 18446744073709546551, \n"
"          | 101958788>\n"
"Error(47078:4): The record looks bad\n"
"Error(47078:4): Actually, it looks really bad\n",
      RunIndentedAssemblyTest(true, false, 0, 376590, &Record, true));

  EXPECT_EQ(
      "Error(8070:4): This is an error\n"
      "Error(8073:1): Oops, an error!\n"
      "Error(8075:2): The record looks bad\n"
      "Error(8075:2): Actually, it looks really bad\n",
      RunIndentedAssemblyTest(false, false, 0, 64564, &Record, true));
}

// Tests effects of objdump when there isn't a record to write, and we indent.
TEST(NaClObjDumpTest, NoDumpIndentRecords) {

  EXPECT_EQ(
      "          |                             |One line assembly.\n"
      "          |                             |Two Line\n"
      "          |                             |example assembly.\n",
      RunIndentedAssemblyTest(true, true, 1, 11, 0, false));

  EXPECT_EQ(
      "          |                             |One line assembly.\n"
      "          |                             |Two Line\n"
      "          |                             |example assembly.\n",
      RunIndentedAssemblyTest(true, true, 2, 11, 0, false));

  EXPECT_EQ(
      "One line assembly.\n"
      "Two Line\n"
      "example assembly.\n",
      RunIndentedAssemblyTest(false, true, 1, 91, 0, false));

  EXPECT_EQ(
      "One line assembly.\n"
      "Two Line\n"
      "example assembly.\n",
      RunIndentedAssemblyTest(false, true, 2, 91, 0, false));

  EXPECT_EQ(
      "",
      RunIndentedAssemblyTest(true, false, 1, 37, 0, false));

  EXPECT_EQ(
      "",
      RunIndentedAssemblyTest(true, false, 2, 37, 0, false));

  EXPECT_EQ(
      "",
      RunIndentedAssemblyTest(false, false, 1, 64, 0, false));

  EXPECT_EQ(
      "",
      RunIndentedAssemblyTest(false, false, 2, 64, 0, false));
}

// Tests simple cases where there is both a record and corresponding
// assembly code, and the records are indented.
TEST(NaClObjDumpTest, SimpleIndentRecords) {
  NaClBitcodeRecordData Record;
  Record.Code = 5;
  Record.Values.push_back(10);
  Record.Values.push_back(15);

  EXPECT_EQ(
      "       1:3|  <5, 10, 15>                |\n"
      "       4:0|  <5, 10, 15>                |One line assembly.\n"
      "       6:1|  <5, 10, 15>                |Two Line\n"
      "          |                             |example assembly.\n",
      RunIndentedAssemblyTest(true, true, 1, 11, &Record, false));

  EXPECT_EQ(
      "       1:3|    <5, 10, 15>              |\n"
      "       4:0|    <5, 10, 15>              |One line assembly.\n"
      "       6:1|    <5, 10, 15>              |Two Line\n"
      "          |                             |example assembly.\n",
      RunIndentedAssemblyTest(true, true, 2, 11, &Record, false));

  EXPECT_EQ(
      "One line assembly.\n"
      "Two Line\n"
      "example assembly.\n",
      RunIndentedAssemblyTest(false, true, 1, 91, &Record, false));

  EXPECT_EQ(
      "One line assembly.\n"
      "Two Line\n"
      "example assembly.\n",
      RunIndentedAssemblyTest(false, true, 2, 91, &Record, false));

  EXPECT_EQ(
      "       4:5|  <5, 10, 15>\n"
      "       7:2|  <5, 10, 15>\n"
      "       9:3|  <5, 10, 15>\n",
      RunIndentedAssemblyTest(true, false, 1, 37, &Record, false));

  EXPECT_EQ(
      "       4:5|    <5, 10, 15>\n"
      "       7:2|    <5, 10, 15>\n"
      "       9:3|    <5, 10, 15>\n",
      RunIndentedAssemblyTest(true, false, 2, 37, &Record, false));

  EXPECT_EQ(
      "",
      RunIndentedAssemblyTest(false, false, 1, 64, &Record, false));

  EXPECT_EQ(
      "",
      RunIndentedAssemblyTest(false, false, 2, 64, &Record, false));

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
      "     127:1|  <5, 18446744073709551615,  |\n"
      "          |   100, 15, 107056,          |\n"
      "          |   18446744073709546551,     |\n"
      "          |   101958788>                |\n"
      "     129:6|  <5, 18446744073709551615,  |One line assembly.\n"
      "          |   100, 15, 107056,          |\n"
      "          |   18446744073709546551,     |\n"
      "          |   101958788>                |\n"
      "     131:7|  <5, 18446744073709551615,  |Two Line\n"
      "          |   100, 15, 107056,          |example assembly.\n"
      "          |   18446744073709546551,     |\n"
      "          |   101958788>                |\n",
      RunIndentedAssemblyTest(true, true, 1, 1017, &Record, false));

  EXPECT_EQ(
      "     127:1|      <5,                    |\n"
      "          |       18446744073709551615, |\n"
      "          |       100, 15, 107056,      |\n"
      "          |       18446744073709546551, |\n"
      "          |       101958788>            |\n"
      "     129:6|      <5,                    |One line assembly.\n"
      "          |       18446744073709551615, |\n"
      "          |       100, 15, 107056,      |\n"
      "          |       18446744073709546551, |\n"
      "          |       101958788>            |\n"
      "     131:7|      <5,                    |Two Line\n"
      "          |       18446744073709551615, |example assembly.\n"
      "          |       100, 15, 107056,      |\n"
      "          |       18446744073709546551, |\n"
      "          |       101958788>            |\n",
      RunIndentedAssemblyTest(true, true, 3, 1017, &Record, false));

  EXPECT_EQ(
      "One line assembly.\n"
      "Two Line\n"
      "example assembly.\n",
      RunIndentedAssemblyTest(false, true, 1, 91, &Record, false));

  EXPECT_EQ(
      "One line assembly.\n"
      "Two Line\n"
      "example assembly.\n",
      RunIndentedAssemblyTest(false, true, 2, 91, &Record, false));

  EXPECT_EQ(
"   47073:6|  <5, 18446744073709551615, 100, 15, 107056, 18446744073709546551, \n"
"          |   101958788>\n"
"   47076:3|  <5, 18446744073709551615, 100, 15, 107056, 18446744073709546551, \n"
"          |   101958788>\n"
"   47078:4|  <5, 18446744073709551615, 100, 15, 107056, 18446744073709546551, \n"
"          |   101958788>\n",
      RunIndentedAssemblyTest(true, false, 1, 376590, &Record, false));


  EXPECT_EQ(
      "   47073:6|          <5, 18446744073709551615, 100, 15, 107056, \n"
      "          |           18446744073709546551, 101958788>\n"
      "   47076:3|          <5, 18446744073709551615, 100, 15, 107056, \n"
      "          |           18446744073709546551, 101958788>\n"
      "   47078:4|          <5, 18446744073709551615, 100, 15, 107056, \n"
      "          |           18446744073709546551, 101958788>\n",
      RunIndentedAssemblyTest(true, false, 5, 376590, &Record, false));

  EXPECT_EQ(
      "",
      RunIndentedAssemblyTest(false, false, 1, 64564, &Record, false));

  EXPECT_EQ(
      "",
      RunIndentedAssemblyTest(false, false, 2, 64564, &Record, false));
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
"          |                             |begin SomeReasonablylongText(\n"
"          |                             |                    ThisIsBoring \n"
"          |                             |                    VeryBoring \n"
"          |                             |                    longggggggggggggggggg\n"
"          |                             |                    Example,),\n",
BufStream.str());
}

// Tests the insertion of an abbreviation index.
TEST(NaClObjDumpTest, UseOfAbbrevationIndex) {
  NaClBitcodeRecordData Record;
  Record.Code = 5;
  Record.Values.push_back(static_cast<uint64_t>(-1));
  Record.Values.push_back(100);
  Record.Values.push_back(15);
  Record.Values.push_back(107056);
  Record.Values.push_back(static_cast<uint64_t>(-5065));
  Record.Values.push_back(101958788);

  EXPECT_EQ(
      "     127:1|3: <5, 18446744073709551615, |\n"
      "          |    100, 15, 107056,         |\n"
      "          |    18446744073709546551,    |\n"
      "          |    101958788>               |\n"
      "     129:6|3: <5, 18446744073709551615, |One line assembly.\n"
      "          |    100, 15, 107056,         |\n"
      "          |    18446744073709546551,    |\n"
      "          |    101958788>               |\n"
      "     131:7|3: <5, 18446744073709551615, |Two Line\n"
      "          |    100, 15, 107056,         |example assembly.\n"
      "          |    18446744073709546551,    |\n"
      "          |    101958788>               |\n",
      RunIndentedAssemblyWithAbbrevTest(true, true, 0, 1017, &Record,
                                        naclbitc::UNABBREV_RECORD, false));

  EXPECT_EQ(
      "     127:1|  3: <5,                     |\n"
      "          |      18446744073709551615,  |\n"
      "          |      100, 15, 107056,       |\n"
      "          |      18446744073709546551,  |\n"
      "          |      101958788>             |\n"
      "     129:6|  3: <5,                     |One line assembly.\n"
      "          |      18446744073709551615,  |\n"
      "          |      100, 15, 107056,       |\n"
      "          |      18446744073709546551,  |\n"
      "          |      101958788>             |\n"
      "     131:7|  3: <5,                     |Two Line\n"
      "          |      18446744073709551615,  |example assembly.\n"
      "          |      100, 15, 107056,       |\n"
      "          |      18446744073709546551,  |\n"
      "          |      101958788>             |\n",
      RunIndentedAssemblyWithAbbrevTest(true, true, 1, 1017, &Record,
                                        naclbitc::UNABBREV_RECORD, false));

  EXPECT_EQ(
      "     127:1|      3: <5,                 |\n"
      "          |        18446744073709551615,|\n"
      "          |        100, 15, 107056,     |\n"
      "          |        18446744073709546551,|\n"
      "          |        101958788>           |\n"
      "     129:6|      3: <5,                 |One line assembly.\n"
      "          |        18446744073709551615,|\n"
      "          |        100, 15, 107056,     |\n"
      "          |        18446744073709546551,|\n"
      "          |        101958788>           |\n"
      "     131:7|      3: <5,                 |Two Line\n"
      "          |        18446744073709551615,|example assembly.\n"
      "          |        100, 15, 107056,     |\n"
      "          |        18446744073709546551,|\n"
      "          |        101958788>           |\n",
      RunIndentedAssemblyWithAbbrevTest(true, true, 3, 1017, &Record,
                                        naclbitc::UNABBREV_RECORD, false));

  EXPECT_EQ(
      "One line assembly.\n"
      "Two Line\n"
      "example assembly.\n",
      RunIndentedAssemblyWithAbbrevTest(false, true, 1, 91, &Record,
                                        naclbitc::UNABBREV_RECORD, false));

  EXPECT_EQ(
      "One line assembly.\n"
      "Two Line\n"
      "example assembly.\n",
      RunIndentedAssemblyWithAbbrevTest(false, true, 2, 91, &Record,
                                        naclbitc::UNABBREV_RECORD, false));

  EXPECT_EQ(
"   47073:6|  3: <5, 18446744073709551615, 100, 15, 107056, 18446744073709546551,\n"
"          |      101958788>\n"
"   47076:3|  3: <5, 18446744073709551615, 100, 15, 107056, 18446744073709546551,\n"
"          |      101958788>\n"
"   47078:4|  3: <5, 18446744073709551615, 100, 15, 107056, 18446744073709546551,\n"
"          |      101958788>\n",
      RunIndentedAssemblyWithAbbrevTest(true, false, 1, 376590, &Record,
                                        naclbitc::UNABBREV_RECORD, false));


  EXPECT_EQ(
      "   47073:6|          3: <5, 18446744073709551615, 100, 15, 107056, \n"
      "          |              18446744073709546551, 101958788>\n"
      "   47076:3|          3: <5, 18446744073709551615, 100, 15, 107056, \n"
      "          |              18446744073709546551, 101958788>\n"
      "   47078:4|          3: <5, 18446744073709551615, 100, 15, 107056, \n"
      "          |              18446744073709546551, 101958788>\n",
      RunIndentedAssemblyWithAbbrevTest(true, false, 5, 376590, &Record,
                                        naclbitc::UNABBREV_RECORD, false));

  EXPECT_EQ(
      "",
      RunIndentedAssemblyWithAbbrevTest(false, false, 1, 64564, &Record,
                                        naclbitc::UNABBREV_RECORD, false));

  EXPECT_EQ(
      "",
      RunIndentedAssemblyWithAbbrevTest(false, false, 2, 64564, &Record,
                                        naclbitc::UNABBREV_RECORD, false));
}

}
