//===-- NaClCompress.cpp - Bitcode (abbrev) compression -----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Analyzes the data in memory buffer, and determines what
// abbreviations can be added to compress the bitcode file. The result
// is written to an output stream.
//
// A bitcode file has two types of abbreviations. The first are Global
// abbreviations that apply to all instances of a particular type of
// block.  These abbreviations appear in the BlockInfo block of the
// bitcode file.
//
// The second type of abbreviations are local to a particular instance
// of a block.
//
// For simplicity, we will only add global abbreviations. Local
// abbreviations are converted to corresponding global abbreviations,
// so that they can be added as global abbreviations.
//
// The process of compressing is done by reading the input file
// twice. In the first round, the records are read and analyzed,
// generating a set of (global) abbreviations to use to generate the
// compressed output file. Then, the input file is read again, and for
// each record, the best fitting global abbreviation is found (or it
// figures out that leaving the record unabbreviated is best) and
// writes the record out accordingly.
//
// TODO(kschimpf): The current implementation does a trivial solution
// for the first round.  It just converts all abbreviations (local and
// global) into global abbreviations.  Future refinements of this file
// will generate better (and more intelligent) global abbreviations,
// based on the records found on the first read of the bitcode file.
//
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/NaCl/NaClCompress.h"

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Bitcode/NaCl/AbbrevTrieNode.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeHeader.h"
#include "llvm/Bitcode/NaCl/NaClBitstreamWriter.h"
#include "llvm/Bitcode/NaCl/NaClCompressBlockDist.h"
#include "llvm/Bitcode/NaCl/NaClReaderWriter.h"

namespace {

using namespace llvm;

// Generates an error message when outside parsing, and no
// corresponding bit position is known.
static bool Error(const std::string &Err) {
  errs() << Err << "\n";
  return true;
}

// Prints out the abbreviation in readable form to the given Stream.
static void printAbbrev(raw_ostream &Stream, unsigned BlockID,
                        const NaClBitCodeAbbrev *Abbrev) {
  Stream << "Abbrev(block " << BlockID << "): ";
  Abbrev->Print(Stream);
}

/// type map from bitstream abbreviation index to corresponding
/// internal abbreviation index.
typedef std::map<unsigned, unsigned> BitstreamToInternalAbbrevMapType;

/// Defines a mapping from bitstream abbreviation indices, to
/// corresponding internal abbreviation indices.
class AbbrevBitstreamToInternalMap {
public:
  AbbrevBitstreamToInternalMap()
      : NextBitstreamAbbrevIndex(0) {}

  /// Returns the bitstream abbreviaion index that will be associated
  /// with the next internal abbreviation index.
  unsigned getNextBitstreamAbbrevIndex() {
    return NextBitstreamAbbrevIndex;
  }

  /// Changes the next bitstream abbreviation index to the given value.
  void setNextBitstreamAbbrevIndex(unsigned NextIndex) {
    NextBitstreamAbbrevIndex = NextIndex;
  }

  /// Returns true if there is an internal abbreviation index for the
  /// given bitstream abbreviation.
  bool definesBitstreamAbbrevIndex(unsigned Index) {
    return BitstreamToInternalAbbrevMap.find(Index) !=
        BitstreamToInternalAbbrevMap.end();
  }

  /// Returns the internal abbreviation index for the given bitstream
  /// abbreviation index.
  unsigned getInternalAbbrevIndex(unsigned Index) {
    return BitstreamToInternalAbbrevMap.at(Index);
  }

  /// Installs the given internal abbreviation index using the next
  /// available bitstream abbreviation index.
  void installNewBitstreamAbbrevIndex(unsigned InternalIndex) {
    BitstreamToInternalAbbrevMap[NextBitstreamAbbrevIndex++] = InternalIndex;
  }

private:
  // The index of the next bitstream abbreviation to be defined.
  unsigned NextBitstreamAbbrevIndex;
  // Map from bitstream abbreviation index to corresponding internal
  // abbreviation index.
  BitstreamToInternalAbbrevMapType BitstreamToInternalAbbrevMap;
};

/// Defines the list of abbreviations associated with a block.
class BlockAbbrevs {
public:
  // Vector to hold the (ordered) list of abbreviations.
  typedef SmallVector<NaClBitCodeAbbrev*, 32> AbbrevVector;

