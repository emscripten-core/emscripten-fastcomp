//===- AbbrevTrieNode.h - Abbreviation lookup tries         ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This header file defines class AbbrevTrieNode that implement abbreviation
// lookup tries. These tries reduce the set of abbreviations that need to
// be tested for best fit to a pnacl bitcode record, by sorting abbreviations
// on literal constants that may appear in the abbreviations. By doing this,
// we can reduce hundreds of possible abbreviations down to a small number
// of possibly applicable abbreviations.
//
// The tries separate abbreviations based on constant size, and constants
// that appear in the abbreviations. The trie is used to capture constants
// that appear at any index, and use these constants to decide if a trie
// node applies to the record.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_BITCODE_NACL_ABBREV_TRIE_NODE_H
#define LLVM_BITCODE_NACL_ABBREV_TRIE_NODE_H

#include "llvm/Bitcode/NaCl/NaClBitcodeParser.h"
#include <map>
#include <set>

namespace llvm {

// Models the association of an abbreviation index with the
// corresponding abbreviation associated with that index.
typedef std::pair<size_t, NaClBitCodeAbbrev*> AbbrevIndexPair;

// Models a trie of abbreviation matches that can be used to reduce
// the number of applicable abbreviations. This is done by moving
// abbreviations that require literals to appear in them, to be in
// successor nodes. Abbreviations without literals are stored
// in this node.
class AbbrevTrieNode {
  // For faster lookup, we could model the successor map as
  // std::map<std::pair<size_t, uint64_t>, AbbrevTrieNode*>.  However,
  // we split up the pair into a nested map. This allows us to reduce
  // the size of the domain of the map considerably, as well as
  // avoiding much value (i.e.  std::pair<size_t, uint64_t>)
  // copying. These modifications result in considerable better time
  // performance.
  //
  // We also do this representation because the trie is sparse,
  // with respect to where constants can appear. Hence, we don't
  // built a possible successor for all possible indicies, but only
  // for those that can contain constants in some abbreviation.
  typedef std::map<uint64_t, AbbrevTrieNode*> SuccessorValueMap;
  typedef std::map<size_t, SuccessorValueMap*> SuccessorMap;

public:
  // Models the list of successor labels defined for a node.
  typedef std::vector< std::pair<size_t, uint64_t> > SuccessorLabels;

  // Creates an entry node into the trie.
  AbbrevTrieNode() {}

  ~AbbrevTrieNode();

  // Print out the trie to the given stream, indenting the given
  // amount.  If LocalOnly is true, no successor information is
  // printed.
  void Print(raw_ostream &Stream,
             const std::string &Indent,
             bool LocalOnly=false) const;

  // Adds matching constants, defined in Abbreviation, to the trie.
  // Returns true if any nodes were added to the trie to add the given
  // abbreviation. Note: This method only creates nodes, Abbreviations
  // must be aded in a separate pass using method Insert.  Note: If
  // you call NaClBuildAbbrevLookupMap, it will construct the
  // (complete) abbrevation trie, calling Add and Insert in the
  // appropriate order.
  bool Add(NaClBitCodeAbbrev *Abbrev) {
    return Add(Abbrev, 0, 0);
  }

  // Inserts the given abbreviation (pair) in all trie nodes that
  // might match the given abbreviation. Should not be called until
  // all trie nodes are built using Add.
  void Insert(AbbrevIndexPair &AbbrevPair);

  // Returns the successor trie node matching the given Index/Value pair.
  AbbrevTrieNode* GetSuccessor(size_t Index, uint64_t Value) const;

  // Collects the set of successor (edge) labels defined for the node.
  void GetSuccessorLabels(SuccessorLabels &labels) const;

  // Returns a trie node (in the trie) that defines all possible
  // abbreviations that may apply to the given record.
  const AbbrevTrieNode *MatchRecord(const NaClBitcodeRecordData &Record) const;

  // Returns the abbreviations associated with the node.
  const std::set<AbbrevIndexPair> &GetAbbreviations() const {
    return Abbreviations;
  }

private:
  // Adds matching constants, defined in Abbreviation, to the trie.
  // Returns true if any nodes were added to the trie to add the given
  // abbreviation. Index is the positition (within the abbreviation)
  // where search should begin for the next available
  // literal. SkipIndex is the smallest skipped index used to find the
  // next available literal.
  bool Add(NaClBitCodeAbbrev *Abbrev,
           size_t Index,
           size_t SkipIndex);

  // The set of possible successor trie nodes defined for this node.
  SuccessorMap Successors;

  // The set of abbreviations that apply if one can't match a pnacl
  // bitcode record against any of the successors.
  std::set<AbbrevIndexPair> Abbreviations;
};

// A map from record sizes, to the corresponding trie one should use
// to find abbreviations for records of that size.
typedef std::map<size_t, AbbrevTrieNode*> AbbrevLookupSizeMap;

// Builds abbreviation lookup trie for abbreviations
void NaClBuildAbbrevLookupMap(AbbrevLookupSizeMap &LookupMap,
                              const SmallVectorImpl<NaClBitCodeAbbrev*> &Abbrevs,
                              size_t InitialIndex = 0);

}

#endif
