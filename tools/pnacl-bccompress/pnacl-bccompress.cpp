//===-- pnacl-bccompress.cpp - Bitcode (abbrev) compression ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This tool may be invoked in the following manner:
//  pnacl-bccompress [options] bcin.pexe -o bcout.pexe
//      - Read frozen PNaCl bitcode from the bcin.pexe and introduce
//        abbreviations to compress it into bcout.pexe.
//
//  Options:
//      --help      - Output information about command line switches
//
// This tool analyzes the data in bcin.pexe, and determines what
// abbreviations can be added to compress the bitcode file. The result
// is written to bcout.pexe.
//
// A bitcode file has two types of abbreviations. The first are Global
// abbreviations that apply to all instances of a particular type of
// block.  These abbreviations appear in the BlockInfo block of the
// bitcode file.
//
// The second type of abbreviations are local to a particular instance
// of a block.
//
// In pnacl-bccompress, for simplicity, we will only add global
// abbreviations. Local abbreviations are converted to corresponding
// global abbreviations, so that they can be added as global
// abbreviations.
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

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/Bitcode/NaCl/AbbrevTrieNode.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeHeader.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeParser.h"
#include "llvm/Bitcode/NaCl/NaClBitstreamReader.h"
#include "llvm/Bitcode/NaCl/NaClBitstreamWriter.h"
#include "llvm/Bitcode/NaCl/NaClCompressBlockDist.h"
#include "llvm/Bitcode/NaCl/NaClReaderWriter.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/raw_ostream.h"
#include <set>
#include <map>
#include <system_error>