  BlockAbbrevs(unsigned BlockID)
      : BlockID(BlockID) {
    // backfill internal indices that don't correspond to bitstream
    // application abbreviations, so that added abbreviations have
    // valid abbreviation indices.
    for (unsigned i = 0; i < naclbitc::FIRST_APPLICATION_ABBREV; ++i) {
      // Make all non-application abbreviations look like the default
      // abbreviation.
      NaClBitCodeAbbrev *Abbrev = new NaClBitCodeAbbrev();
      Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::Array));
      Abbrev->Add(NaClBitCodeAbbrevOp(NaClBitCodeAbbrevOp::VBR, 6));
      Abbrevs.push_back(Abbrev);
    }
    GlobalAbbrevBitstreamToInternalMap.
        setNextBitstreamAbbrevIndex(Abbrevs.size());
  }

  ~BlockAbbrevs() {
    for (AbbrevVector::const_iterator
             Iter = Abbrevs.begin(), IterEnd = Abbrevs.end();
         Iter != IterEnd; ++Iter) {
      (*Iter)->dropRef();
    }
    for (AbbrevLookupSizeMap::const_iterator
             Iter = LookupMap.begin(), IterEnd = LookupMap.end();
         Iter != IterEnd; ++Iter) {
      delete Iter->second;
    }
  }

  // Constant used to denote that a given abbreviation is not in the
  // set of abbreviations defined by the block.
  static const int NO_SUCH_ABBREVIATION = -1;

  // Returns the index to the corresponding application abbreviation,
  // if it exists.  Otherwise return NO_SUCH_ABBREVIATION;
  int findAbbreviation(const NaClBitCodeAbbrev *Abbrev) const {
    for (unsigned i = getFirstApplicationAbbreviation();
         i < getNumberAbbreviations(); ++i) {
      if (*Abbrevs[i] == *Abbrev) return i;
    }
    return NO_SUCH_ABBREVIATION;
  }

  /// Adds the given abbreviation to the set of global abbreviations
  /// defined for the block. Guarantees that duplicate abbreviations
  /// are not added to the block. Note: Code takes ownership of
  /// the given abbreviation. Returns true if new abbreviation.
  /// Updates Index to the index where the abbreviation is located
  /// in the set of abbreviations.
  bool addAbbreviation(NaClBitCodeAbbrev *Abbrev, int &Index) {
    Index = findAbbreviation(Abbrev);
    if (Index != NO_SUCH_ABBREVIATION) {
      // Already defined, don't install.
      Abbrev->dropRef();
      return false;
    }

    // New abbreviation. Add.
    Index = Abbrevs.size();
    Abbrevs.push_back(Abbrev);
    return true;
  }

  /// Adds the given abbreviation to the set of global abbreviations
  /// defined for the block. Guarantees that duplicate abbreviations
  /// are not added to the block. Note: Code takes ownership of
  /// the given abbreviation. Returns true if new abbreviation.
  bool addAbbreviation(NaClBitCodeAbbrev *Abbrev) {
    int Index;
    return addAbbreviation(Abbrev, Index);
  }

  /// The block ID associated with the block.
  unsigned getBlockID() const {
    return BlockID;
  }

  /// Returns the index of the frist application abbreviation for the
  /// block.
  unsigned getFirstApplicationAbbreviation() const {
    return naclbitc::FIRST_APPLICATION_ABBREV;
  }

  // Returns the number of abbreviations associated with the block.
  unsigned getNumberAbbreviations() const {
    return Abbrevs.size();
  }

  // Returns true if there is an application abbreviation.
  bool hasApplicationAbbreviations() const {
    return Abbrevs.size() > naclbitc::FIRST_APPLICATION_ABBREV;
  }

  /// Returns the abbreviation associated with the given abbreviation
  /// index.
  NaClBitCodeAbbrev *getIndexedAbbrev(unsigned index) {
    if (index >= Abbrevs.size()) return 0;
    return Abbrevs[index];
  }

  // Builds the corresponding fast lookup map for finding abbreviations
  // that applies to abbreviations in the block
  void buildAbbrevLookupSizeMap(
      const NaClBitcodeCompressor::CompressFlags &Flags) {
    NaClBuildAbbrevLookupMap(getLookupMap(),
                             getAbbrevs(),
                             getFirstApplicationAbbreviation());
    if (Flags.ShowAbbrevLookupTries) printLookupMap(errs());
  }

  AbbrevBitstreamToInternalMap &getGlobalAbbrevBitstreamToInternalMap() {
    return GlobalAbbrevBitstreamToInternalMap;
  }

  AbbrevLookupSizeMap &getLookupMap() {
    return LookupMap;
  }

  // Returns lower level vector of abbreviations.
  const AbbrevVector &getAbbrevs() const {
    return Abbrevs;
  }

  // Returns the abbreviation (index) to use for the corresponding
  // record, based on the abbreviations of this block.  Note: Assumes
  // that buildAbbrevLookupSizeMap has already been called.
  unsigned getRecordAbbrevIndex(const NaClBitcodeRecordData &Record) {
    unsigned BestIndex = 0; // Ignored unless found candidate.
    unsigned BestScore = 0; // Number of bits associated with BestIndex.
    bool FoundCandidate = false;
    NaClBitcodeValues Values(Record);
    size_t Size = Values.size();

    if (Size > NaClValueIndexCutoff) Size = NaClValueIndexCutoff+1;
    AbbrevLookupSizeMap::const_iterator Pos = LookupMap.find(Size);
    if (Pos != LookupMap.end()) {
      if (const AbbrevTrieNode *Node = Pos->second) {
        if (const AbbrevTrieNode *MatchNode =
            Node->MatchRecord(Record)) {
          const std::set<AbbrevIndexPair> &Abbreviations =
              MatchNode->GetAbbreviations();
          for (std::set<AbbrevIndexPair>::const_iterator
                   Iter = Abbreviations.begin(),
                   IterEnd = Abbreviations.end();
               Iter != IterEnd; ++Iter) {
            uint64_t NumBits = 0;
            if (canUseAbbreviation(Values, Iter->second, NumBits)) {
              if (!FoundCandidate || NumBits < BestScore) {
                // Use this as candidate.
                BestIndex = Iter->first;
                BestScore = NumBits;
                FoundCandidate = true;
              }
            }
          }
        }
      }
    }
    if (FoundCandidate && BestScore <= unabbreviatedSize(Record)) {
      return BestIndex;
    }
    return naclbitc::UNABBREV_RECORD;
  }

  // Computes the number of bits that will be generated by the
  // corresponding read record, if no abbreviation is used.
  static uint64_t unabbreviatedSize(const NaClBitcodeRecordData &Record) {
    uint64_t NumBits = matchVBRBits(Record.Code, DefaultVBRBits);
    size_t NumValues = Record.Values.size();
    NumBits += matchVBRBits(NumValues, DefaultVBRBits);
    for (size_t Index = 0; Index < NumValues; ++Index) {
      NumBits += matchVBRBits(Record.Values[Index], DefaultVBRBits);
    }
    return NumBits;
  }

  // Returns true if the given abbreviation can be used to represent the
  // record. Sets NumBits to the number of bits the abbreviation will
  // generate. Note: Value of NumBits is undefined if this function
  // return false.
  static bool canUseAbbreviation(NaClBitcodeValues &Values,
                                 NaClBitCodeAbbrev *Abbrev, uint64_t &NumBits) {
    NumBits = 0;
    unsigned OpIndex = 0;
    unsigned OpIndexEnd = Abbrev->getNumOperandInfos();
    size_t ValueIndex = 0;
    size_t ValueIndexEnd = Values.size();
    while (ValueIndex < ValueIndexEnd && OpIndex < OpIndexEnd) {
      const NaClBitCodeAbbrevOp &Op = Abbrev->getOperandInfo(OpIndex);
      switch (Op.getEncoding()) {
      case NaClBitCodeAbbrevOp::Literal:
        if (canUseSimpleAbbrevOp(Op, Values[ValueIndex], NumBits)) {
          ++ValueIndex;
          ++OpIndex;
          continue;
        } else {
          return false;
        }
      case NaClBitCodeAbbrevOp::Array: {
        assert(OpIndex+2 == OpIndexEnd);
        const NaClBitCodeAbbrevOp &ElmtOp =
            Abbrev->getOperandInfo(OpIndex+1);

        // Add size of array.
        NumBits += matchVBRBits(Values.size()-ValueIndex, DefaultVBRBits);

        // Add size of each field.
        for (; ValueIndex != ValueIndexEnd; ++ValueIndex) {
          uint64_t FieldBits=0;
          if (!canUseSimpleAbbrevOp(ElmtOp, Values[ValueIndex], FieldBits)) {
            return false;
          }
          NumBits += FieldBits;
        }
        return true;
      }
      default: {
        if (canUseSimpleAbbrevOp(Op, Values[ValueIndex], NumBits)) {
          ++ValueIndex;
          ++OpIndex;
          break;
        }
        return false;
      }
      }
    }
    return ValueIndex == ValueIndexEnd && OpIndex == OpIndexEnd;
  }

  // Returns true if the given abbreviation Op defines a single value,
  // and can be applied to the given Val. Adds the number of bits the
  // abbreviation Op will generate to NumBits if Op applies.
  static bool canUseSimpleAbbrevOp(const NaClBitCodeAbbrevOp &Op,
                                   uint64_t Val,
                                   uint64_t &NumBits) {
    switch (Op.getEncoding()) {
    case NaClBitCodeAbbrevOp::Literal:
      return Val == Op.getValue();
    case NaClBitCodeAbbrevOp::Array:
      return false;
    case NaClBitCodeAbbrevOp::Fixed: {
      uint64_t Width = Op.getValue();
      if (!matchFixedBits(Val, Width))
        return false;
      NumBits += Width;
      return true;
    }
    case NaClBitCodeAbbrevOp::VBR:
      if (unsigned Width = matchVBRBits(Val, Op.getValue())) {
        NumBits += Width;
        return true;
      } else {
        return false;
      }
    case NaClBitCodeAbbrevOp::Char6:
      if (!NaClBitCodeAbbrevOp::isChar6(Val)) return false;
      NumBits += 6;
      return true;
    }
    llvm_unreachable("unhandled NaClBitCodeAbbrevOp encoding");
  }

  // Returns true if the given Val can be represented by abbreviation
  // operand Fixed(Width).
  static bool matchFixedBits(uint64_t Val, unsigned Width) {
    // Note: The reader only allows up to 32 bits for fixed values.
    if (Val & Mask32) return false;
    if (Val & ~(~0U >> (32-Width))) return false;
    return true;
  }

  // Returns the number of bits needed to represent Val by abbreviation
  // operand VBR(Width). Note: Returns 0 if Val can't be represented
  // by VBR(Width).
  static unsigned matchVBRBits(uint64_t Val, unsigned Width) {
    if (Width == 0) return 0;
    unsigned NumBits = 0;
    while (1) {
      // values emitted Width-1 bits at a time (plus a continue bit).
      NumBits += Width;
      if ((Val & (1U << (Width-1))) == 0)
        return NumBits;
      Val >>= Width-1;
    }
  }

private:
  // Defines the number of bits used to print VBR array field values.
  static const unsigned DefaultVBRBits = 6;
  // Masks out the top-32 bits of a uint64_t value.
  static const uint64_t Mask32 = 0xFFFFFFFF00000000;
  // The block ID for which abbreviations are being associated.
  unsigned BlockID;
  // The list of abbreviations defined for the block.
  AbbrevVector Abbrevs;
  // The mapping from global bitstream abbreviations to the corresponding
  // block abbreviation index (in Abbrevs).
  AbbrevBitstreamToInternalMap GlobalAbbrevBitstreamToInternalMap;
  // A fast lookup map for finding the abbreviation that applies
  // to a record.
  AbbrevLookupSizeMap LookupMap;

  void printLookupMap(raw_ostream &Stream) const {
    Stream << "------------------------------\n";
    Stream << "Block " << getBlockID() << " abbreviation tries:\n";
    bool IsFirstIteration = true;
    for (AbbrevLookupSizeMap::const_iterator
           Iter = LookupMap.begin(), IterEnd = LookupMap.end();
         Iter != IterEnd; ++Iter) {
      if (IsFirstIteration)
        IsFirstIteration = false;
      else
        Stream << "-----\n";
      if (Iter->second) {
        Stream << "Index " << Iter->first << ":\n";
        Iter->second->Print(Stream, "  ");
      }
    }
    Stream << "------------------------------\n";
  }
};

/// Defines a map from block ID's to the corresponding abbreviation
/// map to use.
typedef DenseMap<unsigned, BlockAbbrevs*> BlockAbbrevsMapType;
typedef std::pair<unsigned, BlockAbbrevs*> BlockAbbrevsMapElmtType;

