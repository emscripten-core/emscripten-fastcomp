//===- llvm/unittest/Bitcode/NaClMungedBitcodeTest.cpp -------------------===//
//     Tests munging NaCl bitcode records.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// Tests munging NaCl bitcode records.

#include "NaClMungeTest.h"

#include <limits>

using namespace llvm;

namespace naclmungetest {

TEST(NaClMungedBitcodeTest, TestInsertBefore) {
  const uint64_t Records[] = {
    1, 2, 3, Terminator,
    4, 5, Terminator,
    6, 7, 8 , 9, Terminator,
    10, 11, Terminator
  };
  NaClMungedBitcode MungedRecords(ARRAY_TERM(Records));
  EXPECT_EQ(
      "       1: [2, 3]\n"
      "       4: [5]\n"
      "       6: [7, 8, 9]\n"
      "      10: [11]\n",
      stringify(MungedRecords));

  // Add a record before the second record.
  const uint64_t BeforeSecond[] = {
    1, NaClMungedBitcode::AddBefore, 12, 13, 14, Terminator
  };
  MungedRecords.munge(ARRAY_TERM(BeforeSecond));
  EXPECT_EQ(
      "       1: [2, 3]\n"
      "      12: [13, 14]\n"
      "       4: [5]\n"
      "       6: [7, 8, 9]\n"
      "      10: [11]\n",
      stringify(MungedRecords));

  // Add two more records before the second record.
  const uint64_t BeforeSecondMore[] = {
    1, NaClMungedBitcode::AddBefore, 15, 16, 17, Terminator,
    1, NaClMungedBitcode::AddBefore, 18, 19, Terminator
  };
  MungedRecords.munge(ARRAY_TERM(BeforeSecondMore));
  EXPECT_EQ(
      "       1: [2, 3]\n"
      "      12: [13, 14]\n"
      "      15: [16, 17]\n"
      "      18: [19]\n"
      "       4: [5]\n"
      "       6: [7, 8, 9]\n"
      "      10: [11]\n",
      stringify(MungedRecords));

  // Add two records before the last record.
  const uint64_t BeforeLast[] = {
    3, NaClMungedBitcode::AddBefore, 21, 22, 23, Terminator,
    3, NaClMungedBitcode::AddBefore, 24, 25, 26, 27, Terminator
  };
  MungedRecords.munge(ARRAY_TERM(BeforeLast));
  EXPECT_EQ(
      "       1: [2, 3]\n"
      "      12: [13, 14]\n"
      "      15: [16, 17]\n"
      "      18: [19]\n"
      "       4: [5]\n"
      "       6: [7, 8, 9]\n"
      "      21: [22, 23]\n"
      "      24: [25, 26, 27]\n"
      "      10: [11]\n",
      stringify(MungedRecords));
}

TEST(NaClMungedBitcodeTest, TestInsertAfter) {
  const uint64_t Records[] = {
    1, 2, 3, Terminator,
    4, 5, Terminator,
    6, 7, 8 , 9, Terminator,
    10, 11, Terminator
  };
  NaClMungedBitcode MungedRecords(ARRAY_TERM(Records));
  EXPECT_EQ(
      "       1: [2, 3]\n"
      "       4: [5]\n"
      "       6: [7, 8, 9]\n"
      "      10: [11]\n",
      stringify(MungedRecords));

  // Add a record after the second record.
  const uint64_t AfterSecond[] = {
    1, NaClMungedBitcode::AddAfter, 12, 13, 14, Terminator
  };
  MungedRecords.munge(ARRAY_TERM(AfterSecond));
  EXPECT_EQ(
      "       1: [2, 3]\n"
      "       4: [5]\n"
      "      12: [13, 14]\n"
      "       6: [7, 8, 9]\n"
      "      10: [11]\n",
      stringify(MungedRecords));

  // Add two more records after the second record.
  const uint64_t AfterSecondMore[] = {
    1, NaClMungedBitcode::AddAfter, 15, 16, 17, Terminator,
    1, NaClMungedBitcode::AddAfter, 18, 19, Terminator
  };
  MungedRecords.munge(ARRAY_TERM(AfterSecondMore));
  EXPECT_EQ(
      "       1: [2, 3]\n"
      "       4: [5]\n"
      "      12: [13, 14]\n"
      "      15: [16, 17]\n"
      "      18: [19]\n"
      "       6: [7, 8, 9]\n"
      "      10: [11]\n",
      stringify(MungedRecords));

  // Add two records after the last record.
  const uint64_t AfterLast[] = {
    3, NaClMungedBitcode::AddAfter, 21, 22, 23, Terminator,
    3, NaClMungedBitcode::AddAfter, 24, 25, 26, 27, Terminator
  };
  MungedRecords.munge(ARRAY_TERM(AfterLast));
  EXPECT_EQ(
      "       1: [2, 3]\n"
      "       4: [5]\n"
      "      12: [13, 14]\n"
      "      15: [16, 17]\n"
      "      18: [19]\n"
      "       6: [7, 8, 9]\n"
      "      10: [11]\n"
      "      21: [22, 23]\n"
      "      24: [25, 26, 27]\n",
      stringify(MungedRecords));
}

TEST(NaClMungedBitcodeTest, TestRemove) {
  const uint64_t Records[] = {
    1, 2, 3, Terminator,
    4, 5, Terminator,
    6, 7, 8 , 9, Terminator,
    10, 11, Terminator
  };
  NaClMungedBitcode MungedRecords(ARRAY_TERM(Records));
  EXPECT_EQ(
      "       1: [2, 3]\n"
      "       4: [5]\n"
      "       6: [7, 8, 9]\n"
      "      10: [11]\n",
      stringify(MungedRecords));

  // Remove the second record.
  const uint64_t RemoveSecond[] = {
    1, NaClMungedBitcode::Remove
  };
  MungedRecords.munge(ARRAY_TERM(RemoveSecond));
  EXPECT_EQ(
      "       1: [2, 3]\n"
      "       6: [7, 8, 9]\n"
      "      10: [11]\n",
      stringify(MungedRecords));

  // Remove first and last records.
  const uint64_t RemoveEnds[] = {
    0, NaClMungedBitcode::Remove,
    3, NaClMungedBitcode::Remove
  };
  MungedRecords.munge(ARRAY_TERM(RemoveEnds));
  EXPECT_EQ(
      "       6: [7, 8, 9]\n",
      stringify(MungedRecords));

  // Remove remaining record.
  const uint64_t RemoveOther[] = {
    2, NaClMungedBitcode::Remove
  };
  MungedRecords.munge(ARRAY_TERM(RemoveOther));
  EXPECT_EQ(
      "",
      stringify(MungedRecords));
}

TEST(NaClMungedBitcodeTest, TestReplace) {
  const uint64_t Records[] = {
    1, 2, 3, Terminator,
    4, 5, Terminator,
    6, 7, 8 , 9, Terminator,
    10, 11, Terminator
  };
  NaClMungedBitcode MungedRecords(ARRAY_TERM(Records));
  EXPECT_EQ(
      "       1: [2, 3]\n"
      "       4: [5]\n"
      "       6: [7, 8, 9]\n"
      "      10: [11]\n",
      stringify(MungedRecords));

  // Replace the second record.
  const uint64_t ReplaceSecond[] = {
    1, NaClMungedBitcode::Replace, 12, 13, 14, Terminator
  };
  MungedRecords.munge(ARRAY_TERM(ReplaceSecond));
  EXPECT_EQ(
      "       1: [2, 3]\n"
      "      12: [13, 14]\n"
      "       6: [7, 8, 9]\n"
      "      10: [11]\n",
      stringify(MungedRecords));

  // Replace the first and last record.
  const uint64_t ReplaceEnds[] = {
    0, NaClMungedBitcode::Replace, 15, 16, 17, 18, Terminator,
    3, NaClMungedBitcode::Replace, 19, 20, Terminator
  };
  MungedRecords.munge(ARRAY_TERM(ReplaceEnds));
  EXPECT_EQ(
      "      15: [16, 17, 18]\n"
      "      12: [13, 14]\n"
      "       6: [7, 8, 9]\n"
      "      19: [20]\n",
      stringify(MungedRecords));

  // Replace the first three records, which includes two already replaced
  // records.
  const uint64_t ReplaceFirst3[] = {
    0, NaClMungedBitcode::Replace, 21, 22, 23, Terminator,
    1, NaClMungedBitcode::Replace, 24, 25, Terminator,
    2, NaClMungedBitcode::Replace, 26, 27, 28, 29, Terminator
  };
  MungedRecords.munge(ARRAY_TERM(ReplaceFirst3));
  EXPECT_EQ(
      "      21: [22, 23]\n"
      "      24: [25]\n"
      "      26: [27, 28, 29]\n"
      "      19: [20]\n",
      stringify(MungedRecords));

  // Show that we can remove replaced records.
  const uint64_t RemoveReplaced[] = {
    1, NaClMungedBitcode::Remove,
    3, NaClMungedBitcode::Remove
  };
  MungedRecords.munge(ARRAY_TERM(RemoveReplaced));
  EXPECT_EQ(
      "      21: [22, 23]\n"
      "      26: [27, 28, 29]\n",
      stringify(MungedRecords));
}

TEST(NaClMungedBitcodeTest, TestBlockStructure) {
  const uint64_t Records[] = {
    1, 2, 3, 4, Terminator,
    5, naclbitc::BLK_CODE_ENTER, 6, Terminator,
    7, 8, Terminator,
    9, naclbitc::BLK_CODE_ENTER, 10, Terminator,
    11, 12, 13, Terminator,
    14, naclbitc::BLK_CODE_EXIT, Terminator,
    15, naclbitc::BLK_CODE_ENTER, 16, Terminator,
    17, naclbitc::BLK_CODE_EXIT, 18, Terminator,
    19, 20, 21, Terminator,
    22, naclbitc::BLK_CODE_EXIT, Terminator
  };
  NaClMungedBitcode MungedRecords(ARRAY_TERM(Records));
  EXPECT_EQ(
      "       1: [2, 3, 4]\n"
      "       5: [65535, 6]\n"
      "         7: [8]\n"
      "         9: [65535, 10]\n"
      "          11: [12, 13]\n"
      "        14: [65534]\n"
      "        15: [65535, 16]\n"
      "        17: [65534, 18]\n"
      "        19: [20, 21]\n"
      "      22: [65534]\n",
      stringify(MungedRecords));

  // Show what happens if you have unbalanced blocks.
  const uint64_t ExitEdits[] = {
    4, NaClMungedBitcode::AddAfter, 0, naclbitc::BLK_CODE_EXIT, Terminator,
    4, NaClMungedBitcode::AddAfter, 0, naclbitc::BLK_CODE_EXIT, Terminator,
    2, NaClMungedBitcode::Replace, 0, naclbitc::BLK_CODE_EXIT, Terminator
  };
  MungedRecords.munge(ARRAY_TERM(ExitEdits));
  EXPECT_EQ(
      "       1: [2, 3, 4]\n"
      "       5: [65535, 6]\n"
      "       0: [65534]\n"
      "       9: [65535, 10]\n"
      "        11: [12, 13]\n"
      "       0: [65534]\n"
      "       0: [65534]\n"
      "      14: [65534]\n"
      "      15: [65535, 16]\n"
      "      17: [65534, 18]\n"
      "      19: [20, 21]\n"
      "      22: [65534]\n",
      stringify(MungedRecords));
}

// Tests that replace/remove superceed other replace/removes at same
// record index.
TEST(NaClMungedBitcodeTest, TestReplaceRemoveEffects) {
  const uint64_t Records[] = {
    1, 2, 3, Terminator,
    4, 5, Terminator,
    6, 7, 8 , 9, Terminator,
    10, 11, Terminator
  };
  NaClMungedBitcode MungedRecords(ARRAY_TERM(Records));
  EXPECT_EQ(
      "       1: [2, 3]\n"
      "       4: [5]\n"
      "       6: [7, 8, 9]\n"
      "      10: [11]\n",
      stringify(MungedRecords));

  // Remove the second record.
  const uint64_t RemoveSecond[] = {
    1, NaClMungedBitcode::Remove
  };
  MungedRecords.munge(ARRAY_TERM(RemoveSecond));
  EXPECT_EQ(
      "       1: [2, 3]\n"
      "       6: [7, 8, 9]\n"
      "      10: [11]\n",
      stringify(MungedRecords));

  // Try it again. Should have no effect.
  MungedRecords.munge(ARRAY_TERM(RemoveSecond));
  EXPECT_EQ(
      "       1: [2, 3]\n"
      "       6: [7, 8, 9]\n"
      "      10: [11]\n",
      stringify(MungedRecords));

  // Override removed record with a replacement.
  const uint64_t ReplaceSecond[] = {
    1, NaClMungedBitcode::Replace, 12, 12, 14, 15, Terminator
  };
  MungedRecords.munge(ARRAY_TERM(ReplaceSecond));
  EXPECT_EQ(
      "       1: [2, 3]\n"
      "      12: [12, 14, 15]\n"
      "       6: [7, 8, 9]\n"
      "      10: [11]\n",
      stringify(MungedRecords));

  // Override replacement with a different replacement.
  const uint64_t ReplaceSecondAgain[] = {
    1, NaClMungedBitcode::Replace, 16, 17, 18, Terminator
  };
  MungedRecords.munge(ARRAY_TERM(ReplaceSecondAgain));
  EXPECT_EQ(
      "       1: [2, 3]\n"
      "      16: [17, 18]\n"
      "       6: [7, 8, 9]\n"
      "      10: [11]\n",
      stringify(MungedRecords));

  // Override replacement with a remove.
  MungedRecords.munge(ARRAY_TERM(RemoveSecond));
  EXPECT_EQ(
      "       1: [2, 3]\n"
      "       6: [7, 8, 9]\n"
      "      10: [11]\n",
      stringify(MungedRecords));
}

// Show how before/after interact between neighboring indices
TEST(NaClMungedBitcodeTest, TestBeforeAfterInteraction) {
  const uint64_t Records[] = {
    1, 2, 3, Terminator,
    4, 5, Terminator,
    6, 7, 8 , 9, Terminator,
    10, 11, Terminator
  };
  NaClMungedBitcode MungedRecords(ARRAY_TERM(Records));
  EXPECT_EQ(
      "       1: [2, 3]\n"
      "       4: [5]\n"
      "       6: [7, 8, 9]\n"
      "      10: [11]\n",
      stringify(MungedRecords));

  // Add record before the third record.
  const uint64_t AddBeforeThird[] = {
    2, NaClMungedBitcode::AddBefore, 12, 13, 14, Terminator
  };
  MungedRecords.munge(ARRAY_TERM(AddBeforeThird));
  EXPECT_EQ(
      "       1: [2, 3]\n"
      "       4: [5]\n"
      "      12: [13, 14]\n"
      "       6: [7, 8, 9]\n"
      "      10: [11]\n",
      stringify(MungedRecords));

  // Add record after the second record.
  const uint64_t AddAfterSecond[] = {
    1, NaClMungedBitcode::AddAfter, 15, 16, 17, 18, Terminator
  };
  MungedRecords.munge(ARRAY_TERM(AddAfterSecond));
  EXPECT_EQ(
      "       1: [2, 3]\n"
      "       4: [5]\n"
      "      15: [16, 17, 18]\n"
      "      12: [13, 14]\n"
      "       6: [7, 8, 9]\n"
      "      10: [11]\n",
      stringify(MungedRecords));

  // Add more records before the third record.
  const uint64_t AddBeforeThirdMore[] = {
    2, NaClMungedBitcode::AddBefore, 19, 20, Terminator,
    2, NaClMungedBitcode::AddBefore, 21, 22, Terminator
  };
  MungedRecords.munge(ARRAY_TERM(AddBeforeThirdMore));
  EXPECT_EQ(
      "       1: [2, 3]\n"
      "       4: [5]\n"
      "      15: [16, 17, 18]\n"
      "      12: [13, 14]\n"
      "      19: [20]\n"
      "      21: [22]\n"
      "       6: [7, 8, 9]\n"
      "      10: [11]\n",
      stringify(MungedRecords));

  // Add more records after the second record.
  const uint64_t AddAfterSecondMore[] = {
    1, NaClMungedBitcode::AddAfter, 23, 24, 25, Terminator,
    1, NaClMungedBitcode::AddAfter, 26, 27, 28, 29, Terminator
  };
  MungedRecords.munge(ARRAY_TERM(AddAfterSecondMore));
  EXPECT_EQ(
      "       1: [2, 3]\n"
      "       4: [5]\n"
      "      15: [16, 17, 18]\n"
      "      23: [24, 25]\n"
      "      26: [27, 28, 29]\n"
      "      12: [13, 14]\n"
      "      19: [20]\n"
      "      21: [22]\n"
      "       6: [7, 8, 9]\n"
      "      10: [11]\n",
      stringify(MungedRecords));
}

// Do a sample combination of all possible edits.
TEST(NaClMungedBitcodeTest, CombinationEdits) {
  const uint64_t Records[] = {
    1, 2, 3, Terminator,
    4, 5, Terminator,
    6, 7, 8 , 9, Terminator,
    10, 11, Terminator
  };

  NaClMungedBitcode MungedRecords(ARRAY_TERM(Records));
  EXPECT_EQ(
      "       1: [2, 3]\n"
      "       4: [5]\n"
      "       6: [7, 8, 9]\n"
      "      10: [11]\n",
      stringify(MungedRecords));

  // Remove First
  const uint64_t RemoveFirst[] = {
    0, NaClMungedBitcode::Remove
  };
  MungedRecords.munge(ARRAY_TERM(RemoveFirst));
  EXPECT_EQ(
      "       4: [5]\n"
      "       6: [7, 8, 9]\n"
      "      10: [11]\n",
      stringify(MungedRecords));

  // Add records after the first (base) record, which corresponds to
  // before the first record in the munged result.
  const uint64_t AddAfterFirst[] = {
    0, NaClMungedBitcode::AddAfter, 12, 13, 14, Terminator,
    0, NaClMungedBitcode::AddAfter, 15, 16, Terminator
  };
  MungedRecords.munge(ARRAY_TERM(AddAfterFirst));
  EXPECT_EQ(
      "      12: [13, 14]\n"
      "      15: [16]\n"
      "       4: [5]\n"
      "       6: [7, 8, 9]\n"
      "      10: [11]\n",
      stringify(MungedRecords));

  // Add records before the second (base) record, which corresponds to
  // before the third record in the munged result.
  const uint64_t AddBeforeSecond[] = {
    1, NaClMungedBitcode::AddBefore, 17, 18, 19, 20, Terminator,
    1, NaClMungedBitcode::AddBefore, 21, 22, 23, Terminator
  };
  MungedRecords.munge(ARRAY_TERM(AddBeforeSecond));
  EXPECT_EQ(
      "      12: [13, 14]\n"
      "      15: [16]\n"
      "      17: [18, 19, 20]\n"
      "      21: [22, 23]\n"
      "       4: [5]\n"
      "       6: [7, 8, 9]\n"
      "      10: [11]\n",
      stringify(MungedRecords));

  // Put the first (base) record back, which will also be the first
  // record in the munged result.
  const uint64_t ReplaceFirst[] = {
    0, NaClMungedBitcode::Replace, 1, 2, 3, Terminator
  };
  MungedRecords.munge(ARRAY_TERM(ReplaceFirst));
  EXPECT_EQ(
      "       1: [2, 3]\n"
      "      12: [13, 14]\n"
      "      15: [16]\n"
      "      17: [18, 19, 20]\n"
      "      21: [22, 23]\n"
      "       4: [5]\n"
      "       6: [7, 8, 9]\n"
      "      10: [11]\n",
      stringify(MungedRecords));

  // Add before the first (base) record, which will also be before all
  // other records in the munged result.
  const uint64_t AddBeforeFirst[] = {
    0, NaClMungedBitcode::AddBefore, 24, 25, 26, 27, Terminator,
    0, NaClMungedBitcode::AddBefore, 28, 29, Terminator,
    0, NaClMungedBitcode::AddBefore, 30, 31, 32, Terminator
  };
  MungedRecords.munge(ARRAY_TERM(AddBeforeFirst));
  EXPECT_EQ(
      "      24: [25, 26, 27]\n"
      "      28: [29]\n"
      "      30: [31, 32]\n"
      "       1: [2, 3]\n"
      "      12: [13, 14]\n"
      "      15: [16]\n"
      "      17: [18, 19, 20]\n"
      "      21: [22, 23]\n"
      "       4: [5]\n"
      "       6: [7, 8, 9]\n"
      "      10: [11]\n",
      stringify(MungedRecords));
}

} // end of namespace naclmungetest
