//===-- NaClBitcodeAnalyzer.cpp - Bitcode Analyzer ------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "nacl-bitcode-analyzer"

#include "llvm/ADT/OwningPtr.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeAnalyzer.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeHeader.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeParser.h"
#include "llvm/Bitcode/NaCl/NaClCommonBitcodeRecordDists.h"
#include "llvm/Bitcode/NaCl/NaClBitstreamReader.h"
#include "llvm/Bitcode/NaCl/NaClLLVMBitCodes.h"
#include "llvm/Bitcode/NaCl/NaClReaderWriter.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/system_error.h"
#include <algorithm>
#include <map>

/// Error - All bitcode analysis errors go through this function, making this a
/// good place to breakpoint if debugging.
static bool Error(const llvm::Twine &Err) {
  llvm::errs() << Err << "\n";
  return true;
}

namespace llvm {

/// GetBlockName - Return a symbolic block name if known, otherwise return
/// null.
static const char *GetBlockName(unsigned BlockID) {
  // Standard blocks for all bitcode files.
  if (BlockID < naclbitc::FIRST_APPLICATION_BLOCKID) {
    if (BlockID == naclbitc::BLOCKINFO_BLOCK_ID)
      return "BLOCKINFO_BLOCK";
    return 0;
  }

  switch (BlockID) {
  default: return 0;
  case naclbitc::MODULE_BLOCK_ID:          return "MODULE_BLOCK";
  case naclbitc::PARAMATTR_BLOCK_ID:       return "PARAMATTR_BLOCK";
  case naclbitc::PARAMATTR_GROUP_BLOCK_ID: return "PARAMATTR_GROUP_BLOCK_ID";
  case naclbitc::TYPE_BLOCK_ID_NEW:        return "TYPE_BLOCK_ID";
  case naclbitc::CONSTANTS_BLOCK_ID:       return "CONSTANTS_BLOCK";
  case naclbitc::FUNCTION_BLOCK_ID:        return "FUNCTION_BLOCK";
  case naclbitc::VALUE_SYMTAB_BLOCK_ID:    return "VALUE_SYMTAB";
  case naclbitc::METADATA_BLOCK_ID:        return "METADATA_BLOCK";
  case naclbitc::METADATA_ATTACHMENT_ID:   return "METADATA_ATTACHMENT_BLOCK";
  case naclbitc::USELIST_BLOCK_ID:         return "USELIST_BLOCK_ID";
  case naclbitc::GLOBALVAR_BLOCK_ID:       return "GLOBALVAR_BLOCK";
  }
}

struct PerBlockIDStats {
private:
  PerBlockIDStats(const PerBlockIDStats&) LLVM_DELETED_FUNCTION;
  void operator=(const PerBlockIDStats&) LLVM_DELETED_FUNCTION;

public:
  /// NumInstances - This the number of times this block ID has been
  /// seen.
  unsigned NumInstances;

  /// NumBits - The total size in bits of all of these blocks.
  uint64_t NumBits;

  /// NumSubBlocks - The total number of blocks these blocks contain.
  unsigned NumSubBlocks;

  /// NumAbbrevs - The total number of abbreviations.
  unsigned NumAbbrevs;

  /// NumRecords - The total number of records these blocks contain,
  /// and the number that are abbreviated.
  unsigned NumRecords, NumAbbreviatedRecords;

  /// RecordCodeDist - Distribution of each record code for this
  /// block.
  NaClBitcodeRecordCodeDist RecordCodeDist;

  explicit PerBlockIDStats(unsigned BlockID)
    : NumInstances(0), NumBits(0),
      NumSubBlocks(0), NumAbbrevs(0), NumRecords(0), NumAbbreviatedRecords(0),
      RecordCodeDist(BlockID)
  {}
};

// Parses all bitcode blocks, and collects distribution of records in
// each block.  Also dumps bitcode structure if specified (via global
// variables).
class PNaClBitcodeAnalyzerParser : public NaClBitcodeParser {
public:
  PNaClBitcodeAnalyzerParser(NaClBitstreamCursor &Cursor,
                             raw_ostream &OS,
                             const AnalysisDumpOptions &DumpOptions)
    : NaClBitcodeParser(Cursor),
      IndentLevel(0),
      OS(OS),
      DumpOptions(DumpOptions) {
  }

