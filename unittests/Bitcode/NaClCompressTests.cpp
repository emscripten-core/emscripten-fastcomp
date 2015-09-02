//===- llvm/unittest/Bitcode/NaClCompressTests.cpp ------------------------===//
//     Tests pnacl compression of bitcode files.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "NaClMungeTest.h"

#include "llvm/Bitcode/NaCl/NaClLLVMBitCodes.h"

using namespace llvm;

namespace naclmungetest {

// Note: Tests fix for bug in
// https://code.google.com/p/nativeclient/issues/detail?id=4104
TEST(NaClCompressTests, FixedModuleAbbrevIdBug) {
  const uint64_t BitcodeRecords[] = {
    1, naclbitc::BLK_CODE_ENTER, naclbitc::MODULE_BLOCK_ID, 4, Terminator,
    // Note: We need at least one module abbreviation to activate bug.
    2, naclbitc::BLK_CODE_DEFINE_ABBREV, 2,
       0, NaClBitCodeAbbrevOp::Array,
       0, NaClBitCodeAbbrevOp::VBR, 6,
       Terminator,
    // Note: We need at least one record in the module that can introduce
    // a new abbreviation and cause the bug.
    4, naclbitc::MODULE_CODE_VERSION, 1, Terminator,
    1, naclbitc::BLK_CODE_ENTER, 17, 4, Terminator,
    3, naclbitc::TYPE_CODE_NUMENTRY, 0, Terminator,
    0, naclbitc::BLK_CODE_EXIT, Terminator,
    0, naclbitc::BLK_CODE_EXIT, Terminator,
  };

  // Show textual version of sample input.
  NaClObjDumpMunger DumpMunger(ARRAY_TERM(BitcodeRecords));
  EXPECT_TRUE(DumpMunger.runTest());
  EXPECT_EQ(
      "       0:0|<65532, 80, 69, 88, 69, 1, 0,|Magic Number: 'PEXE' (80, 69, 8"
      "8, 69)\n"
      "          | 8, 0, 17, 0, 4, 0, 2, 0, 0, |PNaCl Version: 2\n"
      "          | 0>                          |\n"
      "      16:0|1: <65535, 8, 4>             |module {  // BlockID = 8\n"
      "      24:0|  2: <65533, 2, 0, 3, 0, 2, 6|  %a0 = abbrev <array(vbr(6))>;"
      "\n"
      "          |      >                      |\n"
      "      26:6|  4: <1, 1>                  |  version 1; <%a0>\n"
      "      29:4|  1: <65535, 17, 4>          |  types {  // BlockID = 17\n"
      "      36:0|    3: <1, 0>                |    count 0;\n"
      "      38:6|  0: <65534>                 |  }\n"
      "      40:0|0: <65534>                   |}\n",
      DumpMunger.getTestResults());

  // Show that we can compress as well.
  NaClCompressMunger CompressMunger(BitcodeRecords,
                                    array_lengthof(BitcodeRecords), Terminator);
  EXPECT_TRUE(CompressMunger.runTest());
}

} // end of namespace naclmungetest
