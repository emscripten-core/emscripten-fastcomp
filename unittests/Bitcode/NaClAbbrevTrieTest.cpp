//===- llvm/unittest/Bitcode/NaClAbbrevTest.cpp - Tests for NaCl Abbrevs --===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// Tests if we properly sort abbreviations when building an
// abbreviation trie.

#include "llvm/Bitcode/NaCl/AbbrevTrieNode.h"
#include "llvm/Bitcode/NaCl/NaClBitCodes.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeValueDist.h"
#include "gtest/gtest.h"

#include <iostream>
#include <sstream>

using namespace llvm;

namespace {

static const unsigned MaxValueIndex = NaClValueIndexCutoff + 1;

typedef SmallVector<NaClBitCodeAbbrev*, 32> AbbrevVector;

static void Clear(AbbrevVector &Vector) {
  for (AbbrevVector::iterator Iter = Vector.begin(), IterEnd = Vector.end();
       Iter != IterEnd; ++Iter) {
    (*Iter)->dropRef();
  }
}

static void Clear(AbbrevLookupSizeMap &LookupMap) {
  for (AbbrevLookupSizeMap::iterator
           Iter = LookupMap.begin(), IterEnd = LookupMap.end();
       Iter != IterEnd; ++Iter) {
    delete Iter->second;
  }
}

static std::string DescribeAbbreviations(AbbrevVector &Abbrevs) {
  std::string Message;
  raw_string_ostream ostrm(Message);
  for (AbbrevVector::const_iterator
           Iter = Abbrevs.begin(), IterEnd = Abbrevs.end();
       Iter != IterEnd; ++Iter) {
    (*Iter)->Print(ostrm);
  }
  return ostrm.str();
}

static std::string DescribeAbbrevTrieNode(const AbbrevTrieNode *Node,
                                          bool LocalOnly) {
  std::string Message;
  raw_string_ostream ostrm(Message);
  if (Node)
    Node->Print(ostrm, "", LocalOnly);
  else
    ostrm << "NULL";
  return ostrm.str();
}

static std::string DescribeAbbrevTrie(const AbbrevTrieNode *Node) {
  return DescribeAbbrevTrieNode(Node, false);
}

static std::string DescribeAbbrevTrieNode(const AbbrevTrieNode *Node) {
  return DescribeAbbrevTrieNode(Node, true);
}

static std::string DescribeRecord(const NaClBitcodeRecordData &Record) {
  std::string Message;
  raw_string_ostream ostrm(Message);
  Record.Print(ostrm);
  return ostrm.str();
}

TEST(NaClAbbrevTrieTest, Simple) {
  // Test example of multiple abbreviations of length 2.
  AbbrevVector Abbrevs;
  // [1, VBR(6)]
  NaClBitCodeAbbrev *Abbrev = new NaClBitCodeAbbrev();
  Abbrev->Add(NaClBitCodeAbbrevOp(1));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));
  Abbrevs.push_back(Abbrev);
  // [4, VBR(8)]
  Abbrev = new NaClBitCodeAbbrev();
  Abbrev->Add(NaClBitCodeAbbrevOp(4));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 8));
  Abbrevs.push_back(Abbrev);
  // [4, 0]
  Abbrev = new NaClBitCodeAbbrev();
  Abbrev->Add(NaClBitCodeAbbrevOp(4));
  Abbrev->Add(NaClBitCodeAbbrevOp(0));
  Abbrevs.push_back(Abbrev);
  // [1, 2]
  Abbrev = new NaClBitCodeAbbrev();
  Abbrev->Add(NaClBitCodeAbbrevOp(1));
  Abbrev->Add(NaClBitCodeAbbrevOp(2));
  Abbrevs.push_back(Abbrev);
  // [1, 0]
  Abbrev = new NaClBitCodeAbbrev();
  Abbrev->Add(NaClBitCodeAbbrevOp(1));
  Abbrev->Add(NaClBitCodeAbbrevOp(0));
  Abbrevs.push_back(Abbrev);
  // [VBR(6), VBR(6)]
  Abbrev = new NaClBitCodeAbbrev();
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));
  Abbrevs.push_back(Abbrev);
  // [VBR(6), 0]
  Abbrev = new NaClBitCodeAbbrev();
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));
  Abbrev->Add(NaClBitCodeAbbrevOp(0));
  Abbrevs.push_back(Abbrev);

  // Verify we built the expected abbreviations.
  EXPECT_EQ(std::string(
      "[1, VBR(6)]\n"
      "[4, VBR(8)]\n"
      "[4, 0]\n"
      "[1, 2]\n"
      "[1, 0]\n"
      "[VBR(6), VBR(6)]\n"
      "[VBR(6), 0]\n"),
            DescribeAbbreviations(Abbrevs));

  // Build lookup map, and check that we build the expected trie.
  AbbrevLookupSizeMap LookupMap;
  NaClBuildAbbrevLookupMap(LookupMap, Abbrevs);
  EXPECT_EQ((size_t)1, LookupMap.size())
      << "There should only be one entry in the Lookup map "
      << "for abbreviations of length 2";
  for (AbbrevLookupSizeMap::iterator
           Iter = LookupMap.begin(), IterEnd = LookupMap.end();
       Iter != IterEnd; ++Iter) {
    EXPECT_EQ(Iter->first, (size_t)2)
        << "Expecting abbreviations to be of length 2";
    EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [VBR(6), VBR(6)] (abbrev #5)\n"
        "Successor Map:\n"
        "  Record.Code = 1\n"
        "    Abbreviations:\n"
        "      [1, VBR(6)] (abbrev #0)\n"
        "      [VBR(6), VBR(6)] (abbrev #5)\n"
        "    Successor Map:\n"
        "      Record.Values[0] = 0\n"
        "        Abbreviations:\n"
        "          [1, VBR(6)] (abbrev #0)\n"
        "          [1, 0] (abbrev #4)\n"
        "          [VBR(6), VBR(6)] (abbrev #5)\n"
        "          [VBR(6), 0] (abbrev #6)\n"
        "      Record.Values[0] = 2\n"
        "        Abbreviations:\n"
        "          [1, VBR(6)] (abbrev #0)\n"
        "          [1, 2] (abbrev #3)\n"
        "          [VBR(6), VBR(6)] (abbrev #5)\n"
        "  Record.Code = 4\n"
        "    Abbreviations:\n"
        "      [4, VBR(8)] (abbrev #1)\n"
        "      [VBR(6), VBR(6)] (abbrev #5)\n"
        "    Successor Map:\n"
        "      Record.Values[0] = 0\n"
        "        Abbreviations:\n"
        "          [4, VBR(8)] (abbrev #1)\n"
        "          [4, 0] (abbrev #2)\n"
        "          [VBR(6), VBR(6)] (abbrev #5)\n"
        "          [VBR(6), 0] (abbrev #6)\n"
        "  Record.Values[0] = 0\n"
        "    Abbreviations:\n"
        "      [VBR(6), VBR(6)] (abbrev #5)\n"
        "      [VBR(6), 0] (abbrev #6)\n"),
              DescribeAbbrevTrie(Iter->second));
  }

  // Test matching [1, 0].
  NaClBitcodeRecordData Record;
  Record.Code = 1;
  Record.Values.push_back(0);
  EXPECT_EQ(std::string("[1, 0]"), DescribeRecord(Record));
  {
    AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
    ASSERT_NE(Node, (void*) 0);
    EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [1, VBR(6)] (abbrev #0)\n"
        "  [1, 0] (abbrev #4)\n"
        "  [VBR(6), VBR(6)] (abbrev #5)\n"
        "  [VBR(6), 0] (abbrev #6)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
  }

  // Test matching [1, 2]
  Record.Values[0] = 2;
  EXPECT_EQ(std::string("[1, 2]"), DescribeRecord(Record));
  {
    AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
    ASSERT_NE(Node, (void*) 0);
    EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [1, VBR(6)] (abbrev #0)\n"
        "  [1, 2] (abbrev #3)\n"
        "  [VBR(6), VBR(6)] (abbrev #5)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
  }

  // Test match [1, 8] (i.e. Record.Values[1] ~in {0, 2}).
  Record.Values[0] = 8;
  EXPECT_EQ(std::string("[1, 8]"), DescribeRecord(Record));
  {
    AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
    ASSERT_NE(Node, (void*) 0);
    EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [1, VBR(6)] (abbrev #0)\n"
        "  [VBR(6), VBR(6)] (abbrev #5)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
  }

  // Test match [4, 0]
  Record.Code = 4;
  Record.Values[0] = 0;
  EXPECT_EQ(std::string("[4, 0]"), DescribeRecord(Record));
  {
    AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
    ASSERT_NE(Node, (void*) 0);
    EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [4, VBR(8)] (abbrev #1)\n"
        "  [4, 0] (abbrev #2)\n"
        "  [VBR(6), VBR(6)] (abbrev #5)\n"
        "  [VBR(6), 0] (abbrev #6)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
  }

  // Test match [4, 8] (i.e. Record.Values[1] ~in {0})
  Record.Values[0] = 8;
  EXPECT_EQ(std::string("[4, 8]"), DescribeRecord(Record));
  {
    AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
    ASSERT_NE(Node, (void*) 0);
    EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [4, VBR(8)] (abbrev #1)\n"
        "  [VBR(6), VBR(6)] (abbrev #5)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
  }

  // Test match [8, 0] (i.e. Record.Code ~in {1, 4})
  Record.Code = 8;
  Record.Values[0] = 0;
  EXPECT_EQ(std::string("[8, 0]"), DescribeRecord(Record));
  {
    AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
    ASSERT_NE(Node, (void*) 0);
    EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [VBR(6), VBR(6)] (abbrev #5)\n"
        "  [VBR(6), 0] (abbrev #6)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
  }

  // Test match [7, 6] (i.e. Record.Code ~in {1, 4}
  //                    and  Record.Values[0] ~in {0})
  Record.Code = 7;
  Record.Values[0] = 6;
  EXPECT_EQ(std::string("[7, 6]"), DescribeRecord(Record));
  {
    AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
    ASSERT_NE(Node, (void*) 0);
    EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [VBR(6), VBR(6)] (abbrev #5)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
  }

  // Test match [1, 2, 3] (i.e no abbreviations defined).
  Record.Code = 1;
  Record.Values[0] = 2;
  Record.Values.push_back(3);
  EXPECT_EQ(std::string("[1, 2, 3]"), DescribeRecord(Record));
  {
    AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
    ASSERT_EQ(Node, (void*) 0);
  }

  Clear(Abbrevs);
  Clear(LookupMap);
}

TEST(NaClAbbrevTrieTest, Array) {
  // Test for variable length abbreviations, with some specific
  // unwindings.
  AbbrevVector Abbrevs;
  // [Array(VBR(6))]
  NaClBitCodeAbbrev *Abbrev = new NaClBitCodeAbbrev();
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Array));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));
  Abbrevs.push_back(Abbrev);
  // [VBR(6), VBR(6), 0, VBR(6), VBR(6)]
  Abbrev = new NaClBitCodeAbbrev();
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));
  Abbrev->Add(NaClBitCodeAbbrevOp(0));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));
  Abbrevs.push_back(Abbrev);
  // [8, VBR(6), VBR(6), VBR(6), VBR(6)]
  Abbrev = new NaClBitCodeAbbrev();
  Abbrev->Add(NaClBitCodeAbbrevOp(8));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));
  Abbrevs.push_back(Abbrev);
  // [VBR(6), VBR(6), VBR(6), 0, VBR(6)]
  Abbrev = new NaClBitCodeAbbrev();
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));
  Abbrev->Add(NaClBitCodeAbbrevOp(0));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));
  Abbrevs.push_back(Abbrev);
  // [VBR(6), VBR(6), VBR(6), VBR(6), 3]
  Abbrev = new NaClBitCodeAbbrev();
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));
  Abbrev->Add(NaClBitCodeAbbrevOp(3));
  Abbrevs.push_back(Abbrev);

  // Verify we built the expected abbreviations.
  EXPECT_EQ(std::string(
      "[Array(VBR(6))]\n"
      "[VBR(6), VBR(6), 0, VBR(6), VBR(6)]\n"
      "[8, VBR(6), VBR(6), VBR(6), VBR(6)]\n"
      "[VBR(6), VBR(6), VBR(6), 0, VBR(6)]\n"
      "[VBR(6), VBR(6), VBR(6), VBR(6), 3]\n"),
            DescribeAbbreviations(Abbrevs));

  // Build lookup map, and check that we build the expected trie.
  AbbrevLookupSizeMap LookupMap;
  NaClBuildAbbrevLookupMap(LookupMap, Abbrevs);
  EXPECT_EQ(MaxValueIndex+1, LookupMap.size());
  for (AbbrevLookupSizeMap::iterator
           Iter = LookupMap.begin(), IterEnd = LookupMap.end();
       Iter != IterEnd; ++Iter) {
    EXPECT_LE((size_t)0, Iter->first);
    EXPECT_GE(MaxValueIndex, Iter->first);
    if (Iter->first == 5) {
      // Note that all abbreviations accept records with 5 values.
      EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [Array(VBR(6))] (abbrev #0)\n"
        "Successor Map:\n"
        "  Record.Code = 8\n"
        "    Abbreviations:\n"
        "      [Array(VBR(6))] (abbrev #0)\n"
        "      [8, VBR(6), VBR(6), VBR(6), VBR(6)] (abbrev #2)\n"
        "    Successor Map:\n"
        "      Record.Values[1] = 0\n"
        "        Abbreviations:\n"
        "          [Array(VBR(6))] (abbrev #0)\n"
        "          [VBR(6), VBR(6), 0, VBR(6), VBR(6)] (abbrev #1)\n"
        "          [8, VBR(6), VBR(6), VBR(6), VBR(6)] (abbrev #2)\n"
        "        Successor Map:\n"
        "          Record.Values[2] = 0\n"
        "            Abbreviations:\n"
        "              [Array(VBR(6))] (abbrev #0)\n"
        "              [VBR(6), VBR(6), 0, VBR(6), VBR(6)] (abbrev #1)\n"
        "              [8, VBR(6), VBR(6), VBR(6), VBR(6)] (abbrev #2)\n"
        "              [VBR(6), VBR(6), VBR(6), 0, VBR(6)] (abbrev #3)\n"
        "            Successor Map:\n"
        "              Record.Values[3] = 3\n"
        "                Abbreviations:\n"
        "                  [Array(VBR(6))] (abbrev #0)\n"
        "                  [VBR(6), VBR(6), 0, VBR(6), VBR(6)] (abbrev #1)\n"
        "                  [8, VBR(6), VBR(6), VBR(6), VBR(6)] (abbrev #2)\n"
        "                  [VBR(6), VBR(6), VBR(6), 0, VBR(6)] (abbrev #3)\n"
        "                  [VBR(6), VBR(6), VBR(6), VBR(6), 3] (abbrev #4)\n"
        "          Record.Values[3] = 3\n"
        "            Abbreviations:\n"
        "              [Array(VBR(6))] (abbrev #0)\n"
        "              [VBR(6), VBR(6), 0, VBR(6), VBR(6)] (abbrev #1)\n"
        "              [8, VBR(6), VBR(6), VBR(6), VBR(6)] (abbrev #2)\n"
        "              [VBR(6), VBR(6), VBR(6), VBR(6), 3] (abbrev #4)\n"
        "      Record.Values[2] = 0\n"
        "        Abbreviations:\n"
        "          [Array(VBR(6))] (abbrev #0)\n"
        "          [8, VBR(6), VBR(6), VBR(6), VBR(6)] (abbrev #2)\n"
        "          [VBR(6), VBR(6), VBR(6), 0, VBR(6)] (abbrev #3)\n"
        "        Successor Map:\n"
        "          Record.Values[3] = 3\n"
        "            Abbreviations:\n"
        "              [Array(VBR(6))] (abbrev #0)\n"
        "              [8, VBR(6), VBR(6), VBR(6), VBR(6)] (abbrev #2)\n"
        "              [VBR(6), VBR(6), VBR(6), 0, VBR(6)] (abbrev #3)\n"
        "              [VBR(6), VBR(6), VBR(6), VBR(6), 3] (abbrev #4)\n"
        "      Record.Values[3] = 3\n"
        "        Abbreviations:\n"
        "          [Array(VBR(6))] (abbrev #0)\n"
        "          [8, VBR(6), VBR(6), VBR(6), VBR(6)] (abbrev #2)\n"
        "          [VBR(6), VBR(6), VBR(6), VBR(6), 3] (abbrev #4)\n"
        "  Record.Values[1] = 0\n"
        "    Abbreviations:\n"
        "      [Array(VBR(6))] (abbrev #0)\n"
        "      [VBR(6), VBR(6), 0, VBR(6), VBR(6)] (abbrev #1)\n"
        "    Successor Map:\n"
        "      Record.Values[2] = 0\n"
        "        Abbreviations:\n"
        "          [Array(VBR(6))] (abbrev #0)\n"
        "          [VBR(6), VBR(6), 0, VBR(6), VBR(6)] (abbrev #1)\n"
        "          [VBR(6), VBR(6), VBR(6), 0, VBR(6)] (abbrev #3)\n"
        "        Successor Map:\n"
        "          Record.Values[3] = 3\n"
        "            Abbreviations:\n"
        "              [Array(VBR(6))] (abbrev #0)\n"
        "              [VBR(6), VBR(6), 0, VBR(6), VBR(6)] (abbrev #1)\n"
        "              [VBR(6), VBR(6), VBR(6), 0, VBR(6)] (abbrev #3)\n"
        "              [VBR(6), VBR(6), VBR(6), VBR(6), 3] (abbrev #4)\n"
        "      Record.Values[3] = 3\n"
        "        Abbreviations:\n"
        "          [Array(VBR(6))] (abbrev #0)\n"
        "          [VBR(6), VBR(6), 0, VBR(6), VBR(6)] (abbrev #1)\n"
        "          [VBR(6), VBR(6), VBR(6), VBR(6), 3] (abbrev #4)\n"
        "  Record.Values[2] = 0\n"
        "    Abbreviations:\n"
        "      [Array(VBR(6))] (abbrev #0)\n"
        "      [VBR(6), VBR(6), VBR(6), 0, VBR(6)] (abbrev #3)\n"
        "    Successor Map:\n"
        "      Record.Values[3] = 3\n"
        "        Abbreviations:\n"
        "          [Array(VBR(6))] (abbrev #0)\n"
        "          [VBR(6), VBR(6), VBR(6), 0, VBR(6)] (abbrev #3)\n"
        "          [VBR(6), VBR(6), VBR(6), VBR(6), 3] (abbrev #4)\n"
        "  Record.Values[3] = 3\n"
        "    Abbreviations:\n"
        "      [Array(VBR(6))] (abbrev #0)\n"
        "      [VBR(6), VBR(6), VBR(6), VBR(6), 3] (abbrev #4)\n"),
                DescribeAbbrevTrie(Iter->second));
    } else {
      // When the record doesn't contain 5 values, only
      // abbreviation [Array(VBR(6))] applies.
      EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [Array(VBR(6))] (abbrev #0)\n"),
                DescribeAbbrevTrie(Iter->second));
    }
  }

  // Test matching [8, 10, 0, 0, 3].
  NaClBitcodeRecordData Record;
  Record.Code = 8;
  Record.Values.push_back(10);
  Record.Values.push_back(0);
  Record.Values.push_back(0);
  Record.Values.push_back(3);
  EXPECT_EQ(std::string("[8, 10, 0, 0, 3]"), DescribeRecord(Record));
  {
    AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
    ASSERT_NE(Node, (void*) 0);
    EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [Array(VBR(6))] (abbrev #0)\n"
        "  [VBR(6), VBR(6), 0, VBR(6), VBR(6)] (abbrev #1)\n"
        "  [8, VBR(6), VBR(6), VBR(6), VBR(6)] (abbrev #2)\n"
        "  [VBR(6), VBR(6), VBR(6), 0, VBR(6)] (abbrev #3)\n"
        "  [VBR(6), VBR(6), VBR(6), VBR(6), 3] (abbrev #4)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
  }

  // Test matching [8, 10, 0, 11, 3].
  Record.Values[2] = 11;
  EXPECT_EQ(std::string("[8, 10, 0, 11, 3]"), DescribeRecord(Record));
  {
    AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
    ASSERT_NE(Node, (void*) 0);
    EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [Array(VBR(6))] (abbrev #0)\n"
        "  [VBR(6), VBR(6), 0, VBR(6), VBR(6)] (abbrev #1)\n"
        "  [8, VBR(6), VBR(6), VBR(6), VBR(6)] (abbrev #2)\n"
        "  [VBR(6), VBR(6), VBR(6), VBR(6), 3] (abbrev #4)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
  }

  // Test matching [8, 10, 0, 11, 12].
  Record.Values[3] = 12;
  EXPECT_EQ(std::string("[8, 10, 0, 11, 12]"), DescribeRecord(Record));
  {
    AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
    ASSERT_NE(Node, (void*) 0);
    EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [Array(VBR(6))] (abbrev #0)\n"
        "  [VBR(6), VBR(6), 0, VBR(6), VBR(6)] (abbrev #1)\n"
        "  [8, VBR(6), VBR(6), VBR(6), VBR(6)] (abbrev #2)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
  }

  // Test matching [13, 10, 0, 0, 3].
  Record.Code = 13;
  Record.Values[2] = 0;
  Record.Values[3] = 3;
  EXPECT_EQ(std::string("[13, 10, 0, 0, 3]"), DescribeRecord(Record));
  {
    AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
    ASSERT_NE(Node, (void*) 0);
    EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [Array(VBR(6))] (abbrev #0)\n"
        "  [VBR(6), VBR(6), 0, VBR(6), VBR(6)] (abbrev #1)\n"
        "  [VBR(6), VBR(6), VBR(6), 0, VBR(6)] (abbrev #3)\n"
        "  [VBR(6), VBR(6), VBR(6), VBR(6), 3] (abbrev #4)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
  }

  // Test matching [13, 10, 0, 0, 14].
  Record.Values[3] = 14;
  EXPECT_EQ(std::string("[13, 10, 0, 0, 14]"), DescribeRecord(Record));
  {
    AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
    ASSERT_NE(Node, (void*) 0);
    EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [Array(VBR(6))] (abbrev #0)\n"
        "  [VBR(6), VBR(6), 0, VBR(6), VBR(6)] (abbrev #1)\n"
        "  [VBR(6), VBR(6), VBR(6), 0, VBR(6)] (abbrev #3)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
  }

  // Test matching [13, 10, 0, 15, 3].
  Record.Values[2] = 15;
  Record.Values[3] = 3;
  EXPECT_EQ(std::string("[13, 10, 0, 15, 3]"), DescribeRecord(Record));
  {
    AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
    ASSERT_NE(Node, (void*) 0);
    EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [Array(VBR(6))] (abbrev #0)\n"
        "  [VBR(6), VBR(6), 0, VBR(6), VBR(6)] (abbrev #1)\n"
        "  [VBR(6), VBR(6), VBR(6), VBR(6), 3] (abbrev #4)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
  }

  // Test matching [13, 10, 0, 15, 14].
  Record.Values[3] = 14;
  EXPECT_EQ(std::string("[13, 10, 0, 15, 14]"), DescribeRecord(Record));
  {
    AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
    ASSERT_NE(Node, (void*) 0);
    EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [Array(VBR(6))] (abbrev #0)\n"
        "  [VBR(6), VBR(6), 0, VBR(6), VBR(6)] (abbrev #1)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
  }

  // Test matching [13, 10, 16, 0, 3].
  Record.Values[1] = 16;
  Record.Values[2] = 0;
  Record.Values[3] = 3;
  EXPECT_EQ(std::string("[13, 10, 16, 0, 3]"), DescribeRecord(Record));
  {
    AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
    ASSERT_NE(Node, (void*) 0);
    EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [Array(VBR(6))] (abbrev #0)\n"
        "  [VBR(6), VBR(6), VBR(6), 0, VBR(6)] (abbrev #3)\n"
        "  [VBR(6), VBR(6), VBR(6), VBR(6), 3] (abbrev #4)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
  }

  // Test matching [13, 10, 16, 0, 17].
  Record.Values[3] = 17;
  EXPECT_EQ(std::string("[13, 10, 16, 0, 17]"), DescribeRecord(Record));
  {
    AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
    ASSERT_NE(Node, (void*) 0);
    EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [Array(VBR(6))] (abbrev #0)\n"
        "  [VBR(6), VBR(6), VBR(6), 0, VBR(6)] (abbrev #3)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
  }

  // Test matching [13, 10, 16, 18, 3].
  Record.Values[2] = 18;
  Record.Values[3] = 3;
  EXPECT_EQ(std::string("[13, 10, 16, 18, 3]"), DescribeRecord(Record));
  {
    AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
    ASSERT_NE(Node, (void*) 0);
    EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [Array(VBR(6))] (abbrev #0)\n"
        "  [VBR(6), VBR(6), VBR(6), VBR(6), 3] (abbrev #4)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
  }

  // Test matching [13, 10, 16, 18, 19].
  Record.Values[3] = 19;
  EXPECT_EQ(std::string("[13, 10, 16, 18, 19]"), DescribeRecord(Record));
  {
    AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
    ASSERT_NE(Node, (void*) 0);
    EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [Array(VBR(6))] (abbrev #0)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
  }

  // Test matching [13, 10, 16, 18, 19, 20, 21, 22, 23, 24, 25]
  Record.Values.push_back(20);
  Record.Values.push_back(21);
  Record.Values.push_back(22);
  Record.Values.push_back(23);
  Record.Values.push_back(24);
  Record.Values.push_back(25);
  EXPECT_EQ(std::string("[13, 10, 16, 18, 19, 20, 21, 22, 23, 24, 25]"),
            DescribeRecord(Record));
  {
    AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
    ASSERT_EQ(Node, (void*) 0);
  }

  Clear(Abbrevs);
  Clear(LookupMap);
}

TEST(NaClAbbrevTrieTest, NonsimpleArray) {
  // Test case where Array doesn't appear first.
  AbbrevVector Abbrevs;
  // [Fixed(3), VBR(8), Array(Fixed(8))]
  NaClBitCodeAbbrev *Abbrev = new NaClBitCodeAbbrev();
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, 3));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 8));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Array));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, 8));
  Abbrevs.push_back(Abbrev);
  // [1, VBR(8), Array(Fixed(7))]
  Abbrev = new NaClBitCodeAbbrev();
  Abbrev->Add(NaClBitCodeAbbrevOp(1));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 8));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Array));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, 7));
  Abbrevs.push_back(Abbrev);
  // [1, VBR(8), Array(Char6)]
  Abbrev = new NaClBitCodeAbbrev();
  Abbrev->Add(NaClBitCodeAbbrevOp(1));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 8));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Array));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Char6));
  Abbrevs.push_back(Abbrev);
  // [2, VBR(8), Array(Char6)]
  Abbrev = new NaClBitCodeAbbrev();
  Abbrev->Add(NaClBitCodeAbbrevOp(2));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 8));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Array));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Char6));
  Abbrevs.push_back(Abbrev);
  // [2, Array(VBR(8))]
  Abbrev = new NaClBitCodeAbbrev();
  Abbrev->Add(NaClBitCodeAbbrevOp(2));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Array));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 8));
  Abbrevs.push_back(Abbrev);
  // [Fixed(3), VBR(8), 5, Array(Fixed(8))]
  Abbrev = new NaClBitCodeAbbrev();
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, 3));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 8));
  Abbrev->Add(NaClBitCodeAbbrevOp(5));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Array));
  Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Fixed, 8));
  Abbrevs.push_back(Abbrev);

  // Verify we built the expected abbreviations.
  EXPECT_EQ(std::string(
        "[Fixed(3), VBR(8), Array(Fixed(8))]\n"
        "[1, VBR(8), Array(Fixed(7))]\n"
        "[1, VBR(8), Array(Char6)]\n"
        "[2, VBR(8), Array(Char6)]\n"
        "[2, Array(VBR(8))]\n"
        "[Fixed(3), VBR(8), 5, Array(Fixed(8))]\n"),
            DescribeAbbreviations(Abbrevs));

  // Build lookup map, and check that we build the expected trie.
  AbbrevLookupSizeMap LookupMap;
  NaClBuildAbbrevLookupMap(LookupMap, Abbrevs);
  // Note: Above abbreviations accept all record lengths but 0. Hence,
  // there should be one for each possible (truncated) record length
  // except zero.
  EXPECT_EQ(MaxValueIndex, LookupMap.size())
      << "Should accept all (truncated) record lengths (except 0)";
  for (AbbrevLookupSizeMap::iterator
           Iter = LookupMap.begin(), IterEnd = LookupMap.end();
       Iter != IterEnd; ++Iter) {
    NaClBitcodeRecordData Record;
    switch (Iter->first) {
    case 0:
      ASSERT_FALSE(true) << "There are not abbreviations of length 0";
      break;
    case 1:
      EXPECT_EQ(std::string(
        "Successor Map:\n"
        "  Record.Code = 2\n"
        "    Abbreviations:\n"
        "      [2, Array(VBR(8))] (abbrev #4)\n"),
                DescribeAbbrevTrie(Iter->second));

      // Test matching [2]
      Record.Code = 2;
      EXPECT_EQ(std::string("[2]"), DescribeRecord(Record));
      {
        AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
        ASSERT_NE(Node, (void*) 0);
        EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [2, Array(VBR(8))] (abbrev #4)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
      }

      // Test matching [5];
      Record.Code = 5;
      EXPECT_EQ(std::string("[5]"), DescribeRecord(Record));
      {
        AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
        ASSERT_NE(Node, (void*) 0);
        EXPECT_EQ(std::string(""),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
      }
      // Test matching [2, 10]
      Record.Code = 2;
      Record.Values.push_back(10);
      EXPECT_EQ(std::string("[2, 10]"), DescribeRecord(Record));
      {
        AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
        ASSERT_NE(Node, (void*) 0);
        EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [Fixed(3), VBR(8), Array(Fixed(8))] (abbrev #0)\n"
        "  [2, VBR(8), Array(Char6)] (abbrev #3)\n"
        "  [2, Array(VBR(8))] (abbrev #4)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
      }
      break;
    case 2:
      EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [Fixed(3), VBR(8), Array(Fixed(8))] (abbrev #0)\n"
        "Successor Map:\n"
        "  Record.Code = 1\n"
        "    Abbreviations:\n"
        "      [Fixed(3), VBR(8), Array(Fixed(8))] (abbrev #0)\n"
        "      [1, VBR(8), Array(Fixed(7))] (abbrev #1)\n"
        "      [1, VBR(8), Array(Char6)] (abbrev #2)\n"
        "  Record.Code = 2\n"
        "    Abbreviations:\n"
        "      [Fixed(3), VBR(8), Array(Fixed(8))] (abbrev #0)\n"
        "      [2, VBR(8), Array(Char6)] (abbrev #3)\n"
        "      [2, Array(VBR(8))] (abbrev #4)\n"),
                DescribeAbbrevTrie(Iter->second));

      // Test matching [1, 5]
      Record.Code = 1;
      Record.Values.push_back(5);
      EXPECT_EQ(std::string("[1, 5]"), DescribeRecord(Record));
      {
        AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
        ASSERT_NE(Node, (void*) 0);
        EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [Fixed(3), VBR(8), Array(Fixed(8))] (abbrev #0)\n"
        "  [1, VBR(8), Array(Fixed(7))] (abbrev #1)\n"
        "  [1, VBR(8), Array(Char6)] (abbrev #2)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
      }
      // Test matching [2, 5]
      Record.Code = 2;
      EXPECT_EQ(std::string("[2, 5]"), DescribeRecord(Record));
      {
        AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
        ASSERT_NE(Node, (void*) 0);
        EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [Fixed(3), VBR(8), Array(Fixed(8))] (abbrev #0)\n"
        "  [2, VBR(8), Array(Char6)] (abbrev #3)\n"
        "  [2, Array(VBR(8))] (abbrev #4)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
      }
      // Test matching [3, 5]
      Record.Code = 3;
      EXPECT_EQ(std::string("[3, 5]"), DescribeRecord(Record));
      {
        AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
        ASSERT_NE(Node, (void*) 0);
        EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [Fixed(3), VBR(8), Array(Fixed(8))] (abbrev #0)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
      }
      break;
    case 3:
    default:
      EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [Fixed(3), VBR(8), Array(Fixed(8))] (abbrev #0)\n"
        "Successor Map:\n"
        "  Record.Code = 1\n"
        "    Abbreviations:\n"
        "      [Fixed(3), VBR(8), Array(Fixed(8))] (abbrev #0)\n"
        "      [1, VBR(8), Array(Fixed(7))] (abbrev #1)\n"
        "      [1, VBR(8), Array(Char6)] (abbrev #2)\n"
        "    Successor Map:\n"
        "      Record.Values[1] = 5\n"
        "        Abbreviations:\n"
        "          [Fixed(3), VBR(8), Array(Fixed(8))] (abbrev #0)\n"
        "          [1, VBR(8), Array(Fixed(7))] (abbrev #1)\n"
        "          [1, VBR(8), Array(Char6)] (abbrev #2)\n"
        "          [Fixed(3), VBR(8), 5, Array(Fixed(8))] (abbrev #5)\n"
        "  Record.Code = 2\n"
        "    Abbreviations:\n"
        "      [Fixed(3), VBR(8), Array(Fixed(8))] (abbrev #0)\n"
        "      [2, VBR(8), Array(Char6)] (abbrev #3)\n"
        "      [2, Array(VBR(8))] (abbrev #4)\n"
        "    Successor Map:\n"
        "      Record.Values[1] = 5\n"
        "        Abbreviations:\n"
        "          [Fixed(3), VBR(8), Array(Fixed(8))] (abbrev #0)\n"
        "          [2, VBR(8), Array(Char6)] (abbrev #3)\n"
        "          [2, Array(VBR(8))] (abbrev #4)\n"
        "          [Fixed(3), VBR(8), 5, Array(Fixed(8))] (abbrev #5)\n"
        "  Record.Values[1] = 5\n"
        "    Abbreviations:\n"
        "      [Fixed(3), VBR(8), Array(Fixed(8))] (abbrev #0)\n"
        "      [Fixed(3), VBR(8), 5, Array(Fixed(8))] (abbrev #5)\n"),
                DescribeAbbrevTrie(Iter->second));

      // Test matching [1, 0, 5]
      Record.Code = 1;
      Record.Values.push_back(0);
      Record.Values.push_back(5);
      EXPECT_EQ(std::string("[1, 0, 5]"), DescribeRecord(Record));
      {
        AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
        ASSERT_NE(Node, (void*) 0);
        EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [Fixed(3), VBR(8), Array(Fixed(8))] (abbrev #0)\n"
        "  [1, VBR(8), Array(Fixed(7))] (abbrev #1)\n"
        "  [1, VBR(8), Array(Char6)] (abbrev #2)\n"
        "  [Fixed(3), VBR(8), 5, Array(Fixed(8))] (abbrev #5)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
      }
      // Test matching [1, 0, 50]
      Record.Values[1] = 50;
      EXPECT_EQ(std::string("[1, 0, 50]"), DescribeRecord(Record));
      {
        AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
        ASSERT_NE(Node, (void*) 0);
        EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [Fixed(3), VBR(8), Array(Fixed(8))] (abbrev #0)\n"
        "  [1, VBR(8), Array(Fixed(7))] (abbrev #1)\n"
        "  [1, VBR(8), Array(Char6)] (abbrev #2)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
      }
      // Test matching [2, 0, 5]
      Record.Code = 2;
      Record.Values[1] = 5;
      EXPECT_EQ(std::string("[2, 0, 5]"), DescribeRecord(Record));
      {
        AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
        ASSERT_NE(Node, (void*) 0);
        EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [Fixed(3), VBR(8), Array(Fixed(8))] (abbrev #0)\n"
        "  [2, VBR(8), Array(Char6)] (abbrev #3)\n"
        "  [2, Array(VBR(8))] (abbrev #4)\n"
        "  [Fixed(3), VBR(8), 5, Array(Fixed(8))] (abbrev #5)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
      }
      // Test matching [2, 0, 50]
      Record.Values[1] = 50;
      EXPECT_EQ(std::string("[2, 0, 50]"), DescribeRecord(Record));
      {
        AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
        ASSERT_NE(Node, (void*) 0);
        EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [Fixed(3), VBR(8), Array(Fixed(8))] (abbrev #0)\n"
        "  [2, VBR(8), Array(Char6)] (abbrev #3)\n"
        "  [2, Array(VBR(8))] (abbrev #4)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
      }
      // Test matching [5, 0, 5]
      Record.Code = 5;
      Record.Values[1] = 5;
      EXPECT_EQ(std::string("[5, 0, 5]"), DescribeRecord(Record));
      {
        AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
        ASSERT_NE(Node, (void*) 0);
        EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [Fixed(3), VBR(8), Array(Fixed(8))] (abbrev #0)\n"
        "  [Fixed(3), VBR(8), 5, Array(Fixed(8))] (abbrev #5)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
      }
      // Test matching [5, 0, 50]
      Record.Values[1] = 50;
      EXPECT_EQ(std::string("[5, 0, 50]"), DescribeRecord(Record));
      {
        AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
        ASSERT_NE(Node, (void*) 0);
        EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [Fixed(3), VBR(8), Array(Fixed(8))] (abbrev #0)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
      }
      // Test matching [5, 0, 50, 10]
      Record.Values.push_back(10);
      EXPECT_EQ(std::string("[5, 0, 50, 10]"), DescribeRecord(Record));
      {
        AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
        ASSERT_NE(Node, (void*) 0);
        EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [Fixed(3), VBR(8), Array(Fixed(8))] (abbrev #0)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
      }
      // Test matching [5, 0, 50, 10, 20]
      Record.Values.push_back(20);
      EXPECT_EQ(std::string("[5, 0, 50, 10, 20]"), DescribeRecord(Record));
      {
        AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
        ASSERT_NE(Node, (void*) 0);
        EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [Fixed(3), VBR(8), Array(Fixed(8))] (abbrev #0)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
      }
      // Test matching [1, 0, 5, 10, 20]
      Record.Code = 1;
      Record.Values[1] = 5;
      EXPECT_EQ(std::string("[1, 0, 5, 10, 20]"), DescribeRecord(Record));
      {
        AbbrevTrieNode *Node = LookupMap[Record.Values.size()+1];
        ASSERT_NE(Node, (void*) 0);
        EXPECT_EQ(std::string(
        "Abbreviations:\n"
        "  [Fixed(3), VBR(8), Array(Fixed(8))] (abbrev #0)\n"
        "  [1, VBR(8), Array(Fixed(7))] (abbrev #1)\n"
        "  [1, VBR(8), Array(Char6)] (abbrev #2)\n"
        "  [Fixed(3), VBR(8), 5, Array(Fixed(8))] (abbrev #5)\n"),
              DescribeAbbrevTrieNode(Node->MatchRecord(Record)));
      }
      break;
    }
  }

  Clear(Abbrevs);
  Clear(LookupMap);
}

}