  virtual ~PNaClBitcodeAnalyzerParser() {}

  virtual bool Error(const std::string Message) {
    // Use local error routine so that all errors are treated uniformly.
    return ::Error(Message);
  }

  virtual bool ParseBlock(unsigned BlockID);

  // Returns the string defining the indentation to use with respect
  // to the current indent level.
  const std::string &GetIndentation() {
    size_t Size = IndentationCache.size();
    if (IndentLevel >= Size) {
      IndentationCache.resize(IndentLevel+1);
      for (size_t i = Size; i <= IndentLevel; ++i) {
        IndentationCache[i] = std::string(i*2, ' ');
      }
    }
    return IndentationCache[IndentLevel];
  }

  // Keeps track of current indentation level based on block nesting.
  unsigned IndentLevel;
  // The output stream to print to.
  raw_ostream &OS;
  // The dump options to use.
  const AnalysisDumpOptions &DumpOptions;
  // The statistics collected for each block ID.
  std::map<unsigned, PerBlockIDStats*> BlockIDStats;

private:
  // The set of cached, indentation strings. Used for indenting
  // records when dumping.
  std::vector<std::string> IndentationCache;
};

// Parses a bitcode block, and collects distribution of records in that block.
// Also dumps bitcode structure if specified (via global variables).
class PNaClBitcodeAnalyzerBlockParser : public NaClBitcodeParser {
public:
  // Parses top-level block.
  PNaClBitcodeAnalyzerBlockParser(
      unsigned BlockID,
      PNaClBitcodeAnalyzerParser *Parser)
      : NaClBitcodeParser(BlockID, Parser) {
    Initialize(BlockID, Parser);
  }

  virtual ~PNaClBitcodeAnalyzerBlockParser() {}

protected:
  // Parses nested blocks.
  PNaClBitcodeAnalyzerBlockParser(
      unsigned BlockID,
      PNaClBitcodeAnalyzerBlockParser *EnclosingBlock)
      : NaClBitcodeParser(BlockID, EnclosingBlock),
        Context(EnclosingBlock->Context) {
    Initialize(BlockID, EnclosingBlock->Context);
  }

  // Initialize data associated with a block.
  void Initialize(unsigned BlockID, PNaClBitcodeAnalyzerParser *Parser) {
    Context = Parser;
    if (Context->DumpOptions.DoDump) {
      Indent = Parser->GetIndentation();
    }
    NumWords = 0;
    BlockName = 0;
    BlockStats = Context->BlockIDStats[BlockID];
    if (BlockStats == 0) {
      BlockStats = new PerBlockIDStats(BlockID);
      Context->BlockIDStats[BlockID] = BlockStats;
    }
    BlockStats->NumInstances++;
  }

  // Increment the indentation level for dumping.
  void IncrementIndent() {
    Context->IndentLevel++;
    Indent = Context->GetIndentation();
  }

  // Increment the indentation level for dumping.
  void DecrementIndent() {
    Context->IndentLevel--;
    Indent = Context->GetIndentation();
  }

  virtual bool Error(const std::string Message) {
    // Use local error routine so that all errors are treated uniformly.
    return ::Error(Message);
  }

  // Called once the block has been entered by the bitstream reader.
  // Argument NumWords is set to the number of words in the
  // corresponding block.
  virtual void EnterBlock(unsigned NumberWords) {
    NumWords = NumberWords;
    IncrementCallingBlock();
    BlockName = 0;
    if (Context->DumpOptions.DoDump) {
      raw_ostream &OS = Context->OS;
      unsigned BlockID = GetBlockID();
      OS << Indent << "<";
      if ((BlockName = GetBlockName(BlockID)))
        OS << BlockName;
      else
        OS << "UnknownBlock" << BlockID;

      if (Context->DumpOptions.NonSymbolic && BlockName)
        OS << " BlockID=" << BlockID;

      if (!Context->DumpOptions.DumpOnlyRecords) {
        OS << " NumWords=" << NumberWords
           << " BlockCodeSize="
           << Record.GetCursor().getAbbrevIDWidth();
      }
      OS << ">\n";
      IncrementIndent();
    }
  }