/// Gets the corresponding block abbreviations for a block ID.
static BlockAbbrevs *getAbbrevs(BlockAbbrevsMapType &AbbrevsMap,
                                unsigned BlockID) {
  BlockAbbrevs *Abbrevs = AbbrevsMap[BlockID];
  if (Abbrevs == nullptr) {
    Abbrevs = new BlockAbbrevs(BlockID);
    AbbrevsMap[BlockID] = Abbrevs;
  }
  return Abbrevs;
}

/// Parses the bitcode file, analyzes it, and generates the
/// corresponding lists of global abbreviations to use in the
/// generated (compressed) bitcode file.
class NaClAnalyzeParser : public NaClBitcodeParser {
  NaClAnalyzeParser(const NaClAnalyzeParser&) = delete;
  void operator=(const NaClAnalyzeParser&) = delete;

public:
  // Creates the analysis parser, which will fill the given
  // BlockAbbrevsMap with appropriate abbreviations, after
  // analyzing the bitcode file defined by Cursor.
  NaClAnalyzeParser(const NaClBitcodeCompressor::CompressFlags &Flags,
                    NaClBitstreamCursor &Cursor,
                    BlockAbbrevsMapType &BlockAbbrevsMap)
      : NaClBitcodeParser(Cursor), Flags(Flags),
        BlockAbbrevsMap(BlockAbbrevsMap),
        BlockDist(&NaClCompressBlockDistElement::Sentinel),
        AbbrevListener(this)
  {
    SetListener(&AbbrevListener);
  }

  virtual ~NaClAnalyzeParser() {}

  virtual bool ParseBlock(unsigned BlockID);

  const NaClBitcodeCompressor::CompressFlags &Flags;

  // Mapping from block ID's to the corresponding list of abbreviations
  // associated with that block.
  BlockAbbrevsMapType &BlockAbbrevsMap;

  // Nested distribution capturing distribution of records in bitcode file.
  NaClBitcodeBlockDist BlockDist;

  // Listener used to get abbreviations as they are read.
  NaClBitcodeParserListener AbbrevListener;
};

class NaClBlockAnalyzeParser : public NaClBitcodeParser {
  NaClBlockAnalyzeParser(const NaClBlockAnalyzeParser&) = delete;
  void operator=(NaClBlockAnalyzeParser&) = delete;

public:
  /// Top-level constructor to build the top-level block with the
  /// given BlockID, and collect data (for compression) in that block.
  NaClBlockAnalyzeParser(unsigned BlockID,
                         NaClAnalyzeParser *Context)
      : NaClBitcodeParser(BlockID, Context), Context(Context) {
    init();
  }

  virtual ~NaClBlockAnalyzeParser() {
    Context->BlockDist.AddBlock(GetBlock());
  }

protected:
  /// Nested constructor to parse a block within a block.  Creates a
  /// block parser to parse a block with the given BlockID, and
  /// collect data (for compression) in that block.
  NaClBlockAnalyzeParser(unsigned BlockID,
                         NaClBlockAnalyzeParser *EnclosingParser)
      : NaClBitcodeParser(BlockID, EnclosingParser),
        Context(EnclosingParser->Context) {
    init();
  }

public:
  virtual bool ParseBlock(unsigned BlockID) {
    NaClBlockAnalyzeParser Parser(BlockID, this);
    return Parser.ParseThisBlock();
  }

  virtual void ProcessRecord() {
    // Before processing the record, we need to rename the abbreviation
    // index, so that we can look it up in the set of block abbreviations
    // being defined.
    if (Record.UsedAnAbbreviation()) {
      unsigned AbbrevIndex = Record.GetAbbreviationIndex();
      if (LocalAbbrevBitstreamToInternalMap.
          definesBitstreamAbbrevIndex(AbbrevIndex)) {
        Record.SetAbbreviationIndex(
            LocalAbbrevBitstreamToInternalMap.
            getInternalAbbrevIndex(AbbrevIndex));
      } else {
        AbbrevBitstreamToInternalMap &GlobalAbbrevBitstreamToInternalMap =
            GlobalBlockAbbrevs->getGlobalAbbrevBitstreamToInternalMap();
        if (GlobalAbbrevBitstreamToInternalMap.
            definesBitstreamAbbrevIndex(AbbrevIndex)) {
          Record.SetAbbreviationIndex(
              GlobalAbbrevBitstreamToInternalMap.
              getInternalAbbrevIndex(AbbrevIndex));
        } else {
          report_fatal_error("Bad abbreviation index in file");
        }
      }
    }

    cast<NaClCompressBlockDistElement>(
        Context->BlockDist.GetElement(Record.GetBlockID()))
        ->GetAbbrevDist().AddRecord(Record);
  }

  virtual void ProcessAbbreviation(unsigned BlockID,
                                   NaClBitCodeAbbrev *Abbrev,
                                   bool IsLocal) {
    int Index;
    addAbbreviation(BlockID, Abbrev->Simplify(), Index);
    if (IsLocal) {
      LocalAbbrevBitstreamToInternalMap.installNewBitstreamAbbrevIndex(Index);
    } else {
      getGlobalAbbrevs(BlockID)->getGlobalAbbrevBitstreamToInternalMap().
          installNewBitstreamAbbrevIndex(Index);
    }
  }

protected:
  // The context (i.e. top-level) parser.
  NaClAnalyzeParser *Context;

  // The global block abbreviations associated with this block.
  BlockAbbrevs *GlobalBlockAbbrevs;

  // The local abbreviations associated with this block.
  AbbrevBitstreamToInternalMap LocalAbbrevBitstreamToInternalMap;

  /// Returns the set of (global) block abbreviations defined for the
  /// given block ID.
  BlockAbbrevs *getGlobalAbbrevs(unsigned BlockID) {
    return getAbbrevs(Context->BlockAbbrevsMap, BlockID);
  }

  // Adds the abbreviation to the list of abbreviations for the given
  // block.
  void addAbbreviation(unsigned BlockID, NaClBitCodeAbbrev *Abbrev,
                       int &Index) {
    // Get block abbreviations.
    BlockAbbrevs* Abbrevs = getGlobalAbbrevs(BlockID);

    // Read abbreviation and add as a global abbreviation.
    if (Abbrevs->addAbbreviation(Abbrev, Index)
        && Context->Flags.TraceGeneratedAbbreviations) {
      printAbbrev(errs(), BlockID, Abbrev);
    }
  }

  /// Finds the index to the corresponding internal block abbreviation
  /// for the given abbreviation.
  int findAbbreviation(unsigned BlockID, const NaClBitCodeAbbrev *Abbrev) {
    return getGlobalAbbrevs(BlockID)->findAbbreviation(Abbrev);
  }

  void init() {
    GlobalBlockAbbrevs = getGlobalAbbrevs(GetBlockID());
    LocalAbbrevBitstreamToInternalMap.setNextBitstreamAbbrevIndex(
        GlobalBlockAbbrevs->
        getGlobalAbbrevBitstreamToInternalMap().getNextBitstreamAbbrevIndex());
  }
};

bool NaClAnalyzeParser::ParseBlock(unsigned BlockID) {
  NaClBlockAnalyzeParser Parser(BlockID, this);
  return Parser.ParseThisBlock();
}

/// Models the unrolling of an abbreviation into its sequence of
/// individual operators. That is, unrolling arrays to match the width
/// of the abbreviation.
///
/// For example, consider the abbreviation [Array(VBR(6))]. If the
/// distribution map has data for records of size 3, and the
/// distribution map suggests that a constant 4 appears as the second
/// element in the record, it is nontrivial to figure out how to
/// encorporate this into this abbrevation. Hence, we unroll the array
/// (3 times) to get [VBR(6), VBR(6), VBR(6), Array(VBR(6))]. To
/// update the second element to match the literal 4, we only need to
/// replace the second element in the unrolled abbreviation resulting
/// in [VBR(6), Lit(4), VBR(6), Array(VBR(6))].
///
/// After we have done appropriate substitutions, we can simplify the
/// unrolled abbreviation by calling method Restore.
///
/// Note: We unroll in the form that best matches the distribution
/// map. Hence, the code is stored as a separate operator. We also
/// keep the array abbreviation op, for untracked elements within the
/// distribution maps.
class UnrolledAbbreviation {
  void operator=(const UnrolledAbbreviation&) = delete;
public:
  /// Unroll the given abbreviation, assuming it has the given size
  /// (as specified in the distribution maps).
  ///
  /// If argument CanBeBigger is true, then we do not assume that we
  /// can remove the trailing array when expanding, because the
  /// actual size of the corresponding record using this abbreviation
  /// may be bigger.
  UnrolledAbbreviation(NaClBitCodeAbbrev *Abbrev, unsigned Size,
                       bool CanBeBigger = false)
      : CodeOp(0) {
    unsigned NextOp = 0;
    unrollAbbrevOp(CodeOp, Abbrev, NextOp);
    --Size;
    for (unsigned i = 0; i < Size; ++i) {
      // Create slot and then fill with appropriate operator.
      AbbrevOps.push_back(CodeOp);
      unrollAbbrevOp(AbbrevOps[i], Abbrev, NextOp);
    }
    if (CanBeBigger) {
      for (; NextOp < Abbrev->getNumOperandInfos(); ++NextOp) {
        MoreOps.push_back(Abbrev->getOperandInfo(NextOp));
      }
    } else if (NextOp < Abbrev->getNumOperandInfos() &&
               !Abbrev->getOperandInfo(NextOp).isArrayOp()) {
      errs() << (Size+1) << ": ";
      Abbrev->Print(errs());
      llvm_unreachable("Malformed abbreviation/size pair");
    }
  }

