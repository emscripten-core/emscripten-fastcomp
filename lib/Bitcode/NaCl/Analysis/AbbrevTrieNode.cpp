//===- AbbrevTrieNode.cpp ------------------------------------------------===//
//     Implements abbreviation lookup tries.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/NaCl/AbbrevTrieNode.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeValueDist.h"

using namespace llvm;

void AbbrevTrieNode::GetSuccessorLabels(SuccessorLabels &Labels) const {
  for (SuccessorMap::const_iterator
           IndexIter = Successors.begin(), IndexIterEnd = Successors.end();
       IndexIter != IndexIterEnd; ++IndexIter) {
    if (const SuccessorValueMap *ValueMap = IndexIter->second) {
      for (SuccessorValueMap::const_iterator
               ValueIter = ValueMap->begin(), ValueIterEnd = ValueMap->end();
           ValueIter != ValueIterEnd; ++ValueIter) {
        Labels.push_back(std::pair<size_t, uint64_t>(IndexIter->first,
                                                     ValueIter->first));
      }
    }
  }
}

AbbrevTrieNode::~AbbrevTrieNode() {
  for (SuccessorMap::const_iterator
           Iter = Successors.begin(), IterEnd = Successors.end();
       Iter != IterEnd; ++Iter) {
    if (const SuccessorValueMap *ValueMap = Iter->second) {
      for (SuccessorValueMap::const_iterator
               Iter = ValueMap->begin(), IterEnd = ValueMap->end();
           Iter != IterEnd; ++Iter) {
        delete Iter->second;
      }
      delete ValueMap;
    }
  }
}

void AbbrevTrieNode::Print(raw_ostream &Stream,
                           const std::string &Indent,
                           bool LocalOnly) const {
  std::string IndentPlus(Indent);
  IndentPlus.append("  ");
  std::string IndentPlusPlus(IndentPlus);
  IndentPlusPlus.append("  ");
  if (! Abbreviations.empty()) {
    Stream << Indent << "Abbreviations:\n";
    for (std::set<AbbrevIndexPair>::const_iterator
             Iter = Abbreviations.begin(), IterEnd = Abbreviations.end();
         Iter != IterEnd; ++Iter) {
      Stream << IndentPlus;
      Iter->second->Print(Stream, /* AddNewline= */ false);
      Stream << " (abbrev #" << Iter->first << ")\n";
    }
  }
  if (LocalOnly) return;
  if (!Successors.empty()) {
    Stream << Indent << "Successor Map:\n";
    SuccessorLabels Labels;
    GetSuccessorLabels(Labels);
    for (SuccessorLabels::const_iterator
             Iter = Labels.begin(), IterEnd = Labels.end();
         Iter != IterEnd; ++Iter) {
      size_t Index = Iter->first;
      if (Index == 0) {
        Stream << IndentPlus << "Record.Code = " << Iter->second << "\n";
      } else {
        Stream << IndentPlus << "Record.Values[" << (Index-1)
               << "] = " << Iter->second << "\n";
      }
      GetSuccessor(Iter->first, Iter->second)->Print(Stream, IndentPlusPlus);
    }
  }
}

AbbrevTrieNode *AbbrevTrieNode::
GetSuccessor(size_t Index, uint64_t Value) const {
  SuccessorMap::const_iterator IndexPos = Successors.find(Index);
  if (IndexPos != Successors.end()) {
    if (SuccessorValueMap *ValueMap = IndexPos->second) {
      SuccessorValueMap::iterator ValuePos = ValueMap->find(Value);
      if (ValuePos != ValueMap->end())
        return ValuePos->second;
    }
  }
  return 0;
}