  // Called when the corresponding EndBlock of the block being parsed
  // is found.
  virtual void ExitBlock() {
    BlockStats->NumBits += GetLocalNumBits();
    if (Context->DumpOptions.DoDump) {
      DecrementIndent();
      raw_ostream &OS = Context->OS;
      OS << Indent << "</";
      if (BlockName)
        OS << BlockName << ">\n";
      else
        OS << "UnknownBlock" << GetBlockID() << ">\n";
    }
  }

  // Called after a BlockInfo block is parsed.
  virtual void ExitBlockInfo() {
    BlockStats->NumBits += GetLocalNumBits();
    if (Context->DumpOptions.DoDump)
      Context->OS << Indent << "<BLOCKINFO_BLOCK/>\n";
    IncrementCallingBlock();
  }

  // Process the last read record in the block.
  virtual void ProcessRecord() {
    ++BlockStats->NumRecords;
    unsigned Code = Record.GetCode();

    // Increment the # occurrences of this code.
    BlockStats->RecordCodeDist.Add(Record);

    if (Context->DumpOptions.DoDump) {
      raw_ostream &OS = Context->OS;
      std::string CodeName =
          NaClBitcodeRecordCodeDist::GetCodeName(Code, GetBlockID());
      OS << Indent << "<" << CodeName;
      if (Context->DumpOptions.NonSymbolic &&
          !NaClBitcodeRecordCodeDist::HasKnownCodeName(Code, GetBlockID()))
        OS << " codeid=" << Code;
      if (!Context->DumpOptions.DumpOnlyRecords &&
          Record.GetEntryID() != naclbitc::UNABBREV_RECORD)
        OS << " abbrevid=" << Record.GetEntryID();

      const NaClBitcodeRecord::RecordVector &Values = Record.GetValues();
      for (unsigned i = 0, e = Values.size(); i != e; ++i) {
        if (Context->DumpOptions.OpsPerLine
            && (i % Context->DumpOptions.OpsPerLine) == 0
            && i > 0) {
          OS << "\n" << Indent << " ";
          for (unsigned j = 0; j < CodeName.size(); ++j)
            OS << " ";
        }
        OS << " op" << i << "=" << (int64_t)Values[i];
      }

      OS << "/>\n";
    }
  }

  virtual bool ParseBlock(unsigned BlockID) {
    PNaClBitcodeAnalyzerBlockParser Parser(BlockID, this);
    return Parser.ParseThisBlock();
  }

  std::string Indent;
  unsigned NumWords;
  const char *BlockName;
  PerBlockIDStats *BlockStats;
  PNaClBitcodeAnalyzerParser *Context;