  explicit UnrolledAbbreviation(const UnrolledAbbreviation &Abbrev)
      : CodeOp(Abbrev.CodeOp),
        AbbrevOps(Abbrev.AbbrevOps),
        MoreOps(Abbrev.MoreOps) {
  }

  /// Prints out the abbreviation modeled by the unrolled
  /// abbreviation.
  void print(raw_ostream &Stream) const {
    NaClBitCodeAbbrev *Abbrev = restore(false);
    Abbrev->Print(Stream);
    Abbrev->dropRef();
  }

  /// Converts the unrolled abbreviation back into a regular
  /// abbreviation. If Simplify is true, we simplify the
  /// unrolled abbreviation as well.
  NaClBitCodeAbbrev *restore(bool Simplify=true) const {
    NaClBitCodeAbbrev *Abbrev = new NaClBitCodeAbbrev();
    Abbrev->Add(CodeOp);
    for (std::vector<NaClBitCodeAbbrevOp>::const_iterator
             Iter = AbbrevOps.begin(), IterEnd = AbbrevOps.end();
         Iter != IterEnd; ++Iter) {
      Abbrev->Add(*Iter);
    }
    for (std::vector<NaClBitCodeAbbrevOp>::const_iterator
             Iter = MoreOps.begin(), IterEnd = MoreOps.end();
         Iter != IterEnd; ++Iter) {
      Abbrev->Add(*Iter);
    }
    if (Simplify) {
      NaClBitCodeAbbrev *SimpAbbrev = Abbrev->Simplify();
      Abbrev->dropRef();
      return SimpAbbrev;
    } else {
      return Abbrev;
    }
  }

  // The abbreviation used for the record code.
  NaClBitCodeAbbrevOp CodeOp;

  // The abbreviations used for each tracked value index.
  std::vector<NaClBitCodeAbbrevOp> AbbrevOps;

private:
  // Any remaining abbreviation operators not part of the unrolling.
  std::vector<NaClBitCodeAbbrevOp> MoreOps;

  // Extracts out the next abbreviation operator from the abbreviation
  // Abbrev, given the index NextOp, and assigns it to AbbrevOp
  void unrollAbbrevOp(NaClBitCodeAbbrevOp &AbbrevOp,
                      NaClBitCodeAbbrev *Abbrev,
                      unsigned &NextOp) {
    assert(NextOp < Abbrev->getNumOperandInfos());
    const NaClBitCodeAbbrevOp &Op = Abbrev->getOperandInfo(NextOp);
    if (Op.isArrayOp()) {
      // Do not advance. The array operator assumes that all remaining
      // elements should match its argument.
      AbbrevOp = Abbrev->getOperandInfo(NextOp+1);
    } else {
      AbbrevOp = Op;
      NextOp++;
    }
  }
};

/// Models a candidate block abbreviation, which is a blockID, and the
/// corresponding abbreviation to be considered for addition.  Note:
/// Constructors and assignment take ownership of the abbreviation.
class CandBlockAbbrev {
public:
  CandBlockAbbrev(unsigned BlockID, NaClBitCodeAbbrev *Abbrev)
      : BlockID(BlockID), Abbrev(Abbrev) {
  }

  CandBlockAbbrev(const CandBlockAbbrev &BlockAbbrev)
      : BlockID(BlockAbbrev.BlockID),
        Abbrev(BlockAbbrev.Abbrev) {
    Abbrev->addRef();
  }

  void operator=(const CandBlockAbbrev &BlockAbbrev) {
    Abbrev->dropRef();
    BlockID = BlockAbbrev.BlockID;
    Abbrev = BlockAbbrev.Abbrev;
    Abbrev->addRef();
  }

  ~CandBlockAbbrev() {
    Abbrev->dropRef();
  }

  /// The block ID of the candidate abbreviation.
  unsigned getBlockID() const {
    return BlockID;
  }

  /// The abbreviation of the candidate abbreviation.
  const NaClBitCodeAbbrev *getAbbrev() const {
    return Abbrev;
  }

  /// orders this against the candidate abbreviation.
  int compare(const CandBlockAbbrev &CandAbbrev) const {
    unsigned diff = BlockID - CandAbbrev.BlockID;
    if (diff) return diff;
    return Abbrev->Compare(*CandAbbrev.Abbrev);
  }

  /// Prints the candidate abbreviation to the given stream.
  void print(raw_ostream &Stream) const {
    printAbbrev(Stream, BlockID, Abbrev);
  }

private:
  // The block the abbreviation applies to.
  unsigned BlockID;
  // The candidate abbreviation.
  NaClBitCodeAbbrev *Abbrev;
};

static inline bool operator<(const CandBlockAbbrev &A1,
                             const CandBlockAbbrev &A2) {
  return A1.compare(A2) < 0;
}

/// Models the set of candidate abbreviations being considered, and
/// the number of abbreviations associated with each candidate
/// Abbreviation.
///
/// Note: Because we may have abbreviation refinements of A->B->C and
/// A->D->C, we need to accumulate instance counts in such cases.
class CandidateAbbrevs {
public:
  // Map from candidate abbreviations to the corresponding number of
  // instances.
  typedef std::map<CandBlockAbbrev, unsigned> AbbrevCountMap;
  typedef AbbrevCountMap::const_iterator const_iterator;

  /// Creates an empty set of candidate abbreviations, to be
  /// (potentially) added to the given set of block abbreviations.
  /// Argument is the (global) block abbreviations map, which is
  /// used to determine if it already exists.
  CandidateAbbrevs(BlockAbbrevsMapType &BlockAbbrevsMap)
      : BlockAbbrevsMap(BlockAbbrevsMap)
  {}

  /// Adds the given (unrolled) abbreviation as a candidate
  /// abbreviation to the given block. NumInstances is the number of
  /// instances expected to use this candidate abbreviation. Returns
  /// true if the corresponding candidate abbreviation is added to this
  /// set of candidate abbreviations.
  bool add(unsigned BlockID,
           UnrolledAbbreviation &UnrolledAbbrev,
           unsigned NumInstances);

  /// Returns the list of candidate abbreviations in this set.
  const AbbrevCountMap &getAbbrevsMap() const {
    return AbbrevsMap;
  }

  /// Prints out the current contents of this set.
  void print(raw_ostream &Stream, const char *Title = "Candidates") const {
    Stream << "-- " << Title << ": \n";
    for (const_iterator Iter = AbbrevsMap.begin(), IterEnd = AbbrevsMap.end();
         Iter != IterEnd; ++Iter) {
      Stream << format("%12u", Iter->second) << ": ";
      Iter->first.print(Stream);
    }
    Stream << "--\n";
  }

private:
  // The set of abbreviations and corresponding number instances.
  AbbrevCountMap AbbrevsMap;

  // The map of (global) abbreviations already associated with each block.
  BlockAbbrevsMapType &BlockAbbrevsMap;
};

bool CandidateAbbrevs::add(unsigned BlockID,
                           UnrolledAbbreviation &UnrolledAbbrev,
                           unsigned NumInstances) {
  // Drop if it corresponds to an existing global abbreviation.
  NaClBitCodeAbbrev *Abbrev = UnrolledAbbrev.restore();
  if (BlockAbbrevs* Abbrevs = BlockAbbrevsMap[BlockID]) {
    if (Abbrevs->findAbbreviation(Abbrev) !=
        BlockAbbrevs::NO_SUCH_ABBREVIATION) {
      Abbrev->dropRef();
      return false;
    }
  }

  CandBlockAbbrev CandAbbrev(BlockID, Abbrev);
  AbbrevCountMap::iterator Pos = AbbrevsMap.find(CandAbbrev);
  if (Pos == AbbrevsMap.end()) {
    AbbrevsMap[CandAbbrev] = NumInstances;
  } else {
    Pos->second += NumInstances;
  }
  return true;
}

