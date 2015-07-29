//===- llvm/unittest/Bitcode/NaClMungedIoTest.cpp -------------------------===//
//     Tests munging NaCl bitcode records.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// For class NaClMungedBitcode, tests reading initial sequence of records and
// writing out munged set of bitcode records.

#include "NaClMungeTest.h"

namespace naclmungetest {

using namespace llvm;

typedef SmallVector<char, 1024> TextBuffer;

// Writes out a sequence of munged bitcode records, and writes them into
// the text buffer. Returns a corresponding memory buffer containing
// the munged bitcode records.
std::unique_ptr<MemoryBuffer> writeMungedBitcode(
    NaClMungedBitcode &Bitcode, TextBuffer &Buffer,
    NaClMungedBitcode::WriteFlags &Flags) {
  Bitcode.write(Buffer, /* AddHeader = */ true, Flags);
  StringRef Input(Buffer.data(), Buffer.size());
  return MemoryBuffer::getMemBuffer(Input, "Test", false);
}

std::unique_ptr<MemoryBuffer> writeMungedBitcode(
    NaClMungedBitcode &Bitcode, TextBuffer &Buffer) {
  NaClMungedBitcode::WriteFlags Flags;
  return writeMungedBitcode(Bitcode, Buffer, Flags);
}

// Write out the bitcode, parse it back, and return the resulting
// munged bitcode.
std::string parseWrittenMungedBitcode(NaClMungedBitcode &OutBitcode) {
  TextBuffer Buffer;
  NaClMungedBitcode InBitcode(writeMungedBitcode(OutBitcode, Buffer));
  return stringify(InBitcode);
}

// Sample toy bitcode records.
const uint64_t Records[] = {
  1, naclbitc::BLK_CODE_ENTER, 8, 2, Terminator,
  3, naclbitc::MODULE_CODE_VERSION, 1, Terminator,
  1, naclbitc::BLK_CODE_ENTER, 0, 2, Terminator,
  3, naclbitc::BLOCKINFO_CODE_SETBID, 12, Terminator,
  2, naclbitc::BLK_CODE_DEFINE_ABBREV, 1, 1, 10, Terminator,
  0, naclbitc::BLK_CODE_EXIT, Terminator,
  1, naclbitc::BLK_CODE_ENTER, 17, 3, Terminator,
  2, naclbitc::BLK_CODE_DEFINE_ABBREV, 4, 1, 21, 0, 1, 1, 0, 3,
    0, 1, 2, Terminator,
  3, naclbitc::TYPE_CODE_NUMENTRY, 2, Terminator,
  3, naclbitc::TYPE_CODE_VOID, Terminator,
  4, naclbitc::TYPE_CODE_FUNCTION, 0, 0, Terminator,
  0, naclbitc::BLK_CODE_EXIT, Terminator,
  3, naclbitc::MODULE_CODE_FUNCTION, 1, 0, 0, 3, Terminator,
  1, naclbitc::BLK_CODE_ENTER, 12, 3, Terminator,
  3, naclbitc::FUNC_CODE_DECLAREBLOCKS, 1, Terminator,
  4, naclbitc::FUNC_CODE_INST_RET, Terminator,
  0, naclbitc::BLK_CODE_EXIT, Terminator,
  0, naclbitc::BLK_CODE_EXIT, Terminator,
};

// Show a more readable form of what the program is.
TEST(NaClMungedIoTest, TestDumpingBitcode) {
  NaClObjDumpMunger DumpMunger(ARRAY_TERM(Records));
  EXPECT_TRUE(DumpMunger.runTest());
  EXPECT_EQ(
      "       0:0|<65532, 80, 69, 88, 69, 1, 0,|Magic Number: 'PEXE' (80, 69, "
      "88, 69)\n"
      "          | 8, 0, 17, 0, 4, 0, 2, 0, 0, |PNaCl Version: 2\n"
      "          | 0>                          |\n"
      "      16:0|1: <65535, 8, 2>             |module {  // BlockID = 8\n"
      "      24:0|  3: <1, 1>                  |  version 1;\n"
      "      26:4|  1: <65535, 0, 2>           |  abbreviations {  // BlockID "
      "= 0\n"
      "      36:0|    3: <1, 12>               |    function:\n"
      "      38:4|    2: <65533, 1, 1, 10>     |      @a0 = abbrev <10>;\n"
      "      40:4|  0: <65534>                 |  }\n"
      "      44:0|  1: <65535, 17, 3>          |  types {  // BlockID = 17\n"
      "      52:0|    2: <65533, 4, 1, 21, 0,  |    %a0 = abbrev <21, fixed(1),"
      " \n"
      "          |        1, 1, 0, 3, 0, 1, 2> |                  array(fixed("
      "2))>;\n"
      "      56:7|    3: <1, 2>                |    count 2;\n"
      "      59:4|    3: <2>                   |    @t0 = void;\n"
      "      61:3|    4: <21, 0, 0>            |    @t1 = void (); <%a0>\n"
      "      62:7|  0: <65534>                 |  }\n"
      "      64:0|  3: <8, 1, 0, 0, 3>         |  define internal void @f0();\n"
      "      68:6|  1: <65535, 12, 3>          |  function void @f0() {  \n"
      "          |                             |                   // BlockID "
      "= 12\n"
      "      76:0|    3: <1, 1>                |    blocks 1;\n"
      "          |                             |  %b0:\n"
      "      78:5|    4: <10>                  |    ret void; <@a0>\n"
      "      79:0|  0: <65534>                 |  }\n"
      "      80:0|0: <65534>                   |}\n",
      DumpMunger.getTestResults());
}

// Test that we can write out bitcode, and then read it back in.
TEST(NaClMungedIoTest, TestWriteThenRead) {
  // Create munged bitcode for the given records.
  NaClMungedBitcode Bitcode(ARRAY_TERM(Records));

  // The expected output when stringifying this input.
  const std::string ExpectedRecords(
      "       1: [65535, 8, 2]\n"
      "         3: [1, 1]\n"
      "         1: [65535, 0, 2]\n"
      "           3: [1, 12]\n"
      "           2: [65533, 1, 1, 10]\n"
      "         0: [65534]\n"
      "         1: [65535, 17, 3]\n"
      "           2: [65533, 4, 1, 21, 0, 1, 1, 0, 3, 0, 1, 2]\n"
      "           3: [1, 2]\n"
      "           3: [2]\n"
      "           4: [21, 0, 0]\n"
      "         0: [65534]\n"
      "         3: [8, 1, 0, 0, 3]\n"
      "         1: [65535, 12, 3]\n"
      "           3: [1, 1]\n"
      "           4: [10]\n"
      "         0: [65534]\n"
      "       0: [65534]\n");
  EXPECT_EQ(ExpectedRecords, stringify(Bitcode));

  // Write and read the bitcode back into a sequence of records.
  EXPECT_EQ(ExpectedRecords, parseWrittenMungedBitcode(Bitcode));
}


// Test that writing truncated bitcode is difficult, due to word
// alignment requirements for bitcode files. Note: Bitcode files must
// be divisible by 4.
TEST(NaClMungedIoTest, TestTruncatedNonalignedBitcode) {
  // Created an example of a truncated bitcode file.
  NaClMungedBitcode Bitcode(ARRAY_TERM(Records));
  for (size_t i = 2, e = Bitcode.getBaseRecords().size(); i < e; ++i)
    Bitcode.remove(i);

  // The expected output when stringifying this input.
  EXPECT_EQ(
      "       1: [65535, 8, 2]\n"
      "         3: [1, 1]\n",
      stringify(Bitcode));

  // Show that we can't write the bitcode correctly.
  TextBuffer WriteBuffer;
  std::string LogBuffer;
  raw_string_ostream StrBuf(LogBuffer);
  NaClMungedBitcode::WriteFlags Flags;
  Flags.setErrStream(StrBuf);
  writeMungedBitcode(Bitcode, WriteBuffer, Flags);
  EXPECT_EQ(
      "Error (Block 8): Missing close block.\n",
      StrBuf.str());
}

} // end of namespace naclmungetest