bool AbbrevTrieNode::Add(NaClBitCodeAbbrev *Abbrev,
                         size_t Index, size_t SkipIndex) {
  if (Index >= Abbrev->getNumOperandInfos()) return false;
  bool AddedNodes = false;

  // Skip over matches that may match because they don't have constants
  // in the index.
  while (SkipIndex < Index) {
    SuccessorMap::iterator Pos = Successors.find(SkipIndex);
    if (Pos != Successors.end()) {
      SuccessorValueMap *ValueMap = Pos->second;
      for (SuccessorValueMap::const_iterator
               Iter = ValueMap->begin(), IterEnd = ValueMap->end();
           Iter != IterEnd; ++Iter) {
        if (AbbrevTrieNode *Successor = Iter->second) {
          if (Successor->Add(Abbrev, Index, SkipIndex+1))
            AddedNodes = true;
        }
      }
    }
    ++SkipIndex;
  }

  // Now update successors for next matching constant in abbreviation.
  for (; Index < Abbrev->GetMinRecordSize(); ++Index) {
    const NaClBitCodeAbbrevOp &Op = Abbrev->getOperandInfo(Index);
    if (Op.isLiteral()) {
      if (Index == SkipIndex) {
        // No preceeding nodes to match, add here.
        SuccessorValueMap *ValueMap = Successors[Index];
        if (ValueMap == 0) {
          ValueMap = new SuccessorValueMap();
          Successors[Index] = ValueMap;
        }
        AbbrevTrieNode *Successor = (*ValueMap)[Op.getValue()];
        if (Successor == 0) {
          Successor = new AbbrevTrieNode();
          AddedNodes = true;
          (*ValueMap)[Op.getValue()] = Successor;
        }
        if (Successor->Add(Abbrev, Index+1, Index+1))
          AddedNodes = true;
      } else {
        // Need to match all possible prefixes before inserting constant.
        if (Add(Abbrev, Index, SkipIndex))
          AddedNodes = true;
      }
      return AddedNodes;
    }
  }
  return AddedNodes;
}

void AbbrevTrieNode::Insert(AbbrevIndexPair &AbbrevPair) {
  NaClBitCodeAbbrev *Abbrev = AbbrevPair.second;

  for (std::map<size_t, SuccessorValueMap*>::iterator
           Iter = Successors.begin(), IterEnd = Successors.end();
       Iter != IterEnd; ++Iter) {
    if (SuccessorValueMap *ValueMap = Iter->second) {
      if (Iter->first < Abbrev->GetMinRecordSize()) {
        const NaClBitCodeAbbrevOp &Op =
            Abbrev->getOperandInfo(Iter->first);
        if (Op.isLiteral()) {
          uint64_t Value = Op.getValue();
          SuccessorValueMap::iterator Pos = ValueMap->find(Value);
          if (Pos != ValueMap->end()) {
            if (AbbrevTrieNode *Next = Pos->second) {
              // Found constant that will be followed, update subgraph
              // and quit.
              Next->Insert(AbbrevPair);
              return;
            }
          }
        } else {
          // Not literal, so it may (or may not) be followed, depending
          // on the value in the record that will be matched against. So
          // add to subgraph, and here if no succeeding matching literal
          // constants.
          for (SuccessorValueMap::iterator
                   Iter = ValueMap->begin(), IterEnd = ValueMap->end();
               Iter != IterEnd; ++Iter) {
            if (AbbrevTrieNode *Next = Iter->second)
              Next->Insert(AbbrevPair);
          }
        }
      } else if (!Abbrev->IsFixedSize()) {
        // May match array element. So add to subgraph as well as here.
        for (SuccessorValueMap::iterator
                 Iter = ValueMap->begin(), IterEnd = ValueMap->end();
             Iter != IterEnd; ++Iter) {
          if (AbbrevTrieNode *Next = Iter->second)
            Next->Insert(AbbrevPair);
        }
      }
    }
  }

  // If reached, no guarantees that any edge was followed, so add
  // to matches of this node.
  Abbreviations.insert(AbbrevPair);
}