// Look for new abbreviations in block BlockID, considering it was
// read with the given abbreviation Abbrev, and considering changing
// the abbreviation opererator for value Index. ValueDist is how
// values at Index are distributed. Any found abbreviations are added
// to the candidate abbreviations CandAbbrevs. Returns true only if we
// have added new candidate abbreviations to CandAbbrevs.
static bool addNewAbbreviations(unsigned BlockID,
                                const UnrolledAbbreviation &Abbrev,
                                unsigned Index,
                                NaClBitcodeValueDist &ValueDist,
                                CandidateAbbrevs &CandAbbrevs) {
  // TODO(kschimpf): Add code to try and find a better encoding for
  // the values, based on the distribution.

  // If this index is already a literal abbreviation, no improvements can
  // be made.
  const NaClBitCodeAbbrevOp Op = Abbrev.AbbrevOps[Index];
  if (Op.isLiteral()) return false;

  // Search based on sorted distribution, which sorts by number of
  // instances.  Start by trying to find possible constants to use.
  const NaClBitcodeDist::Distribution *
      Distribution = ValueDist.GetDistribution();
  for (NaClBitcodeDist::Distribution::const_iterator
           Iter = Distribution->begin(), IterEnd = Distribution->end();
       Iter != IterEnd; ++Iter) {
    NaClValueRangeType Range = GetNaClValueRange(Iter->second);
    if (Range.first != Range.second) continue;  // not a constant.

    // Defines a constant. Try as new candidate range.  In addition,
    // don't try any more constant values, since this is the one with
    // the greatest number of instances.
    NaClBitcodeDistElement *Elmt = ValueDist.at(Range.first);
    UnrolledAbbreviation CandAbbrev(Abbrev);
    CandAbbrev.AbbrevOps[Index] = NaClBitCodeAbbrevOp(Range.first);
    return CandAbbrevs.add(BlockID, CandAbbrev, Elmt->GetNumInstances());
  }
  return false;
}

// Look for new abbreviations in block BlockID, considering it was
// read with the given abbreviation Abbrev. IndexDist is the
// corresponding distribution of value indices associated with the
// abbreviation.  Any found abbreviations are added to the candidate
// abbreviations CandAbbrevs.
static void addNewAbbreviations(
    const NaClBitcodeCompressor::CompressFlags &Flags,
    unsigned BlockID, const UnrolledAbbreviation &Abbrev,
    NaClBitcodeDist &IndexDist, CandidateAbbrevs &CandAbbrevs) {
  // Search based on sorted distribution, which sorts based on heuristic
  // of best index to fix first.
  const NaClBitcodeDist::Distribution *
      IndexDistribution = IndexDist.GetDistribution();
  for (NaClBitcodeDist::Distribution::const_iterator
           IndexIter = IndexDistribution->begin(),
           IndexIterEnd = IndexDistribution->end();
       IndexIter != IndexIterEnd; ++IndexIter) {
    unsigned Index = static_cast<unsigned>(IndexIter->second);
    if (addNewAbbreviations(
            BlockID, Abbrev, Index,
            cast<NaClBitcodeValueIndexDistElement>(IndexDist.at(Index))
            ->GetValueDist(),
            CandAbbrevs)) {
      return;
    }
  }
}

// Look for new abbreviations in block BlockID, considering it was
// read with the given abbreviation Abbrev, and the given record Code.
// SizeDist is the corresponding distribution of sizes associated with
// the abbreviation. Any found abbreviations are added to the
// candidate abbreviations CandAbbrevs.
static void addNewAbbreviations(
    const NaClBitcodeCompressor::CompressFlags &Flags,
    unsigned BlockID, NaClBitCodeAbbrev *Abbrev, unsigned Code,
    NaClBitcodeDist &SizeDist, CandidateAbbrevs &CandAbbrevs) {
  const NaClBitcodeDist::Distribution *
      SizeDistribution = SizeDist.GetDistribution();
  for (NaClBitcodeDist::Distribution::const_iterator
           SizeIter = SizeDistribution->begin(),
           SizeIterEnd = SizeDistribution->end();
       SizeIter != SizeIterEnd; ++SizeIter) {
    unsigned Size = static_cast<unsigned>(SizeIter->second);
    UnrolledAbbreviation UnrolledAbbrev(Abbrev, Size+1 /* Add code! */,
                                        Size >= NaClValueIndexCutoff);
    if (!UnrolledAbbrev.CodeOp.isLiteral()) {
      // Try making the code a literal.
      UnrolledAbbreviation CandAbbrev(UnrolledAbbrev);
      CandAbbrev.CodeOp = NaClBitCodeAbbrevOp(Code);
      CandAbbrevs.add(BlockID, CandAbbrev,
                      SizeDist.at(Size)->GetNumInstances());
    }
    // Now process value indices to find candidate abbreviations.
    addNewAbbreviations(Flags, BlockID, UnrolledAbbrev,
        cast<NaClBitcodeSizeDistElement>(SizeDist.at(Size))
        ->GetValueIndexDist(),
        CandAbbrevs);
  }
}

// Look for new abbreviations in block BlockID. Abbrevs is the map of
// read (globally defined) abbreviations associated with the
// BlockID. AbbrevDist is the distribution map of abbreviations
// associated with BlockID. Any found abbreviations are added to the
// candidate abbreviations CandAbbrevs.
static void addNewAbbreviations(
    const NaClBitcodeCompressor::CompressFlags &Flags, unsigned BlockID,
    BlockAbbrevs *Abbrevs, NaClBitcodeDist &AbbrevDist,
    CandidateAbbrevs &CandAbbrevs) {
  const NaClBitcodeDist::Distribution *
      AbbrevDistribution = AbbrevDist.GetDistribution();
  for (NaClBitcodeDist::Distribution::const_iterator
           AbbrevIter = AbbrevDistribution->begin(),
           AbbrevIterEnd = AbbrevDistribution->end();
       AbbrevIter != AbbrevIterEnd; ++AbbrevIter) {
    NaClBitcodeDistValue AbbrevIndex = AbbrevIter->second;
    NaClBitCodeAbbrev *Abbrev = Abbrevs->getIndexedAbbrev(AbbrevIndex);
    NaClBitcodeAbbrevDistElement *AbbrevElmt =
        cast<NaClBitcodeAbbrevDistElement>(AbbrevDist.at(AbbrevIndex));
    NaClBitcodeDist &CodeDist = AbbrevElmt->GetCodeDist();

    const NaClBitcodeDist::Distribution *
        CodeDistribution = CodeDist.GetDistribution();
    for (NaClBitcodeDist::Distribution::const_iterator
             CodeIter = CodeDistribution->begin(),
             CodeIterEnd = CodeDistribution->end();
         CodeIter != CodeIterEnd; ++CodeIter) {
      unsigned Code = static_cast<unsigned>(CodeIter->second);
      addNewAbbreviations(
          Flags, BlockID, Abbrev, Code,
          cast<NaClCompressCodeDistElement>(CodeDist.at(CodeIter->second))
          ->GetSizeDist(),
          CandAbbrevs);
    }
  }
}

typedef std::pair<unsigned, CandBlockAbbrev> CountedAbbrevType;

