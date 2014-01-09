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
// of a block. They are defined by abbreviations processed by the
// ProcessRecordAbbrev method of class NaClBitcodeParser.
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

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeHeader.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeParser.h"
#include "llvm/Bitcode/NaCl/NaClBitstreamReader.h"
#include "llvm/Bitcode/NaCl/NaClBitstreamWriter.h"
#include "llvm/Bitcode/NaCl/NaClReaderWriter.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/system_error.h"
#include "llvm/Support/ToolOutputFile.h"
#include <set>
#include <map>

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

/// Error - All bitcode analysis errors go through this function,
/// making this a good place to breakpoint if debugging.
static bool Error(const std::string &Err) {
  errs() << Err << "\n";
  return true;
}

// For debugging. Prints out the abbreviation in readable form to errs().
static void PrintAbbrev(unsigned BlockID, const NaClBitCodeAbbrev *Abbrev) {
  errs() << "Abbrev(block " << BlockID << "): [";
  // ContinuationCount>0 implies that the current operand is a
  // continuation of previous operand(s).
  unsigned ContinuationCount = 0;
  for (unsigned i = 0; i < Abbrev->getNumOperandInfos(); ++i) {
    if (i > 0 && ContinuationCount == 0) errs() << ", ";
    const NaClBitCodeAbbrevOp &Op = Abbrev->getOperandInfo(i);
    if (Op.isLiteral()) {
      errs() << Op.getLiteralValue();
    } else if (Op.isEncoding()) {
      switch (Op.getEncoding()) {
      case NaClBitCodeAbbrevOp::Fixed:
        errs() << "Fixed(" << Op.getEncodingData() << ")";
        break;
      case NaClBitCodeAbbrevOp::VBR:
        errs() << "VBR(" << Op.getEncodingData() << ")";
        break;
      case NaClBitCodeAbbrevOp::Array:
        errs() << "Array:";
        ++ContinuationCount;
        continue;
      case NaClBitCodeAbbrevOp::Char6:
        errs() << "Char6";
        break;
      case NaClBitCodeAbbrevOp::Blob:
        errs() << "Blob";
        break;
      default:
        errs() << "??";
        break;
      }
    } else {
      errs() << "??";
    }
    if (ContinuationCount) --ContinuationCount;
  }
  errs() << "]\n";
}

// Reads the input file into the given buffer.
static bool ReadAndBuffer(OwningPtr<MemoryBuffer> &MemBuf) {
  if (error_code ec =
      MemoryBuffer::getFileOrSTDIN(InputFilename.c_str(), MemBuf)) {
    return Error("Error reading '" + InputFilename + "': " + ec.message());
  }

  if (MemBuf->getBufferSize() % 4 != 0)
    return Error("Bitcode stream should be a multiple of 4 bytes in length");
  return false;
}

/// Defines the list of abbreviations associated with a block.
class BlockAbbrevs {
public:
  // Vector to hold the (ordered) list of abbreviations.
  typedef SmallVector<NaClBitCodeAbbrev*, 32> AbbrevVector;

  BlockAbbrevs(unsigned BlockID)
      : BlockID(BlockID) {}

  ~BlockAbbrevs() {
    for (AbbrevVector::const_iterator
             Iter = Abbrevs.begin(), IterEnd = Abbrevs.end();
         Iter != IterEnd; ++Iter) {
      (*Iter)->dropRef();
    }
  }

  // Constant used to denote that a given abbreviation is not in the
  // set of abbreviations defined by the block.
  static const int NO_SUCH_ABBREVIATION = -1;

  // Returns the index to the corresponding abbreviation, if it
  // exists.  Otherwise return NO_SUCH_ABBREVIATION;
  int FindAbbreviation(const NaClBitCodeAbbrev *Abbrev) const {
    for (unsigned i = 0; i < Abbrevs.size(); ++i) {
      if (*Abbrevs[i] == *Abbrev) return i;
    }
    return NO_SUCH_ABBREVIATION;
  }