  void IncrementCallingBlock() {
    if (NaClBitcodeParser *Parser = GetEnclosingParser()) {
      PNaClBitcodeAnalyzerBlockParser *PNaClBlock =
          static_cast<PNaClBitcodeAnalyzerBlockParser*>(Parser);
      ++PNaClBlock->BlockStats->NumSubBlocks;
    }
  }
};

bool PNaClBitcodeAnalyzerParser::ParseBlock(unsigned BlockID) {
  PNaClBitcodeAnalyzerBlockParser Parser(BlockID, this);
  return Parser.ParseThisBlock();
}

static void PrintSize(double Bits, raw_ostream &OS) {
  OS << format("%.2f/%.2fB/%luW", Bits, Bits/8,(unsigned long)(Bits/32));
}
static void PrintSize(uint64_t Bits, raw_ostream &OS) {
  OS << format("%lub/%.2fB/%luW", (unsigned long)Bits,
               (double)Bits/8, (unsigned long)(Bits/32));
}

int AnalyzeBitcodeInBuffer(const MemoryBuffer &Buf, raw_ostream &OS,
                           const AnalysisDumpOptions &DumpOptions) {
  DEBUG(dbgs() << "-> AnalyzeBitcodeInBuffer\n");

  if (Buf.getBufferSize() & 3)
    return Error("Bitcode stream should be a multiple of 4 bytes in length");

  const unsigned char *BufPtr = (const unsigned char *)Buf.getBufferStart();
  const unsigned char *EndBufPtr = BufPtr+Buf.getBufferSize();

  NaClBitcodeHeader Header;
  if (Header.Read(BufPtr, EndBufPtr))
    return Error("Invalid PNaCl bitcode header");

  if (!Header.IsSupported())
    errs() << "Warning: " << Header.Unsupported() << "\n";

  if (!Header.IsReadable())
    Error("Bitcode file is not readable");

  NaClBitstreamReader StreamFile(BufPtr, EndBufPtr);
  NaClBitstreamCursor Stream(StreamFile);

  unsigned NumTopBlocks = 0;

  // Print out header information.
  for (size_t i = 0, limit = Header.NumberFields(); i < limit; ++i) {
    OS << Header.GetField(i)->Contents() << "\n";
  }
  if (Header.NumberFields()) OS << "\n";

  PNaClBitcodeAnalyzerParser Parser(Stream, OS, DumpOptions);
  // Parse the top-level structure.  We only allow blocks at the top-level.
  while (!Stream.AtEndOfStream()) {
    ++NumTopBlocks;
    if (Parser.Parse()) return 1;
  }

  if (DumpOptions.DoDump) OS << "\n\n";

  if (DumpOptions.DumpOnlyRecords) return 0;

  uint64_t BufferSizeBits = (EndBufPtr-BufPtr)*CHAR_BIT;
  // Print a summary
  OS << "  Total size: ";
  PrintSize(BufferSizeBits, OS);
  OS << "\n";
  OS << "  # Toplevel Blocks: " << NumTopBlocks << "\n";
  OS << "\n";

  // Emit per-block stats.
  OS << "Per-block Summary:\n";
  for (std::map<unsigned, PerBlockIDStats*>::iterator
           I = Parser.BlockIDStats.begin(),
           E = Parser.BlockIDStats.end();
       I != E; ++I) {
    OS << "  Block ID #" << I->first;
    if (const char *BlockName = GetBlockName(I->first))
      OS << " (" << BlockName << ")";
    OS << ":\n";

    const PerBlockIDStats &Stats = *I->second;
    OS << "      Num Instances: " << Stats.NumInstances << "\n";
    OS << "         Total Size: ";
    PrintSize(Stats.NumBits, OS);
    OS << "\n";
    double pct = (Stats.NumBits * 100.0) / BufferSizeBits;
    OS << "    Percent of file: " << format("%2.4f%%", pct) << "\n";
    if (Stats.NumInstances > 1) {
      OS << "       Average Size: ";
      PrintSize(Stats.NumBits/(double)Stats.NumInstances, OS);
      OS << "\n";
      OS << "  Tot/Avg SubBlocks: " << Stats.NumSubBlocks << "/"
         << Stats.NumSubBlocks/(double)Stats.NumInstances << "\n";
      OS << "    Tot/Avg Abbrevs: " << Stats.NumAbbrevs << "/"
         << Stats.NumAbbrevs/(double)Stats.NumInstances << "\n";
      OS << "    Tot/Avg Records: " << Stats.NumRecords << "/"
         << Stats.NumRecords/(double)Stats.NumInstances << "\n";
    } else {
      OS << "      Num SubBlocks: " << Stats.NumSubBlocks << "\n";
      OS << "        Num Abbrevs: " << Stats.NumAbbrevs << "\n";
      OS << "        Num Records: " << Stats.NumRecords << "\n";
    }
    if (Stats.NumRecords) {
      double pct = (Stats.NumAbbreviatedRecords * 100.0) / Stats.NumRecords;
      OS << "    Percent Abbrevs: " << format("%2.4f%%", pct) << "\n";
    }
    OS << "\n";

    // Print a histogram of the codes we see.
    if (!DumpOptions.NoHistogram && !Stats.RecordCodeDist.empty()) {
      Stats.RecordCodeDist.Print(OS, "    ");
      OS << "\n";
    }
  }
  DEBUG(dbgs() << "<- AnalyzeBitcode\n");
  return 0;
}

int AnalyzeBitcodeInFile(const StringRef &InputFilename, raw_ostream &OS,
                         const AnalysisDumpOptions &DumpOptions) {
  // Read the input file.
  OwningPtr<MemoryBuffer> MemBuf;

  if (error_code ec =
        MemoryBuffer::getFileOrSTDIN(InputFilename, MemBuf))
    return Error(Twine("Error reading '") + InputFilename + "': " +
                 ec.message());

  return AnalyzeBitcodeInBuffer(*MemBuf, OS, DumpOptions);
}

} // namespace llvm