// Look for new abbreviations in the given block distribution map
// BlockDist.  BlockAbbrevsMap contains the set of read global
// abbreviations. Adds found candidate abbreviations to the set of
// known global abbreviations.
static void addNewAbbreviations(
    const NaClBitcodeCompressor::CompressFlags &Flags,
    NaClBitcodeBlockDist &BlockDist, BlockAbbrevsMapType &BlockAbbrevsMap) {
  CandidateAbbrevs CandAbbrevs(BlockAbbrevsMap);
  // Start by collecting candidate abbreviations.
  const NaClBitcodeDist::Distribution *
      Distribution = BlockDist.GetDistribution();
  for (NaClBitcodeDist::Distribution::const_iterator
           Iter = Distribution->begin(), IterEnd = Distribution->end();
       Iter != IterEnd; ++Iter) {
    unsigned BlockID = static_cast<unsigned>(Iter->second);
    addNewAbbreviations(
        Flags, BlockID, BlockAbbrevsMap[BlockID],
        cast<NaClCompressBlockDistElement>(BlockDist.at(BlockID))
        ->GetAbbrevDist(),
        CandAbbrevs);
  }
  // Install candidate abbreviations.
  //
  // Sort the candidate abbreviations by number of instances, so that
  // if multiple abbreviations apply, the one with the largest number
  // of instances will be chosen when compressing a file.
  //
  // For example, we may have just generated two abbreviations. The
  // first has replaced the 3rd element with the constant 4 while the
  // second replaced the 4th element with the constant 5. The first
  // abbreviation can apply to 300 records while the second can apply
  // to 1000 records.  Assuming that both abbreviations shrink the
  // record by the same number of bits, we clearly want the tool to
  // choose the second abbreviation when selecting the abbreviation
  // index to choose (via method getRecordAbbrevIndex).
  //
  // Selecting the second is important in that abbreviation are
  // refined by successive calls to this tool. We do not want to
  // restrict downstream refinements prematurely.  Hence, we want the
  // tool to choose the abbreviation with the most candidates first.
  //
  // Since method getRecordAbbrevIndex chooses the first abbreviation
  // that generates the least number of bits, we clearly want to make
  // sure that the second abbreviation occurs before the first.
  std::vector<CountedAbbrevType> Candidates;
  for (CandidateAbbrevs::const_iterator
           Iter = CandAbbrevs.getAbbrevsMap().begin(),
           IterEnd = CandAbbrevs.getAbbrevsMap().end();
       Iter != IterEnd; ++Iter) {
    Candidates.push_back(CountedAbbrevType(Iter->second,Iter->first));
  }
  std::sort(Candidates.begin(), Candidates.end());
  std::vector<CountedAbbrevType>::const_reverse_iterator
      Iter = Candidates.rbegin(), IterEnd = Candidates.rend();
  if (Iter == IterEnd) return;

  if (Flags.TraceGeneratedAbbreviations) {
    errs() << "-- New abbrevations:\n";
  }
  unsigned Min = (Iter->first >> 2);
  for (; Iter != IterEnd; ++Iter) {
    if (Iter->first < Min) break;
    unsigned BlockID = Iter->second.getBlockID();
    BlockAbbrevs *Abbrevs = BlockAbbrevsMap[BlockID];
    NaClBitCodeAbbrev *Abbrev = Iter->second.getAbbrev()->Copy();
    if (Flags.TraceGeneratedAbbreviations) {
      errs() <<format("%12u", Iter->first) << ": ";
      printAbbrev(errs(), BlockID, Abbrev);
    }
    Abbrevs->addAbbreviation(Abbrev);
  }
  if (Flags.TraceGeneratedAbbreviations) {
    errs() << "--\n";
  }
}

// Walks the block distribution (BlockDist), sorting entries based
// on the distribution of blocks and abbreviations, and then
// prints out the frequency of each abbreviation used.
static void displayAbbreviationFrequencies(
    raw_ostream &Output,
    const NaClBitcodeBlockDist &BlockDist,
    const BlockAbbrevsMapType &BlockAbbrevsMap) {
  const NaClBitcodeDist::Distribution *BlockDistribution =
      BlockDist.GetDistribution();
  for (NaClBitcodeDist::Distribution::const_iterator
           BlockIter = BlockDistribution->begin(),
           BlockIterEnd = BlockDistribution->end();
       BlockIter != BlockIterEnd; ++BlockIter) {
    unsigned BlockID = static_cast<unsigned>(BlockIter->second);
    BlockAbbrevsMapType::const_iterator BlockPos = BlockAbbrevsMap.find(BlockID);
    if (BlockPos == BlockAbbrevsMap.end()) continue;
    Output << "Block " << BlockID << "\n";
    if (NaClCompressBlockDistElement *BlockElement =
        dyn_cast<NaClCompressBlockDistElement>(BlockDist.at(BlockID))) {
      NaClBitcodeDist &AbbrevDist = BlockElement->GetAbbrevDist();
      const NaClBitcodeDist::Distribution *AbbrevDistribution =
          AbbrevDist.GetDistribution();
      unsigned Total = AbbrevDist.GetTotal();
      for (NaClBitcodeDist::Distribution::const_iterator
               AbbrevIter = AbbrevDistribution->begin(),
               AbbrevIterEnd = AbbrevDistribution->end();
           AbbrevIter != AbbrevIterEnd; ++AbbrevIter) {
        unsigned Index = static_cast<unsigned>(AbbrevIter->second);
        unsigned Count = AbbrevDist.at(Index)->GetNumInstances();
        Output << format("%8u (%6.2f%%): ", Count,
                         (double) Count/Total*100.0);
        BlockPos->second->getIndexedAbbrev(Index)->Print(Output);
      }
    }
    Output << '\n';
  }
}

// Read in bitcode, analyze data, and figure out set of abbreviations
// to use, from memory buffer MemBuf containing the input bitcode file.
// If ShowAbbreviationFrequencies or Flag.ShowValueDistributions are
// defined, the corresponding results is printed to Output.
static bool analyzeBitcode(
    const NaClBitcodeCompressor::CompressFlags &Flags,
    MemoryBuffer *MemBuf,
    raw_ostream &Output,
    BlockAbbrevsMapType &BlockAbbrevsMap) {
  // TODO(kschimpf): The current code only extracts abbreviations
  // defined in the bitcode file. This code needs to be updated to
  // collect data distributions and figure out better (global)
  // abbreviations to use.

  const unsigned char *BufPtr = (const unsigned char *)MemBuf->getBufferStart();
  const unsigned char *EndBufPtr = BufPtr+MemBuf->getBufferSize();

  // First read header and verify it is good.
  NaClBitcodeHeader Header;
  if (Header.Read(BufPtr, EndBufPtr))
    return Error("Invalid PNaCl bitcode header");
  if (!Header.IsSupported()) {
    errs() << Header.Unsupported();
    if (!Header.IsReadable())
      return Error("Invalid PNaCl bitcode header");
  }

  // Create a bitstream reader to read the bitcode file.
  NaClBitstreamReader StreamFile(BufPtr, EndBufPtr, Header);
  NaClBitstreamCursor Stream(StreamFile);

  // Parse the the bitcode file.
  NaClAnalyzeParser Parser(Flags, Stream, BlockAbbrevsMap);
  while (!Stream.AtEndOfStream()) {
    if (Parser.Parse()) return true;
  }

  if (Flags.ShowAbbreviationFrequencies)
    displayAbbreviationFrequencies(Output, Parser.BlockDist, BlockAbbrevsMap);
  if (Flags.ShowValueDistributions)
    Parser.BlockDist.Print(Output);

  addNewAbbreviations(Flags, Parser.BlockDist, BlockAbbrevsMap);
  return false;
}

/// Models a queue of selected abbreviation indices, for each record
/// in all instances of a given block. Elements are added in the order
/// they appear in the bitcode file.
///
/// The goal is to remove abbreviations that are not really used, from
/// the list of candidate abbrevations, reducing the number of
/// abbreviations needed. This is important because as the number of
/// abbreviations increase, the number of bits needed to store the
/// abbreviations also increase. By removing unnecessary
/// abbreviations, this improves the ability of this executable to
/// compress the bitcode file.
class SelectedAbbrevsQueue {
  SelectedAbbrevsQueue(const SelectedAbbrevsQueue &) = delete;
  SelectedAbbrevsQueue &operator=(const SelectedAbbrevsQueue &) = delete;

  // The minimum number of times an abbreviation must be used in the
  // compressed version of the bitcode file, if it is going to be used
  // at all.
  static const uint32_t MinUsageCount = 5;

public:
  SelectedAbbrevsQueue() : AbbrevsIndexQueueFront(0) {}

  /// Adds the given selected abbreviation index to the end of the
  /// queue.
  void addIndex(unsigned Index) { AbbrevsIndexQueue.push_back(Index); }

  /// Removes the next selected abbreviation index from the
  /// queue.
  unsigned removeIndex() {
    assert(AbbrevsIndexQueueFront < AbbrevsIndexQueue.size());
    return AbbrevsIndexQueue[AbbrevsIndexQueueFront++];
  }

  /// Takes the abbreviation indices on the queue, determines what
  /// subset of abbreviations should be kept, and puts them on the
  /// list of abbreviations defined by getKeptAbbrevs. Updates the
  /// abbreviation idices on the queue to match the corresponding kept
  /// abbreviations.
  ///
  /// Should be called after the last call to AddIndex, and before
  /// calling removeIndex.
  void installFrequentlyUsedAbbrevs(BlockAbbrevs *Abbrevs);

  /// Return the list of kept abbreviations, for the corresponding
  /// block, in the order they should be defined.
  const std::vector<NaClBitCodeAbbrev *> &getKeptAbbrevs() const {
    return KeptAbbrevs;
  }

  /// Returns the maximum abbreviation index used by the kept
  /// abbreviations.
  unsigned getMaxKeptAbbrevIndex() const {
    return KeptAbbrevs.size() + naclbitc::DEFAULT_MAX_ABBREV;
  }

protected:
  // Defines a queue of abbreviations indices using a
  // vector. AbbrevsIndexQueueFront is used to point to the front of
  // the queue.
  std::vector<unsigned> AbbrevsIndexQueue;
  unsigned AbbrevsIndexQueueFront;
  // The list of abbreviations that should be defined for the block,
  // in the order they should be defined.
  std::vector<NaClBitCodeAbbrev *> KeptAbbrevs;
};