  /// Adds the given abbreviation to the set of global abbreviations
  /// defined for the block. Guarantees that duplicate abbreviations
  /// are not added to the block. Note: Code takes ownership of
  /// the given abbreviation. Returns true if new abbreviation.
  bool AddAbbreviation(NaClBitCodeAbbrev *Abbrev) {
    int Index = FindAbbreviation(Abbrev);
    if (Index != NO_SUCH_ABBREVIATION) {
      // Already defined, don't install.
      Abbrev->dropRef();
      return false;
    }

    // New abbreviation. Add.
    Abbrevs.push_back(Abbrev);
    return true;
  }

  /// The block ID associated with the block.
  unsigned GetBlockID() const {
    return BlockID;
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

  /// Converts to corresponding bitstream abbreviation index.
  static unsigned ConvertToBitstreamAbbrevIndex(unsigned Index) {
    /// Note: the abbreviation indices are ordered using the
    /// position in AbbrevVector Abbrevs, which is dynamically created.
    /// Hence, we convert it by moving the index past the manditory
    // abbreviation indices defined by the bitstream reader/writer.
    return Index + naclbitc::FIRST_APPLICATION_ABBREV;
  }

  /// Converts bitstream abbreviation index back to corresponding
  /// block abbreviation index.
  static unsigned ConvertToAbbrevIndex(unsigned Index) {
    return Index - naclbitc::FIRST_APPLICATION_ABBREV;
  }

private:
  // The block ID for which abbreviations are being associated.
  unsigned BlockID;
  // The list of abbreviations defined for the block.
  AbbrevVector Abbrevs;
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
        BlockAbbrevsMap(BlockAbbrevsMap)
  {}

  virtual ~NaClAnalyzeParser() {}

  virtual bool Error(const std::string &Message) {
    // Use local error routine so that all errors are treated uniformly.
    return ::Error(Message);
  }

  virtual bool ParseBlock(unsigned BlockID);

  // Mapping from block ID's to the corresponding list of abbreviations
  // associated with that block.
  BlockAbbrevsMapType &BlockAbbrevsMap;
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
      : NaClBitcodeParser(BlockID, Context), Context(Context)
  {}

  virtual ~NaClBlockAnalyzeParser() {}

protected:
  /// Nested constructor to parse a block within a block.  Creates a
  /// block parser to parse a block with the given BlockID, and
  /// collect data (for compression) in that block.
  NaClBlockAnalyzeParser(unsigned BlockID,
                         NaClBlockAnalyzeParser *EnclosingParser)
      : NaClBitcodeParser(BlockID, EnclosingParser),
        Context(EnclosingParser->Context)
  {}

public:
  virtual bool Error(const std::string &Message) {
    // Use local error routine so that all errors are treated uniformly.
    return ::Error(Message);
  }

  virtual bool ParseBlock(unsigned BlockID) {
    NaClBlockAnalyzeParser Parser(BlockID, this);
    return Parser.ParseThisBlock();
  }

  virtual void EnterBlock(unsigned NumWords) {
    // Make sure that we have a block abbreviations record defined for
    // the corresponding block ID.
    unsigned BlockID = GetBlockID();
    BlockAbbrevs *Abbrevs = Context->BlockAbbrevsMap[BlockID];
    if (Abbrevs == 0) {
      Abbrevs = new BlockAbbrevs(BlockID);
      Context->BlockAbbrevsMap[BlockID] = Abbrevs;
    }
  }

  virtual void ProcessRecordAbbrev() {
    // Convert the local abbreviation to a corresponding global
    // abbreviation.

    // TODO(kschimpf): Replace this with appropriate code once we are
    // building our own abbreviations.

    AddAbbreviation(GetBlockID(), CopyAbbreviation(
        Record.GetCursor().GetNewestAbbrev()));
  }

  virtual void ExitBlockInfo() {
    // Now extract out global abbreviations and put into corresponding
    // block abbreviations map, so that they will be used when the
    // bitcode is compressed.

    // TODO(kschimpf): Replace with appropriate code once we are
    // building our own abbreviations.

    NaClBitstreamReader &Reader = Record.GetReader();
    SmallVector<unsigned, 12> BlockIDs;
    Reader.GetBlockInfoBlockIDs(BlockIDs);
    for (SmallVectorImpl<unsigned>::const_iterator
             IDIter = BlockIDs.begin(), IDIterEnd = BlockIDs.end();
         IDIter != IDIterEnd; ++IDIter) {
      unsigned BlockID = *IDIter;
      if (const NaClBitstreamReader::BlockInfo *Info =
          Reader.getBlockInfo(BlockID)) {
        for (std::vector<NaClBitCodeAbbrev*>::const_iterator
                 AbbrevIter = Info->Abbrevs.begin(),
                 AbbrevIterEnd = Info->Abbrevs.end();
             AbbrevIter != AbbrevIterEnd;
             ++AbbrevIter) {
          AddAbbreviation(BlockID, CopyAbbreviation(*AbbrevIter));
        }
      }
    }
  }

protected:
  // The context parser, defining locals to parsing blocks.
  NaClAnalyzeParser *Context;

  // Creates a copy of the given abbreviation.
  NaClBitCodeAbbrev *CopyAbbreviation(const NaClBitCodeAbbrev *Abbrev) const {
    NaClBitCodeAbbrev *Copy = new NaClBitCodeAbbrev();
    for (unsigned I=0, IEnd = Abbrev->getNumOperandInfos();
         I != IEnd; ++I) {
      Copy->Add(NaClBitCodeAbbrevOp(Abbrev->getOperandInfo(I)));
    }
    return Copy;
  }

  // Adds the abbreviation to the list of abbreviations for the given
  // block.
  void AddAbbreviation(unsigned BlockID, NaClBitCodeAbbrev *Abbrev) {
    // Get block abbreviations.
    BlockAbbrevs* Abbrevs = Context->BlockAbbrevsMap[BlockID];
    if (Abbrevs == 0) {
      Abbrevs = new BlockAbbrevs(BlockID);
      Context->BlockAbbrevsMap[BlockID] = Abbrevs;
    }
    // Read abbreviation and add as a global abbreviation.
    if (Abbrevs->AddAbbreviation(Abbrev) && TraceGeneratedAbbreviations) {
      PrintAbbrev(BlockID, Abbrev);
    }
  }

};

bool NaClAnalyzeParser::ParseBlock(unsigned BlockID) {
  NaClBlockAnalyzeParser Parser(BlockID, this);
  return Parser.ParseThisBlock();
}

// Read in bitcode, analyze data, and figure out set of abbreviations
// to use, from memory buffer MemBuf containing the input bitcode file.
static bool AnalyzeBitcode(OwningPtr<MemoryBuffer> &MemBuf,
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

  // Masks out the top-32 bits of a uint64_t value.
  static const uint64_t Mask32 = 0xFFFFFFFF00000000;

  // Defines the number of bits used to print VBR array field values.
  static const unsigned DefaultVBRBits = 6;

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

  virtual bool ParseBlock(unsigned BlockID) {
    NaClBlockCopyParser Parser(BlockID, this);
    return Parser.ParseThisBlock();
  }

  virtual void EnterBlock(unsigned NumWords) {
    unsigned BlockID = GetBlockID();
    BlockAbbreviations = Context->BlockAbbrevsMap[BlockID];

    // Enter the subblock.
    NaClBitcodeSelectorAbbrev Selector(
        BlockAbbrevs::ConvertToBitstreamAbbrevIndex(
            BlockAbbreviations->GetNumberAbbreviations()-1));
    Context->Writer.EnterSubblock(BlockID, Selector);
  }

  virtual void ExitBlock() {
    Context->Writer.ExitBlock();
  }

  virtual void ExitBlockInfo() {
    assert(!Context->FoundFirstBlockInfo &&
           "Input bitcode has more that one BlockInfoBlock");
    Context->FoundFirstBlockInfo = true;

    // Generate global abbreviations within a blockinfo block.
    Context->Writer.EnterBlockInfoBlock();
    for (BlockAbbrevsMapType::const_iterator
             Iter = Context->BlockAbbrevsMap.begin(),
             IterEnd = Context->BlockAbbrevsMap.end();
         Iter != IterEnd; ++Iter) {
      unsigned BlockID = Iter->first;
      BlockAbbrevs *Abbrevs = Iter->second;
      if (Abbrevs == 0) continue;
      for (unsigned i = 0; i < Abbrevs->GetNumberAbbreviations(); ++i) {
        NaClBitCodeAbbrev *Abbrev = Abbrevs->GetIndexedAbbrev(i);
        Context->Writer.EmitBlockInfoAbbrev(BlockID, Abbrev);
      }
    }
    Context->Writer.ExitBlock();
  }

  virtual void ProcessRecord() {
    // Find best fitting abbreviation to use, and print out the record
    // using that abbreviations.
    unsigned AbbrevIndex = GetRecordAbbrevIndex();

    const NaClBitcodeRecord::RecordVector &Values = Record.GetValues();
    if (AbbrevIndex == naclbitc::UNABBREV_RECORD) {
      Context->Writer.EmitRecord(Record.GetCode(), Values, 0);
    } else {
      Context->Writer.EmitRecord(Record.GetCode(), Values, AbbrevIndex);
    }
  }

  /// Returns the abbreviation (index) to use for the corresponding
  /// read record.
  unsigned GetRecordAbbrevIndex() {

    // Note: We can't use abbreviations till they have been inserted
    // into the bitcode file. So give up if the record appears before
    // where they are inserted (which is where the first BlockInfo
    // block appears in the input bitcode file).
    if (!Context->FoundFirstBlockInfo)
      return naclbitc::UNABBREV_RECORD;

    BlockAbbrevs *Abbrevs = BlockAbbreviations;
    unsigned NumCandidates = Abbrevs->GetNumberAbbreviations();
    unsigned BestIndex = 0; // Ignored unless found candidate.
    unsigned BestScore = 0; // Number of bits associated with BestIndex.
    bool FoundCandidate = false;
    for (unsigned Index = 0; Index < NumCandidates; ++Index) {
      uint64_t NumBits = 0;
      if (CanUseAbbreviation(Abbrevs->GetIndexedAbbrev(Index), NumBits)) {
        if (!FoundCandidate || NumBits < BestScore) {
          // Use this as candidate.
          BestIndex = Index;
          BestScore = NumBits;
          FoundCandidate = true;
        }
      }
    }
    if (FoundCandidate && BestScore <= UnabbreviatedSize()) {
      return BlockAbbrevs::ConvertToBitstreamAbbrevIndex(BestIndex);
    }
    else {
      return naclbitc::UNABBREV_RECORD;
    }
  }

  // Computes the number of bits that will be generated by the
  // corresponding read record, if no abbreviation is used.
  uint64_t UnabbreviatedSize() {
    uint64_t NumBits = MatchVBRBits(Record.GetCode(), DefaultVBRBits);
    const NaClBitcodeRecord::RecordVector &Values = Record.GetValues();
    size_t NumValues = Values.size();
    NumBits += MatchVBRBits(NumValues, DefaultVBRBits);
    for (size_t Index = 0; Index < NumValues; ++Index) {
      NumBits += MatchVBRBits(Values[Index], DefaultVBRBits);
    }
    return NumBits;
  }

  /// Simple container class to convert the values of the
  /// corresponding read record to the form expected by
  /// abbreviations. That is, the record code is prefixed
  /// to the set of values in the record.
  struct AbbrevValues {
  public:
    AbbrevValues(const NaClBitcodeRecord &Record)
        : Code(Record.GetCode()), Values(Record.GetValues()) {}

    size_t size() const {
      return Values.size()+1;
    }

    uint64_t operator[](size_t index) const {
      return index == 0 ? Code : Values[index-1];
    }

  private:
    uint64_t Code;
    const NaClBitcodeRecord::RecordVector &Values;
  };

  // Returns true if the given abbreviation can be used to represent the
  // record. Sets NumBits to the number of bits the abbreviation will
  // generate. Note: Value of NumBits is undefined if this function
  // return false.
  bool CanUseAbbreviation(NaClBitCodeAbbrev *Abbrev, uint64_t &NumBits) {
    NumBits = 0;
    unsigned OpIndex = 0;
    unsigned OpIndexEnd = Abbrev->getNumOperandInfos();
    AbbrevValues Values(Record);
    size_t ValueIndex = 0;
    size_t ValueIndexEnd = Values.size();
    while (ValueIndex < ValueIndexEnd && OpIndex < OpIndexEnd) {
      const NaClBitCodeAbbrevOp &Op = Abbrev->getOperandInfo(OpIndex);
      if (Op.isLiteral()) {
        if (CanUseSimpleAbbrevOp(Op, Values[ValueIndex], NumBits)) {
          ++ValueIndex;
          ++OpIndex;
          continue;
        } else {
          return false;
        }
      }
      switch (Op.getEncoding()) {
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
      case NaClBitCodeAbbrevOp::Blob:
        assert(OpIndex+1 == OpIndexEnd);
        // Add size of blob.
        NumBits += MatchVBRBits(Values.size()-ValueIndex, DefaultVBRBits);

        // We don't know how many bits are needed to word align, so we
        // will assume 32. This makes blob more expensive than array
        // unless there is a lot of elements that can modeled using
        // fewer bits.
        NumBits += 32;

        // Add size of each byte in blob.
        for (; ValueIndex != ValueIndexEnd; ++ValueIndex) {
          if (Values[ValueIndex] >= 256) {
            return false;
          }
          NumBits += 8;
        }
        return true;
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
    if (Op.isLiteral())
      return Val == Op.getLiteralValue();

    switch (Op.getEncoding()) {
    case NaClBitCodeAbbrevOp::Array:
    case NaClBitCodeAbbrevOp::Blob:
      return false;
    case NaClBitCodeAbbrevOp::Fixed: {
      uint64_t Width = Op.getEncodingData();
      if (!MatchFixedBits(Val, Width))
        return false;
      NumBits += Width;
      return true;
    }
    case NaClBitCodeAbbrevOp::VBR:
      if (unsigned Width = MatchVBRBits(Val, Op.getEncodingData())) {
        NumBits += Width;
        return true;
      } else {
        return false;
      }
    case NaClBitCodeAbbrevOp::Char6:
      if (!NaClBitCodeAbbrevOp::isChar6(Val)) return false;
      NumBits += 6;
      return true;
    default:
      assert(0 && "Bad abbreviation operator");
      return false;
    }
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
};

bool NaClBitcodeCopyParser::ParseBlock(unsigned BlockID) {
  NaClBlockCopyParser Parser(BlockID, this);
  return Parser.ParseThisBlock();
}

// Read in bitcode, and write it back out using the abbreviations in
// BlockAbbrevsMap, from memory buffer MemBuf containing the input
// bitcode file.
static bool CopyBitcode(OwningPtr<MemoryBuffer> &MemBuf,
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
  OwningPtr<tool_output_file> OutFile(
      new tool_output_file(OutputFilename.c_str(), ErrorInfo,
                           raw_fd_ostream::F_Binary));
  if (!ErrorInfo.empty())
    return Error(ErrorInfo);

  // Write the generated bitstream to "Out".
  OutFile->os().write((char*)&OutputBuffer.front(),
                      OutputBuffer.size());
  OutFile->keep();

  return false;
}

}  // namespace

int main(int argc, char **argv) {
  // Print a stack trace if we signal out.
  sys::PrintStackTraceOnErrorSignal();
  PrettyStackTraceProgram X(argc, argv);
  llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.
  cl::ParseCommandLineOptions(argc, argv, "pnacl-bccompress file analyzer\n");

  OwningPtr<MemoryBuffer> MemBuf;
  if (ReadAndBuffer(MemBuf)) return 1;
  BlockAbbrevsMapType BlockAbbrevsMap;
  if (AnalyzeBitcode(MemBuf, BlockAbbrevsMap)) return 1;
  if (CopyBitcode(MemBuf, BlockAbbrevsMap)) return 1;
  return 0;
}