const AbbrevTrieNode *AbbrevTrieNode::
MatchRecord(const NaClBitcodeRecordData &Record) const {
  for (std::map<size_t, SuccessorValueMap*>::const_iterator
           Iter = Successors.begin(), IterEnd = Successors.end();
       Iter != IterEnd; ++Iter) {
    if (SuccessorValueMap *ValueMap = Iter->second) {
      if (Iter->first <= Record.Values.size()) {
        uint64_t Value;
        if (Iter->first == 0)
          Value = Record.Code;
        else
          Value = Record.Values[Iter->first-1];
        SuccessorValueMap::iterator Pos = ValueMap->find(Value);
        if (Pos != ValueMap->end()) {
          if (AbbrevTrieNode *Next = Pos->second) {
            return Next->MatchRecord(Record);
          }
        }
      } else {
        // Map index too big, quit.
        break;
      }
    }
  }

  // If reached, no refinement found, use this node.
  return this;
}

static void ComputeAbbrevRange(NaClBitCodeAbbrev *Abbrev,
                               size_t &MinIndex, size_t &MaxIndex) {
  // Find the range of record lengths for which the abbreviation may
  // apply. Note: To keep a limit on the number of copies, collapse all
  // records with length > NaClValueIndexCutoff into the same bucket.
  MinIndex = Abbrev->GetMinRecordSize();
  if (MinIndex > NaClValueIndexCutoff) {
    MinIndex = NaClValueIndexCutoff + 1;
  }
  MaxIndex = MinIndex;
  if (!Abbrev->IsFixedSize()) {
    MaxIndex = NaClValueIndexCutoff + 1;
  }
}

// Once all nodes have been added (via calls to AddAbbrevToLookupMap),
// this function adds the given abbreviation pair to all possible
// matchxes in the lookup map.
static void AddAbbrevPairToLookupMap(AbbrevLookupSizeMap &LookupMap,
                                     AbbrevIndexPair &AbbrevPair) {
  size_t MinIndex, MaxIndex;
  ComputeAbbrevRange(AbbrevPair.second, MinIndex, MaxIndex);
  for (size_t Index = MinIndex; Index <= MaxIndex; ++Index) {
    AbbrevTrieNode *Node = LookupMap[Index];
    assert(Node);
    Node->Insert(AbbrevPair);
  }
}

// Adds the given abbreviation to the corresponding lookup map, constructing
// the map of usable lookup tries.
static bool AddAbbrevToLookupMap(AbbrevLookupSizeMap &LookupMap,
                                 NaClBitCodeAbbrev *Abbrev) {
  bool Added = false;
  size_t MinIndex, MaxIndex;
  ComputeAbbrevRange(Abbrev, MinIndex, MaxIndex);
  for (size_t Index = MinIndex; Index <= MaxIndex; ++Index) {
    AbbrevTrieNode *Node = LookupMap[Index];
    if (Node == 0) {
      Node = new AbbrevTrieNode();
      LookupMap[Index] = Node;
      Added = true;
    }
    if (Node->Add(Abbrev)) Added = true;
  }
  return Added;
}

void llvm::NaClBuildAbbrevLookupMap(
    AbbrevLookupSizeMap &LookupMap,
    const SmallVectorImpl<NaClBitCodeAbbrev*> &Abbrevs,
    size_t InitialIndex) {
  // First build nodes of trie.
  bool FixpointFound = false;
  while (!FixpointFound) {
    FixpointFound = true;
    for (size_t i = InitialIndex; i < Abbrevs.size(); ++i) {
      if (AddAbbrevToLookupMap(LookupMap, Abbrevs[i]))
        FixpointFound = false;
    }
  }

  // Now populate with abbreviations that apply.
  for (size_t i = InitialIndex; i < Abbrevs.size(); ++i) {
    AbbrevIndexPair Pair(i, Abbrevs[i]);
    AddAbbrevPairToLookupMap(LookupMap, Pair);
  }
}