void SelectedAbbrevsQueue::installFrequentlyUsedAbbrevs(BlockAbbrevs *Abbrevs) {
  // Start by collecting usage counts for each abbreviation.
  assert(AbbrevsIndexQueueFront == 0);
  assert(KeptAbbrevs.empty());
  std::map<unsigned, uint32_t> UsageMap;
  for (unsigned Index : AbbrevsIndexQueue) {
    if (Index != naclbitc::UNABBREV_RECORD)
      ++UsageMap[Index];
  }

  // Install results
  std::map<unsigned, unsigned> KeepIndexMap;
  for (const std::pair<unsigned, uint32_t> &Pair : UsageMap) {
    if (Pair.second >= MinUsageCount) {
      KeepIndexMap[Pair.first] =
          KeptAbbrevs.size() + naclbitc::FIRST_APPLICATION_ABBREV;
      KeptAbbrevs.push_back(Abbrevs->getIndexedAbbrev(Pair.first));
    }
  }

  // Update the queue of selected abbreviation indices to match kept
  // abbreviations.
  for (unsigned &AbbrevIndex : AbbrevsIndexQueue) {
    std::map<unsigned, unsigned>::const_iterator NewPos =
        KeepIndexMap.find(AbbrevIndex);
    AbbrevIndex = NewPos == KeepIndexMap.end() ? naclbitc::UNABBREV_RECORD
                                               : NewPos->second;
  }
}

/// Models the kept queue of abbreviations associated with each block ID.
typedef std::map<unsigned, SelectedAbbrevsQueue *> BlockAbbrevsQueueMap;
typedef std::pair<unsigned, SelectedAbbrevsQueue *> BlockAbbrevsQueueMapElmt;

/// Installs frequently used abbreviations for each of the blocks
/// defined in AbbrevsQueueMap, based on the abbreviations in the
/// corresponding AbbrevsMap
static void
installFrequentlyUsedAbbrevs(BlockAbbrevsMapType &AbbrevsMap,
                             BlockAbbrevsQueueMap &AbbrevsQueueMap) {
  for (const BlockAbbrevsQueueMapElmt &Pair : AbbrevsQueueMap) {
    if (SelectedAbbrevsQueue *SelectedAbbrevs = Pair.second)
      SelectedAbbrevs->installFrequentlyUsedAbbrevs(AbbrevsMap[Pair.first]);
  }
}

/// Top level parser to queue selected abbreviation indices (within
/// the bitcode file), so that we can remove unused abbreviations from
/// the collected list of abbreviations before generating the final,
/// compressed bitcode file.
class NaClAssignAbbrevsParser : public NaClBitcodeParser {
public:
  NaClAssignAbbrevsParser(NaClBitstreamCursor &Cursor,
                          BlockAbbrevsMapType &AbbrevsMap,
                          BlockAbbrevsQueueMap &AbbrevsQueueMap)
      : NaClBitcodeParser(Cursor), AbbrevsMap(AbbrevsMap),
        AbbrevsQueueMap(AbbrevsQueueMap) {}

  ~NaClAssignAbbrevsParser() final {}

  bool ParseBlock(unsigned BlockID) final;

  /// The abbreviations to use for the compressed bitcode.
  BlockAbbrevsMapType &AbbrevsMap;

  /// The corresponding selected abbreviation indices to for each
  /// block.
  BlockAbbrevsQueueMap &AbbrevsQueueMap;
};

/// Block parser to queue abbreviation indices used by the records in
/// the block.
class NaClAssignAbbrevsBlockParser : public NaClBitcodeParser {
public:
  NaClAssignAbbrevsBlockParser(unsigned BlockID,
                               NaClAssignAbbrevsParser *Context)
      : NaClBitcodeParser(BlockID, Context), BlockID(BlockID),
        Context(Context) {
    init();
  }

  ~NaClAssignAbbrevsBlockParser() final {}

protected:
  unsigned BlockID;
  NaClAssignAbbrevsParser *Context;
  // Cached abbreviations defined for this block,.
  BlockAbbrevs *Abbreviations;
  // Cached abbreviations queue to add abbreviation indices to.
  SelectedAbbrevsQueue *Queue;

  NaClAssignAbbrevsBlockParser(unsigned BlockID,
                               NaClAssignAbbrevsBlockParser *EnclosingParser)
      : NaClBitcodeParser(BlockID, EnclosingParser), BlockID(BlockID),
        Context(EnclosingParser->Context) {
    init();
  }

  void init() {
    Abbreviations = getAbbrevs(Context->AbbrevsMap, BlockID);
    Queue = Context->AbbrevsQueueMap[BlockID];
    if (Queue == nullptr) {
      Queue = new SelectedAbbrevsQueue();
      Context->AbbrevsQueueMap[BlockID] = Queue;
    }
  }

  bool ParseBlock(unsigned BlockID) final {
    NaClAssignAbbrevsBlockParser Parser(BlockID, this);
    return Parser.ParseThisBlock();
  }

  void ProcessRecord() final {
    // Find best fitting abbreviation to use, and save.
    Queue->addIndex(
        Abbreviations->getRecordAbbrevIndex(Record.GetRecordData()));
  }
};

bool NaClAssignAbbrevsParser::ParseBlock(unsigned BlockID) {
  NaClAssignAbbrevsBlockParser Parser(BlockID, this);
  return Parser.ParseThisBlock();
}

// Reads the bitcode in MemBuf, using the abbreviations in AbbrevsMap,
// and queues the selected abbrevations for each record into
// AbbrevsQueueMap.
static bool chooseAbbrevs(MemoryBuffer *MemBuf, BlockAbbrevsMapType &AbbrevsMap,
                          BlockAbbrevsQueueMap &AbbrevsQueueMap) {
  const unsigned char *BufPtr = (const unsigned char *)MemBuf->getBufferStart();
  const unsigned char *EndBufPtr = BufPtr + MemBuf->getBufferSize();

  // Read header. No verification is needed since AnalyzeBitcode has
  // already checked it.
  NaClBitcodeHeader Header;
  if (Header.Read(BufPtr, EndBufPtr))
    return Error("Invalid PNaCl bitcode header");

  // Create the bitcode reader.
  NaClBitstreamReader StreamFile(BufPtr, EndBufPtr, Header);
  NaClBitstreamCursor Stream(StreamFile);

  // Set up the parser.
  NaClAssignAbbrevsParser Parser(Stream, AbbrevsMap, AbbrevsQueueMap);

  // Parse the bitcode and choose abbreviations for records.
  while (!Stream.AtEndOfStream()) {
    if (Parser.Parse()) {
      installFrequentlyUsedAbbrevs(AbbrevsMap, AbbrevsQueueMap);
      return true;
    }
  }
  installFrequentlyUsedAbbrevs(AbbrevsMap, AbbrevsQueueMap);
  return false;
}

/// Parses the input bitcode file and generates the corresponding
/// compressed bitcode file, by replacing abbreviations in the input
/// file with the corresponding abbreviations defined in
/// BlockAbbrevsMap using the selected abbreviations in AbbrevsQueueMap.
class NaClBitcodeCopyParser : public NaClBitcodeParser {
public:
  // Top-level constructor to build the appropriate block parser
  // using the given BlockAbbrevsMap to define abbreviations.
  NaClBitcodeCopyParser(const NaClBitcodeCompressor::CompressFlags &Flags,
                        NaClBitstreamCursor &Cursor,
                        BlockAbbrevsMapType &BlockAbbrevsMap,
                        BlockAbbrevsQueueMap &AbbrevsQueueMap,
                        NaClBitstreamWriter &Writer)
      : NaClBitcodeParser(Cursor), Flags(Flags),
        BlockAbbrevsMap(BlockAbbrevsMap), AbbrevsQueueMap(AbbrevsQueueMap),
        Writer(Writer) {}

  virtual ~NaClBitcodeCopyParser() {}

  bool ParseBlock(unsigned BlockID) final;

  const NaClBitcodeCompressor::CompressFlags &Flags;

  // The abbreviations to use for the copied bitcode.
  BlockAbbrevsMapType &BlockAbbrevsMap;

  // Map defining the selected abbreviations for each block.
  BlockAbbrevsQueueMap &AbbrevsQueueMap;

  // The bitstream to copy the compressed bitcode into.
  NaClBitstreamWriter &Writer;
};