namespace {

using namespace llvm;

static cl::opt<bool>
TraceGeneratedAbbreviations(
    "abbreviations",
    cl::desc("Trace abbreviations added to compressed file"),
    cl::init(false));

static cl::opt<std::string>
  InputFilename(cl::Positional, cl::desc("<input bitcode>"), cl::init("-"));


static cl::opt<std::string>
OutputFilename("o", cl::desc("Specify output filename"),
               cl::value_desc("filename"), cl::init("-"));

static cl::opt<bool>
ShowValueDistributions(
    "show-distributions",
    cl::desc("Show collected value distributions in bitcode records. "
             "Turns off compression."),
    cl::init(false));

static cl::opt<bool>
ShowAbbrevLookupTries(
    "show-lookup-tries",
    cl::desc("Show lookup tries used to minimize search for \n"
             "matching abbreviations."),
    cl::init(false));

static cl::opt<bool>
ShowAbbreviationFrequencies(
    "show-abbreviation-frequencies",
    cl::desc("Show how often each abbreviation is used. "
             "Turns off compression."),
    cl::init(false));

// Note: When this flag is true, we still generate new abbreviations,
// because we don't want to add the complexity of turning it off.
// Rather, we simply make sure abbreviations are ignored when writing
// out the final copy.
static cl::opt<bool>
RemoveAbbreviations(
    "remove-abbreviations",
    cl::desc("Remove abbreviations from input bitcode file."),
    cl::init(false));

/// Error - All bitcode analysis errors go through this function,
/// making this a good place to breakpoint if debugging.
static bool Error(const std::string &Err) {
  errs() << Err << "\n";
  return true;
}

// Prints out the abbreviation in readable form to the given Stream.
static void PrintAbbrev(raw_ostream &Stream,
                        unsigned BlockID, const NaClBitCodeAbbrev *Abbrev) {
  Stream << "Abbrev(block " << BlockID << "): ";
  Abbrev->Print(Stream);
}

// Reads the input file into the given buffer.
static bool ReadAndBuffer(std::unique_ptr<MemoryBuffer> &MemBuf) {
  ErrorOr<std::unique_ptr<MemoryBuffer>> ErrOrFile =
      MemoryBuffer::getFileOrSTDIN(InputFilename);
  if (std::error_code EC = ErrOrFile.getError())
    return Error("Error reading '" + InputFilename + "': " + EC.message());

  MemBuf.reset(ErrOrFile.get().release());
  if (MemBuf->getBufferSize() % 4 != 0)
    return Error("Bitcode stream should be a multiple of 4 bytes in length");
  return false;
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
  unsigned GetNextBitstreamAbbrevIndex() {
    return NextBitstreamAbbrevIndex;
  }

  /// Changes the next bitstream abbreviation index to the given value.
  void SetNextBitstreamAbbrevIndex(unsigned NextIndex) {
    NextBitstreamAbbrevIndex = NextIndex;
  }

  /// Returns true if there is an internal abbreviation index for the
  /// given bitstream abbreviation.
  bool DefinesBitstreamAbbrevIndex(unsigned Index) {
    return BitstreamToInternalAbbrevMap.find(Index) !=
        BitstreamToInternalAbbrevMap.end();
  }

  /// Returns the internal abbreviation index for the given bitstream
  /// abbreviation index.
  unsigned GetInternalAbbrevIndex(unsigned Index) {
    return BitstreamToInternalAbbrevMap.at(Index);
  }

  /// Installs the given internal abbreviation index using the next
  /// available bitstream abbreviation index.
  void InstallNewBitstreamAbbrevIndex(unsigned InternalIndex) {
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
        SetNextBitstreamAbbrevIndex(Abbrevs.size());
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
  int FindAbbreviation(const NaClBitCodeAbbrev *Abbrev) const {
    for (unsigned i = GetFirstApplicationAbbreviation();
         i < GetNumberAbbreviations(); ++i) {
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
  bool AddAbbreviation(NaClBitCodeAbbrev *Abbrev, int &Index) {
    Index = FindAbbreviation(Abbrev);
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
  bool AddAbbreviation(NaClBitCodeAbbrev *Abbrev) {
    int Index;
    return AddAbbreviation(Abbrev, Index);
  }

  /// The block ID associated with the block.
  unsigned GetBlockID() const {
    return BlockID;
  }

  /// Returns the index of the frist application abbreviation for the
  /// block.
  unsigned GetFirstApplicationAbbreviation() const {
    return naclbitc::FIRST_APPLICATION_ABBREV;
  }

  // Returns the number of abbreviations associated with the block.
  unsigned GetNumberAbbreviations() const {
    return Abbrevs.size();
  }

  /// Returns the abbreviation associated with the given abbreviation
  /// index.
  NaClBitCodeAbbrev *GetIndexedAbbrev(unsigned index) {
    if (index >= Abbrevs.size()) return 0;
    return Abbrevs[index];
  }

  // Builds the corresponding fast lookup map for finding abbreviations
  // that applies to abbreviations in the block
  void BuildAbbrevLookupSizeMap() {
    NaClBuildAbbrevLookupMap(GetLookupMap(),
                             GetAbbrevs(),
                             GetFirstApplicationAbbreviation());
    if (ShowAbbrevLookupTries) PrintLookupMap(errs());
  }

  AbbrevBitstreamToInternalMap &GetGlobalAbbrevBitstreamToInternalMap() {
    return GlobalAbbrevBitstreamToInternalMap;
  }

  AbbrevLookupSizeMap &GetLookupMap() {
    return LookupMap;
  }

  // Returns lower level vector of abbreviations.
  const AbbrevVector &GetAbbrevs() const {
    return Abbrevs;
  }

  // Returns the abbreviation (index) to use for the corresponding
  // record, based on the abbreviations of this block.  Note: Assumes
  // that BuildAbbrevLookupSizeMap has already been called.
  unsigned GetRecordAbbrevIndex(const NaClBitcodeRecordData &Record) {
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
            if (CanUseAbbreviation(Values, Iter->second, NumBits)) {
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
    if (FoundCandidate && BestScore <= UnabbreviatedSize(Record)) {
      return BestIndex;
    }
    return naclbitc::UNABBREV_RECORD;
  }

  // Computes the number of bits that will be generated by the
  // corresponding read record, if no abbreviation is used.
  static uint64_t UnabbreviatedSize(const NaClBitcodeRecordData &Record) {
    uint64_t NumBits = MatchVBRBits(Record.Code, DefaultVBRBits);
    size_t NumValues = Record.Values.size();
    NumBits += MatchVBRBits(NumValues, DefaultVBRBits);
    for (size_t Index = 0; Index < NumValues; ++Index) {
      NumBits += MatchVBRBits(Record.Values[Index], DefaultVBRBits);
    }
    return NumBits;
  }

  // Returns true if the given abbreviation can be used to represent the
  // record. Sets NumBits to the number of bits the abbreviation will
  // generate. Note: Value of NumBits is undefined if this function
  // return false.
  static bool CanUseAbbreviation(NaClBitcodeValues &Values,
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
        if (CanUseSimpleAbbrevOp(Op, Values[ValueIndex], NumBits)) {
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
        NumBits += MatchVBRBits(Values.size()-ValueIndex, DefaultVBRBits);

        // Add size of each field.
        for (; ValueIndex != ValueIndexEnd; ++ValueIndex) {
          uint64_t FieldBits=0;
          if (!CanUseSimpleAbbrevOp(ElmtOp, Values[ValueIndex], FieldBits)) {
            return false;
          }
          NumBits += FieldBits;
        }
        return true;
      }
      default: {
        if (CanUseSimpleAbbrevOp(Op, Values[ValueIndex], NumBits)) {
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
  static bool CanUseSimpleAbbrevOp(const NaClBitCodeAbbrevOp &Op,
                                   uint64_t Val,
                                   uint64_t &NumBits) {
    switch (Op.getEncoding()) {
    case NaClBitCodeAbbrevOp::Literal:
      return Val == Op.getValue();
    case NaClBitCodeAbbrevOp::Array:
      return false;
    case NaClBitCodeAbbrevOp::Fixed: {
      uint64_t Width = Op.getValue();
      if (!MatchFixedBits(Val, Width))
        return false;
      NumBits += Width;
      return true;
    }
    case NaClBitCodeAbbrevOp::VBR:
      if (unsigned Width = MatchVBRBits(Val, Op.getValue())) {
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
  static bool MatchFixedBits(uint64_t Val, unsigned Width) {
    // Note: The reader only allows up to 32 bits for fixed values.
    if (Val & Mask32) return false;
    if (Val & ~(~0U >> (32-Width))) return false;
    return true;
  }

  // Returns the number of bits needed to represent Val by abbreviation
  // operand VBR(Width). Note: Returns 0 if Val can't be represented
  // by VBR(Width).
  static unsigned MatchVBRBits(uint64_t Val, unsigned Width) {
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

  void PrintLookupMap(raw_ostream &Stream) {
    Stream << "------------------------------\n";
    Stream << "Block " << GetBlockID() << " abbreviation tries:\n";
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

/// Parses the bitcode file, analyzes it, and generates the
/// corresponding lists of global abbreviations to use in the
/// generated (compressed) bitcode file.
class NaClAnalyzeParser : public NaClBitcodeParser {
  NaClAnalyzeParser(const NaClAnalyzeParser&)
      LLVM_DELETED_FUNCTION;
  void operator=(const NaClAnalyzeParser&)
      LLVM_DELETED_FUNCTION;

public:
  // Creates the analysis parser, which will fill the given
  // BlockAbbrevsMap with appropriate abbreviations, after
  // analyzing the bitcode file defined by Cursor.
  NaClAnalyzeParser(NaClBitstreamCursor &Cursor,
                    BlockAbbrevsMapType &BlockAbbrevsMap)
      : NaClBitcodeParser(Cursor),
        BlockAbbrevsMap(BlockAbbrevsMap),
        BlockDist(&NaClCompressBlockDistElement::Sentinel),
        AbbrevListener(this)
  {
    SetListener(&AbbrevListener);
  }

  virtual ~NaClAnalyzeParser() {}

  virtual bool Error(const std::string &Message) {
    // Use local error routine so that all errors are treated uniformly.
    return ::Error(Message);
  }

  virtual bool ParseBlock(unsigned BlockID);

  // Mapping from block ID's to the corresponding list of abbreviations
  // associated with that block.
  BlockAbbrevsMapType &BlockAbbrevsMap;

  // Nested distribution capturing distribution of records in bitcode file.
  NaClBitcodeBlockDist BlockDist;

  // Listener used to get abbreviations as they are read.
  NaClBitcodeParserListener AbbrevListener;
};

class NaClBlockAnalyzeParser : public NaClBitcodeParser {
  NaClBlockAnalyzeParser(const NaClBlockAnalyzeParser&)
      LLVM_DELETED_FUNCTION;
  void operator=(NaClBlockAnalyzeParser&)
      LLVM_DELETED_FUNCTION;

public:
  /// Top-level constructor to build the top-level block with the
  /// given BlockID, and collect data (for compression) in that block.
  NaClBlockAnalyzeParser(unsigned BlockID,
                         NaClAnalyzeParser *Context)
      : NaClBitcodeParser(BlockID, Context), Context(Context) {
    Init();
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
    Init();
  }

public:
  virtual bool Error(const std::string &Message) {
    // Use local error routine so that all errors are treated uniformly.
    return ::Error(Message);
  }

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
          DefinesBitstreamAbbrevIndex(AbbrevIndex)) {
        Record.SetAbbreviationIndex(
            LocalAbbrevBitstreamToInternalMap.
            GetInternalAbbrevIndex(AbbrevIndex));
      } else {
        AbbrevBitstreamToInternalMap &GlobalAbbrevBitstreamToInternalMap =
            GlobalBlockAbbrevs->GetGlobalAbbrevBitstreamToInternalMap();
        if (GlobalAbbrevBitstreamToInternalMap.
            DefinesBitstreamAbbrevIndex(AbbrevIndex)) {
          Record.SetAbbreviationIndex(
              GlobalAbbrevBitstreamToInternalMap.
              GetInternalAbbrevIndex(AbbrevIndex));
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
    AddAbbreviation(BlockID, Abbrev->Simplify(), Index);
    if (IsLocal) {
      LocalAbbrevBitstreamToInternalMap.InstallNewBitstreamAbbrevIndex(Index);
    } else {
      GetGlobalAbbrevs(BlockID)->GetGlobalAbbrevBitstreamToInternalMap().
          InstallNewBitstreamAbbrevIndex(Index);
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
  BlockAbbrevs *GetGlobalAbbrevs(unsigned BlockID) {
    BlockAbbrevs *Abbrevs = Context->BlockAbbrevsMap[BlockID];
    if (Abbrevs == 0) {
      Abbrevs = new BlockAbbrevs(BlockID);
      Context->BlockAbbrevsMap[BlockID] = Abbrevs;
    }
    return Abbrevs;
  }

  void SetGlobalAbbrevs(unsigned BlockID, BlockAbbrevs *Abbrevs) {
    Context->BlockAbbrevsMap[BlockID] = Abbrevs;
  }

  // Adds the abbreviation to the list of abbreviations for the given
  // block.
  void AddAbbreviation(unsigned BlockID, NaClBitCodeAbbrev *Abbrev,
                       int &Index) {
    // Get block abbreviations.
    BlockAbbrevs* Abbrevs = GetGlobalAbbrevs(BlockID);

    // Read abbreviation and add as a global abbreviation.
    if (Abbrevs->AddAbbreviation(Abbrev, Index)
        && TraceGeneratedAbbreviations) {
      PrintAbbrev(errs(), BlockID, Abbrev);
    }
  }

  /// Finds the index to the corresponding internal block abbreviation
  /// for the given abbreviation.
  int FindAbbreviation(unsigned BlockID, const NaClBitCodeAbbrev *Abbrev) {
    return GetGlobalAbbrevs(BlockID)->FindAbbreviation(Abbrev);
  }

  void Init() {
    GlobalBlockAbbrevs = GetGlobalAbbrevs(GetBlockID());
    LocalAbbrevBitstreamToInternalMap.SetNextBitstreamAbbrevIndex(
        GlobalBlockAbbrevs->
        GetGlobalAbbrevBitstreamToInternalMap().GetNextBitstreamAbbrevIndex());
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
  void operator=(const UnrolledAbbreviation&) LLVM_DELETED_FUNCTION;
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
    UnrollAbbrevOp(CodeOp, Abbrev, NextOp);
    --Size;
    for (unsigned i = 0; i < Size; ++i) {
      // Create slot and then fill with appropriate operator.
      AbbrevOps.push_back(CodeOp);
      UnrollAbbrevOp(AbbrevOps[i], Abbrev, NextOp);
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
  void Print(raw_ostream &Stream) const {
    NaClBitCodeAbbrev *Abbrev = Restore(false);
    Abbrev->Print(Stream);
    Abbrev->dropRef();
  }

  /// Converts the unrolled abbreviation back into a regular
  /// abbreviation. If Simplify is true, we simplify the
  /// unrolled abbreviation as well.
  NaClBitCodeAbbrev *Restore(bool Simplify=true) const {
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
  void UnrollAbbrevOp(NaClBitCodeAbbrevOp &AbbrevOp,
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
  unsigned GetBlockID() const {
    return BlockID;
  }

  /// The abbreviation of the candidate abbreviation.
  const NaClBitCodeAbbrev *GetAbbrev() const {
    return Abbrev;
  }

  /// orders this against the candidate abbreviation.
  int Compare(const CandBlockAbbrev &CandAbbrev) const {
    unsigned diff = BlockID - CandAbbrev.BlockID;
    if (diff) return diff;
    return Abbrev->Compare(*CandAbbrev.Abbrev);
  }

  /// Prints the candidate abbreviation to the given stream.
  void Print(raw_ostream &Stream) const {
    PrintAbbrev(Stream, BlockID, Abbrev);
  }

private:
  // The block the abbreviation applies to.
  unsigned BlockID;
  // The candidate abbreviation.
  NaClBitCodeAbbrev *Abbrev;
};

static inline bool operator<(const CandBlockAbbrev &A1,
                             const CandBlockAbbrev &A2) {
  return A1.Compare(A2) < 0;
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
  bool Add(unsigned BlockID,
           UnrolledAbbreviation &UnrolledAbbrev,
           unsigned NumInstances);

  /// Returns the list of candidate abbreviations in this set.
  const AbbrevCountMap &GetAbbrevsMap() const {
    return AbbrevsMap;
  }

  /// Prints out the current contents of this set.
  void Print(raw_ostream &Stream, const char *Title = "Candidates") const {
    Stream << "-- " << Title << ": \n";
    for (const_iterator Iter = AbbrevsMap.begin(), IterEnd = AbbrevsMap.end();
         Iter != IterEnd; ++Iter) {
      Stream << format("%12u", Iter->second) << ": ";
      Iter->first.Print(Stream);
    }
    Stream << "--\n";
  }

private:
  // The set of abbreviations and corresponding number instances.
  AbbrevCountMap AbbrevsMap;

  // The map of (global) abbreviations already associated with each block.
  BlockAbbrevsMapType &BlockAbbrevsMap;
};

bool CandidateAbbrevs::Add(unsigned BlockID,
                           UnrolledAbbreviation &UnrolledAbbrev,
                           unsigned NumInstances) {
  // Drop if it corresponds to an existing global abbreviation.
  NaClBitCodeAbbrev *Abbrev = UnrolledAbbrev.Restore();
  if (BlockAbbrevs* Abbrevs = BlockAbbrevsMap[BlockID]) {
    if (Abbrevs->FindAbbreviation(Abbrev) !=
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
static bool AddNewAbbreviations(unsigned BlockID,
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
    return CandAbbrevs.Add(BlockID, CandAbbrev, Elmt->GetNumInstances());
  }
  return false;
}

// Look for new abbreviations in block BlockID, considering it was
// read with the given abbreviation Abbrev. IndexDist is the
// corresponding distribution of value indices associated with the
// abbreviation.  Any found abbreviations are added to the candidate
// abbreviations CandAbbrevs.
static void AddNewAbbreviations(unsigned BlockID,
                                const UnrolledAbbreviation &Abbrev,
                                NaClBitcodeDist &IndexDist,
                                CandidateAbbrevs &CandAbbrevs) {
  // Search based on sorted distribution, which sorts based on heuristic
  // of best index to fix first.
  const NaClBitcodeDist::Distribution *
      IndexDistribution = IndexDist.GetDistribution();
  for (NaClBitcodeDist::Distribution::const_iterator
           IndexIter = IndexDistribution->begin(),
           IndexIterEnd = IndexDistribution->end();
       IndexIter != IndexIterEnd; ++IndexIter) {
    unsigned Index = static_cast<unsigned>(IndexIter->second);
    if (AddNewAbbreviations(
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
static void AddNewAbbreviations(unsigned BlockID,
                                NaClBitCodeAbbrev *Abbrev,
                                unsigned Code,
                                NaClBitcodeDist &SizeDist,
                                CandidateAbbrevs &CandAbbrevs) {
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
      CandAbbrevs.Add(BlockID, CandAbbrev,
                      SizeDist.at(Size)->GetNumInstances());
    }
    // Now process value indices to find candidate abbreviations.
    AddNewAbbreviations(
        BlockID, UnrolledAbbrev,
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
static void AddNewAbbreviations(unsigned BlockID,
                                BlockAbbrevs *Abbrevs,
                                NaClBitcodeDist &AbbrevDist,
                                CandidateAbbrevs &CandAbbrevs) {
  const NaClBitcodeDist::Distribution *
      AbbrevDistribution = AbbrevDist.GetDistribution();
  for (NaClBitcodeDist::Distribution::const_iterator
           AbbrevIter = AbbrevDistribution->begin(),
           AbbrevIterEnd = AbbrevDistribution->end();
       AbbrevIter != AbbrevIterEnd; ++AbbrevIter) {
    NaClBitcodeDistValue AbbrevIndex = AbbrevIter->second;
    NaClBitCodeAbbrev *Abbrev = Abbrevs->GetIndexedAbbrev(AbbrevIndex);
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
      AddNewAbbreviations(
          BlockID,
          Abbrev,
          Code,
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
static void AddNewAbbreviations(NaClBitcodeBlockDist &BlockDist,
                                BlockAbbrevsMapType &BlockAbbrevsMap) {
  CandidateAbbrevs CandAbbrevs(BlockAbbrevsMap);
  // Start by collecting candidate abbreviations.
  const NaClBitcodeDist::Distribution *
      Distribution = BlockDist.GetDistribution();
  for (NaClBitcodeDist::Distribution::const_iterator
           Iter = Distribution->begin(), IterEnd = Distribution->end();
       Iter != IterEnd; ++Iter) {
    unsigned BlockID = static_cast<unsigned>(Iter->second);
    AddNewAbbreviations(
        BlockID,
        BlockAbbrevsMap[BlockID],
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
  // index to choose (via method GetRecordAbbrevIndex).
  //
  // Selecting the second is important in that abbreviation are
  // refined by successive calls to this tool. We do not want to
  // restrict downstream refinements prematurely.  Hence, we want the
  // tool to choose the abbreviation with the most candidates first.
  //
  // Since method GetRecordAbbrevIndex chooses the first abbreviation
  // that generates the least number of bits, we clearly want to make
  // sure that the second abbreviation occurs before the first.
  std::vector<CountedAbbrevType> Candidates;
  for (CandidateAbbrevs::const_iterator
           Iter = CandAbbrevs.GetAbbrevsMap().begin(),
           IterEnd = CandAbbrevs.GetAbbrevsMap().end();
       Iter != IterEnd; ++Iter) {
    Candidates.push_back(CountedAbbrevType(Iter->second,Iter->first));
  }
  std::sort(Candidates.begin(), Candidates.end());
  std::vector<CountedAbbrevType>::const_reverse_iterator
      Iter = Candidates.rbegin(), IterEnd = Candidates.rend();
  if (Iter == IterEnd) return;

  if (TraceGeneratedAbbreviations) {
    errs() << "-- New abbrevations:\n";
  }
  unsigned Min = (Iter->first >> 2);
  for (; Iter != IterEnd; ++Iter) {
    if (Iter->first < Min) break;
    unsigned BlockID = Iter->second.GetBlockID();
    BlockAbbrevs *Abbrevs = BlockAbbrevsMap[BlockID];
    NaClBitCodeAbbrev *Abbrev = Iter->second.GetAbbrev()->Copy();
    if (TraceGeneratedAbbreviations) {
      errs() <<format("%12u", Iter->first) << ": ";
      PrintAbbrev(errs(), BlockID, Abbrev);
    }
    Abbrevs->AddAbbreviation(Abbrev);
  }
  if (TraceGeneratedAbbreviations) {
    errs() << "--\n";
  }
}

// Walks the block distribution (BlockDist), sorting entries based
// on the distribution of blocks and abbreviations, and then
// prints out the frequency of each abbreviation used.
static void DisplayAbbreviationFrequencies(
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
        BlockPos->second->GetIndexedAbbrev(Index)->Print(Output);
      }
    }
    Output << '\n';
  }
}

// Read in bitcode, analyze data, and figure out set of abbreviations
// to use, from memory buffer MemBuf containing the input bitcode file.
static bool AnalyzeBitcode(std::unique_ptr<MemoryBuffer> &MemBuf,
                           BlockAbbrevsMapType &BlockAbbrevsMap) {
  // TODO(kschimpf): The current code only extracts abbreviations
  // defined in the bitcode file. This code needs to be updated to
  // collect data distributions and figure out better (global)
  // abbreviations to use.

  const unsigned char *BufPtr = (const unsigned char *)MemBuf->getBufferStart();
  const unsigned char *EndBufPtr = BufPtr+MemBuf->getBufferSize();

  // First read header and verify it is good.
  NaClBitcodeHeader Header;
  if (Header.Read(BufPtr, EndBufPtr) || !Header.IsSupported())
    return Error("Invalid PNaCl bitcode header");

  // Create a bitstream reader to read the bitcode file.
  NaClBitstreamReader StreamFile(BufPtr, EndBufPtr);
  NaClBitstreamCursor Stream(StreamFile);

  // Parse the the bitcode file.
  NaClAnalyzeParser Parser(Stream, BlockAbbrevsMap);
  while (!Stream.AtEndOfStream()) {
    if (Parser.Parse()) return true;
  }

  if (ShowAbbreviationFrequencies || ShowValueDistributions) {
    std::string ErrorInfo;
    raw_fd_ostream Output(OutputFilename.c_str(), ErrorInfo, sys::fs::F_None);
    if (!ErrorInfo.empty()) {
      errs() << ErrorInfo << "\n";
      exit(1);
    }
    if (ShowAbbreviationFrequencies)
      DisplayAbbreviationFrequencies(Output, Parser.BlockDist, BlockAbbrevsMap);
    if (ShowValueDistributions)
      Parser.BlockDist.Print(Output);
  }

  AddNewAbbreviations(Parser.BlockDist, BlockAbbrevsMap);
  return false;
}

/// Parses the input bitcode file and generates the corresponding
/// compressed bitcode file, by replacing abbreviations in the input
/// file with the corresponding abbreviations defined in
/// BlockAbbrevsMap.
class NaClBitcodeCopyParser : public NaClBitcodeParser {
public:
  // Top-level constructor to build the appropriate block parser
  // using the given BlockAbbrevsMap to define abbreviations.
  NaClBitcodeCopyParser(NaClBitstreamCursor &Cursor,
                        BlockAbbrevsMapType &BlockAbbrevsMap,
                        NaClBitstreamWriter &Writer)
      : NaClBitcodeParser(Cursor),
        BlockAbbrevsMap(BlockAbbrevsMap),
        Writer(Writer),
        FoundFirstBlockInfo(false)
  {}

  virtual ~NaClBitcodeCopyParser() {}

  virtual bool Error(const std::string &Message) {
    // Use local error routine so that all errors are treated uniformly.
    return ::Error(Message);
  }

  virtual bool ParseBlock(unsigned BlockID);

  // The abbreviations to use for the copied bitcode.
  BlockAbbrevsMapType &BlockAbbrevsMap;

  // The bitstream to copy the compressed bitcode into.
  NaClBitstreamWriter &Writer;

  // True if we have already found the first block info block.
  // Used to make sure we don't use abbreviations until we
  // have put them into the bitcode file.
  bool FoundFirstBlockInfo;
};

class NaClBlockCopyParser : public NaClBitcodeParser {
public:
  // Top-level constructor to build the appropriate block parser.
  NaClBlockCopyParser(unsigned BlockID,
                      NaClBitcodeCopyParser *Context)
      : NaClBitcodeParser(BlockID, Context),
        Context(Context),
        BlockAbbreviations(0)
  {}

  virtual ~NaClBlockCopyParser() {}

protected:
  // The context defining state associated with the block parser.
  NaClBitcodeCopyParser *Context;

  // The block abbreviations defined for this block (initialized by
  // EnterBlock).
  BlockAbbrevs *BlockAbbreviations;

  /// Constructor to parse nested blocks.  Creates a block parser to
  /// parse in a block with the given BlockID, and write the block
  /// back out using the abbreviations in BlockAbbrevsMap.
  NaClBlockCopyParser(unsigned BlockID,
                      NaClBlockCopyParser *EnclosingParser)
      : NaClBitcodeParser(BlockID, EnclosingParser),
        Context(EnclosingParser->Context)
  {}

  virtual bool Error(const std::string &Message) {
    // Use local error routine so that all errors are treated uniformly.
    return ::Error(Message);
  }

  /// Returns the set of (global) block abbreviations defined for the
  /// given block ID.
  BlockAbbrevs *GetGlobalAbbrevs(unsigned BlockID) {
    BlockAbbrevs *Abbrevs = Context->BlockAbbrevsMap[BlockID];
    if (Abbrevs == 0) {
      Abbrevs = new BlockAbbrevs(BlockID);
      Context->BlockAbbrevsMap[BlockID] = Abbrevs;
    }
    return Abbrevs;
  }

  virtual bool ParseBlock(unsigned BlockID) {
    NaClBlockCopyParser Parser(BlockID, this);
    return Parser.ParseThisBlock();
  }

  virtual void EnterBlock(unsigned NumWords) {
    unsigned BlockID = GetBlockID();
    BlockAbbreviations = GetGlobalAbbrevs(BlockID);

    // Enter the subblock.
    NaClBitcodeSelectorAbbrev
        Selector(BlockAbbreviations->GetNumberAbbreviations()-1);
    if (RemoveAbbreviations) Selector = NaClBitcodeSelectorAbbrev();
    Context->Writer.EnterSubblock(BlockID, Selector);

    // Note: We must dump module abbreviations as local
    // abbreviations, because they are in a yet to be
    // dumped BlockInfoBlock.
    if (!RemoveAbbreviations && BlockID == naclbitc::MODULE_BLOCK_ID) {
      BlockAbbrevs* Abbrevs = GetGlobalAbbrevs(naclbitc::MODULE_BLOCK_ID);
      for (unsigned i = 0; i < Abbrevs->GetNumberAbbreviations(); ++i) {
        Context->Writer.EmitAbbrev(Abbrevs->GetIndexedAbbrev(i)->Copy());
      }
    }
  }

  virtual void ExitBlock() {
    Context->Writer.ExitBlock();
  }

  virtual void ProcessBlockInfo() {
    assert(!Context->FoundFirstBlockInfo &&
           "Input bitcode has more that one BlockInfoBlock");
    Context->FoundFirstBlockInfo = true;

    // Generate global abbreviations within a blockinfo block.
    Context->Writer.EnterBlockInfoBlock();
    if (!RemoveAbbreviations) {
      for (BlockAbbrevsMapType::const_iterator
               Iter = Context->BlockAbbrevsMap.begin(),
               IterEnd = Context->BlockAbbrevsMap.end();
           Iter != IterEnd; ++Iter) {
        unsigned BlockID = Iter->first;
        // Don't emit module abbreviations, since they have been
        // emitted as local abbreviations.
        if (BlockID == naclbitc::MODULE_BLOCK_ID) continue;

        BlockAbbrevs *Abbrevs = Iter->second;
        if (Abbrevs == 0) continue;
        for (unsigned i = Abbrevs->GetFirstApplicationAbbreviation();
             i < Abbrevs->GetNumberAbbreviations(); ++i) {
          NaClBitCodeAbbrev *Abbrev = Abbrevs->GetIndexedAbbrev(i);
          Context->Writer.EmitBlockInfoAbbrev(BlockID, Abbrev);
        }
      }
    }
    Context->Writer.ExitBlock();
  }

  virtual void ProcessRecord() {
    const NaClBitcodeRecord::RecordVector &Values = Record.GetValues();
    if (RemoveAbbreviations) {
      Context->Writer.EmitRecord(Record.GetCode(), Values, 0);
      return;
    }
    // Find best fitting abbreviation to use, and print out the record
    // using that abbreviations.
    unsigned AbbrevIndex =
        BlockAbbreviations->GetRecordAbbrevIndex(Record.GetRecordData());
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
// bitcode file.
static bool CopyBitcode(std::unique_ptr<MemoryBuffer> &MemBuf,
                        BlockAbbrevsMapType &BlockAbbrevsMap) {

  const unsigned char *BufPtr = (const unsigned char *)MemBuf->getBufferStart();
  const unsigned char *EndBufPtr = BufPtr+MemBuf->getBufferSize();

  // Read header. No verification is needed since AnalyzeBitcode has
  // already checked it.
  NaClBitcodeHeader Header;
  if (Header.Read(BufPtr, EndBufPtr))
    return Error("Invalid PNaCl bitcode header");

  // Create the bitcode reader.
  NaClBitstreamReader StreamFile(BufPtr, EndBufPtr);
  NaClBitstreamCursor Stream(StreamFile);

  // Create the bitcode writer.
  SmallVector<char, 0> OutputBuffer;
  OutputBuffer.reserve(256*1024);
  NaClBitstreamWriter StreamWriter(OutputBuffer);

  // Emit the file header.
  NaClWriteHeader(Header, StreamWriter);

  // Set up the parser.
  NaClBitcodeCopyParser Parser(Stream, BlockAbbrevsMap, StreamWriter);

  // Parse the bitcode and copy.
  while (!Stream.AtEndOfStream()) {
    if (Parser.Parse()) return true;
  }

  // Write out the copied results.
  std::string ErrorInfo;
  std::unique_ptr<tool_output_file> OutFile(
      new tool_output_file(OutputFilename.c_str(), ErrorInfo, sys::fs::F_None));
  if (!ErrorInfo.empty())
    return Error(ErrorInfo);

  // Write the generated bitstream to "Out".
  OutFile->os().write((char*)&OutputBuffer.front(),
                      OutputBuffer.size());
  OutFile->keep();

  return false;
}

// Build fast lookup abbreviation maps for each of the abbreviation blocks
// defined in AbbrevsMap.
static void BuildAbbrevLookupMaps(BlockAbbrevsMapType &AbbrevsMap) {
  for (BlockAbbrevsMapType::const_iterator
           Iter = AbbrevsMap.begin(),
           IterEnd = AbbrevsMap.end();
       Iter != IterEnd; ++Iter) {
    Iter->second->BuildAbbrevLookupSizeMap();
  }
}

}  // namespace

int main(int argc, char **argv) {
  // Print a stack trace if we signal out.
  sys::PrintStackTraceOnErrorSignal();
  PrettyStackTraceProgram X(argc, argv);
  llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.
  cl::ParseCommandLineOptions(argc, argv, "pnacl-bccompress file analyzer\n");

  std::unique_ptr<MemoryBuffer> MemBuf;
  if (ReadAndBuffer(MemBuf)) return 1;
  BlockAbbrevsMapType BlockAbbrevsMap;
  if (AnalyzeBitcode(MemBuf, BlockAbbrevsMap)) return 1;
  if (ShowAbbreviationFrequencies || ShowValueDistributions) {
    return 0;
  }
  BuildAbbrevLookupMaps(BlockAbbrevsMap);
  if (CopyBitcode(MemBuf, BlockAbbrevsMap)) return 1;
  return 0;
}