class NaClBlockCopyParser : public NaClBitcodeParser {
public:
  // Top-level constructor to build the appropriate block parser.
  NaClBlockCopyParser(unsigned BlockID, NaClBitcodeCopyParser *Context)
      : NaClBitcodeParser(BlockID, Context), Context(Context),
        BlockAbbreviations(nullptr), SelectedAbbrevs(nullptr) {
    init();
  }

  virtual ~NaClBlockCopyParser() {}

protected:
  // The context defining state associated with the block parser.
  NaClBitcodeCopyParser *Context;

  // The block abbreviations defined for this block (initialized by
  // EnterBlock).
  BlockAbbrevs *BlockAbbreviations;

  // The corresonding selected abbreviations.
  SelectedAbbrevsQueue *SelectedAbbrevs;

  /// Constructor to parse nested blocks.  Creates a block parser to
  /// parse in a block with the given BlockID, and write the block
  /// back out using the abbreviations in BlockAbbrevsMap.
  NaClBlockCopyParser(unsigned BlockID,
                      NaClBlockCopyParser *EnclosingParser)
      : NaClBitcodeParser(BlockID, EnclosingParser),
        Context(EnclosingParser->Context), BlockAbbreviations(nullptr),
        SelectedAbbrevs(nullptr) {
    init();
  }

  void init() {
    unsigned BlockID = GetBlockID();
    BlockAbbreviations = getGlobalAbbrevs(BlockID);
    SelectedAbbrevs = Context->AbbrevsQueueMap[BlockID];
    assert(SelectedAbbrevs);
  }

  /// Returns the set of (global) block abbreviations defined for the
  /// given block ID.
  BlockAbbrevs *getGlobalAbbrevs(unsigned BlockID) {
    return getAbbrevs(Context->BlockAbbrevsMap, BlockID);
  }

  virtual bool ParseBlock(unsigned BlockID) {
    NaClBlockCopyParser Parser(BlockID, this);
    return Parser.ParseThisBlock();
  }

  virtual void EnterBlock(unsigned NumWords) {
    // Enter the subblock.
    NaClBitcodeSelectorAbbrev Selector(
        SelectedAbbrevs->getMaxKeptAbbrevIndex());
    if (Context->Flags.RemoveAbbreviations)
      Selector = NaClBitcodeSelectorAbbrev();

    unsigned BlockID = GetBlockID();
    Context->Writer.EnterSubblock(BlockID, Selector);

    if (BlockID != naclbitc::MODULE_BLOCK_ID
        || Context->Flags.RemoveAbbreviations)
      return;

    // To keep things simple, we dump all abbreviations immediately
    // inside the module block. Start by dumping module abbreviations
    // as local abbreviations.
    for (auto Abbrev : SelectedAbbrevs->getKeptAbbrevs()) {
      Context->Writer.EmitAbbrev(Abbrev->Copy());
    }

    // Insert the block info block, if needed, so that nested blocks
    // will have defined abbreviations.
    bool HasNonmoduleAbbrevs = false;
    for (const BlockAbbrevsQueueMapElmt &Pair : Context->AbbrevsQueueMap) {
      if (Pair.second->getKeptAbbrevs().empty())
        continue;
      HasNonmoduleAbbrevs = true;
      break;
    }
    if (!HasNonmoduleAbbrevs)
      return;

    Context->Writer.EnterBlockInfoBlock();
    for (const BlockAbbrevsMapElmtType &Pair : Context->BlockAbbrevsMap) {
      unsigned BlockID = Pair.first;
      // Don't emit module abbreviations, since they have been
      // emitted as local abbreviations.
      if (BlockID == naclbitc::MODULE_BLOCK_ID)
        continue;
      BlockAbbrevs *Abbrevs = Pair.second;
      if (Abbrevs == nullptr)
        continue;
      if (SelectedAbbrevsQueue *SelectedAbbrevs =
              Context->AbbrevsQueueMap[BlockID]) {
        for (auto Abbrev : SelectedAbbrevs->getKeptAbbrevs()) {
          Context->Writer.EmitBlockInfoAbbrev(BlockID, Abbrev->Copy());
        }
      }
    }
    Context->Writer.ExitBlock();
  }

  virtual void ExitBlock() {
    Context->Writer.ExitBlock();
  }

  virtual void ProcessRecord() {
    const NaClBitcodeRecord::RecordVector &Values = Record.GetValues();
    if (Context->Flags.RemoveAbbreviations) {
      Context->Writer.EmitRecord(Record.GetCode(), Values, 0);
      return;
    }
    // Find best fitting abbreviation to use, and print out the record
    // using that abbreviation.
    unsigned AbbrevIndex = SelectedAbbrevs->removeIndex();
    if (AbbrevIndex == naclbitc::UNABBREV_RECORD) {
      Context->Writer.EmitRecord(Record.GetCode(), Values, 0);
    } else {
      Context->Writer.EmitRecord(Record.GetCode(), Values, AbbrevIndex);
    }
  }
};

bool NaClBitcodeCopyParser::ParseBlock(unsigned BlockID) {
  NaClBlockCopyParser Parser(BlockID, this);
  return Parser.ParseThisBlock();
}

// Read in bitcode, and write it back out using the abbreviations in
// BlockAbbrevsMap, from memory buffer MemBuf containing the input
// bitcode file. The bitcode is copied to Output.
static bool copyBitcode(const NaClBitcodeCompressor::CompressFlags &Flags,
                        MemoryBuffer *MemBuf, raw_ostream &Output,
                        BlockAbbrevsMapType &BlockAbbrevsMap,
                        BlockAbbrevsQueueMap &AbbrevsQueueMap) {

  const unsigned char *BufPtr = (const unsigned char *)MemBuf->getBufferStart();
  const unsigned char *EndBufPtr = BufPtr + MemBuf->getBufferSize();

  // Read header. No verification is needed since AnalyzeBitcode has
  // already checked it.
  NaClBitcodeHeader Header;
  if (Header.Read(BufPtr, EndBufPtr))
    return Error("Invalid PNaCl bitcode header");

  // Create the bitcode reader.
  NaClBitstreamReader StreamFile(BufPtr, EndBufPtr, Header);
  NaClBitstreamCursor Stream(StreamFile);

  // Create the bitcode writer.
  SmallVector<char, 0> OutputBuffer;
  OutputBuffer.reserve(256 * 1024);
  NaClBitstreamWriter StreamWriter(OutputBuffer);

  // Emit the file header.
  NaClWriteHeader(Header, StreamWriter);

  // Set up the parser.
  NaClBitcodeCopyParser Parser(Flags, Stream, BlockAbbrevsMap, AbbrevsQueueMap,
                               StreamWriter);

  // Parse the bitcode and copy.
  while (!Stream.AtEndOfStream()) {
    if (Parser.Parse())
      return true;
  }

  // Write out the copied results.
  Output.write((char *)&OutputBuffer.front(), OutputBuffer.size());
  return false;
}

// Build fast lookup abbreviation maps for each of the abbreviation blocks
// defined in AbbrevsMap.
static void buildAbbrevLookupMaps(
    const NaClBitcodeCompressor::CompressFlags &Flags,
    BlockAbbrevsMapType &AbbrevsMap) {
  for (BlockAbbrevsMapType::const_iterator
           Iter = AbbrevsMap.begin(),
           IterEnd = AbbrevsMap.end();
       Iter != IterEnd; ++Iter) {
    Iter->second->buildAbbrevLookupSizeMap(Flags);
  }
}

}  // namespace

bool NaClBitcodeCompressor::analyze(MemoryBuffer *MemBuf, raw_ostream &Output) {
  BlockAbbrevsMapType BlockAbbrevsMap;
  return !analyzeBitcode(Flags, MemBuf, Output, BlockAbbrevsMap);
}

bool NaClBitcodeCompressor::compress(MemoryBuffer *MemBuf,
                                     raw_ostream &BitcodeOutput,
                                     raw_ostream &ShowOutput) {
  BlockAbbrevsMapType BlockAbbrevsMap;
  if (analyzeBitcode(Flags, MemBuf, ShowOutput, BlockAbbrevsMap))
    return false;
  buildAbbrevLookupMaps(Flags, BlockAbbrevsMap);
  BlockAbbrevsQueueMap AbbrevsQueueMap;
  bool Result = true;
  if (chooseAbbrevs(MemBuf, BlockAbbrevsMap, AbbrevsQueueMap))
    Result = false;
  else if (copyBitcode(Flags, MemBuf, BitcodeOutput, BlockAbbrevsMap,
                       AbbrevsQueueMap))
    Result = false;
  DeleteContainerSeconds(AbbrevsQueueMap);
  return Result;
}
