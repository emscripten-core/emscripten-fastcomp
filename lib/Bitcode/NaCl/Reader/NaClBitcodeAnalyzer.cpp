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

/// GetCodeName - Return a symbolic code name if known, otherwise return
/// null.
static const char *GetCodeName(unsigned CodeID, unsigned BlockID) {
  // Standard blocks for all bitcode files.
  if (BlockID < naclbitc::FIRST_APPLICATION_BLOCKID) {
    if (BlockID == naclbitc::BLOCKINFO_BLOCK_ID) {
      switch (CodeID) {
      default: return 0;
      case naclbitc::BLOCKINFO_CODE_SETBID:        return "SETBID";
      }
    }
    return 0;
  }

  switch (BlockID) {
  default: return 0;
  case naclbitc::MODULE_BLOCK_ID:
    switch (CodeID) {
    default: return 0;
    case naclbitc::MODULE_CODE_VERSION:     return "VERSION";
    case naclbitc::MODULE_CODE_TRIPLE:      return "TRIPLE";
    case naclbitc::MODULE_CODE_DATALAYOUT:  return "DATALAYOUT";
    case naclbitc::MODULE_CODE_ASM:         return "ASM";
    case naclbitc::MODULE_CODE_SECTIONNAME: return "SECTIONNAME";
    case naclbitc::MODULE_CODE_DEPLIB:      return "DEPLIB"; // FIXME: Remove in 4.0
    case naclbitc::MODULE_CODE_GLOBALVAR:   return "GLOBALVAR";
    case naclbitc::MODULE_CODE_FUNCTION:    return "FUNCTION";
    case naclbitc::MODULE_CODE_ALIAS:       return "ALIAS";
    case naclbitc::MODULE_CODE_PURGEVALS:   return "PURGEVALS";
    case naclbitc::MODULE_CODE_GCNAME:      return "GCNAME";
    }
  case naclbitc::PARAMATTR_BLOCK_ID:
    switch (CodeID) {
    default: return 0;
    case naclbitc::PARAMATTR_CODE_ENTRY_OLD: return "ENTRY";
    case naclbitc::PARAMATTR_CODE_ENTRY:     return "ENTRY";
    case naclbitc::PARAMATTR_GRP_CODE_ENTRY: return "ENTRY";
    }
  case naclbitc::TYPE_BLOCK_ID_NEW:
    switch (CodeID) {
    default: return 0;
    case naclbitc::TYPE_CODE_NUMENTRY:     return "NUMENTRY";
    case naclbitc::TYPE_CODE_VOID:         return "VOID";
    case naclbitc::TYPE_CODE_FLOAT:        return "FLOAT";
    case naclbitc::TYPE_CODE_DOUBLE:       return "DOUBLE";
    case naclbitc::TYPE_CODE_LABEL:        return "LABEL";
    case naclbitc::TYPE_CODE_OPAQUE:       return "OPAQUE";
    case naclbitc::TYPE_CODE_INTEGER:      return "INTEGER";
    case naclbitc::TYPE_CODE_POINTER:      return "POINTER";
    case naclbitc::TYPE_CODE_ARRAY:        return "ARRAY";
    case naclbitc::TYPE_CODE_VECTOR:       return "VECTOR";
    case naclbitc::TYPE_CODE_X86_FP80:     return "X86_FP80";
    case naclbitc::TYPE_CODE_FP128:        return "FP128";
    case naclbitc::TYPE_CODE_PPC_FP128:    return "PPC_FP128";
    case naclbitc::TYPE_CODE_METADATA:     return "METADATA";
    case naclbitc::TYPE_CODE_STRUCT_ANON:  return "STRUCT_ANON";
    case naclbitc::TYPE_CODE_STRUCT_NAME:  return "STRUCT_NAME";
    case naclbitc::TYPE_CODE_STRUCT_NAMED: return "STRUCT_NAMED";
    case naclbitc::TYPE_CODE_FUNCTION:     return "FUNCTION";
    }

  case naclbitc::CONSTANTS_BLOCK_ID:
    switch (CodeID) {
    default: return 0;
    case naclbitc::CST_CODE_SETTYPE:         return "SETTYPE";
    case naclbitc::CST_CODE_NULL:            return "NULL";
    case naclbitc::CST_CODE_UNDEF:           return "UNDEF";
    case naclbitc::CST_CODE_INTEGER:         return "INTEGER";
    case naclbitc::CST_CODE_WIDE_INTEGER:    return "WIDE_INTEGER";
    case naclbitc::CST_CODE_FLOAT:           return "FLOAT";
    case naclbitc::CST_CODE_AGGREGATE:       return "AGGREGATE";
    case naclbitc::CST_CODE_STRING:          return "STRING";
    case naclbitc::CST_CODE_CSTRING:         return "CSTRING";
    case naclbitc::CST_CODE_CE_BINOP:        return "CE_BINOP";
    case naclbitc::CST_CODE_CE_CAST:         return "CE_CAST";
    case naclbitc::CST_CODE_CE_GEP:          return "CE_GEP";
    case naclbitc::CST_CODE_CE_INBOUNDS_GEP: return "CE_INBOUNDS_GEP";
    case naclbitc::CST_CODE_CE_SELECT:       return "CE_SELECT";
    case naclbitc::CST_CODE_CE_EXTRACTELT:   return "CE_EXTRACTELT";
    case naclbitc::CST_CODE_CE_INSERTELT:    return "CE_INSERTELT";
    case naclbitc::CST_CODE_CE_SHUFFLEVEC:   return "CE_SHUFFLEVEC";
    case naclbitc::CST_CODE_CE_CMP:          return "CE_CMP";
    case naclbitc::CST_CODE_INLINEASM:       return "INLINEASM";
    case naclbitc::CST_CODE_CE_SHUFVEC_EX:   return "CE_SHUFVEC_EX";
    case naclbitc::CST_CODE_BLOCKADDRESS:    return "CST_CODE_BLOCKADDRESS";
    case naclbitc::CST_CODE_DATA:            return "DATA";
    }
  case naclbitc::FUNCTION_BLOCK_ID:
    switch (CodeID) {
    default: return 0;
    case naclbitc::FUNC_CODE_DECLAREBLOCKS: return "DECLAREBLOCKS";

    case naclbitc::FUNC_CODE_INST_BINOP:        return "INST_BINOP";
    case naclbitc::FUNC_CODE_INST_CAST:         return "INST_CAST";
    case naclbitc::FUNC_CODE_INST_GEP:          return "INST_GEP";
    case naclbitc::FUNC_CODE_INST_INBOUNDS_GEP: return "INST_INBOUNDS_GEP";
    case naclbitc::FUNC_CODE_INST_SELECT:       return "INST_SELECT";
    case naclbitc::FUNC_CODE_INST_EXTRACTELT:   return "INST_EXTRACTELT";
    case naclbitc::FUNC_CODE_INST_INSERTELT:    return "INST_INSERTELT";
    case naclbitc::FUNC_CODE_INST_SHUFFLEVEC:   return "INST_SHUFFLEVEC";
    case naclbitc::FUNC_CODE_INST_CMP:          return "INST_CMP";

    case naclbitc::FUNC_CODE_INST_RET:          return "INST_RET";
    case naclbitc::FUNC_CODE_INST_BR:           return "INST_BR";
    case naclbitc::FUNC_CODE_INST_SWITCH:       return "INST_SWITCH";
    case naclbitc::FUNC_CODE_INST_INVOKE:       return "INST_INVOKE";
    case naclbitc::FUNC_CODE_INST_UNREACHABLE:  return "INST_UNREACHABLE";

    case naclbitc::FUNC_CODE_INST_PHI:          return "INST_PHI";
    case naclbitc::FUNC_CODE_INST_ALLOCA:       return "INST_ALLOCA";
    case naclbitc::FUNC_CODE_INST_LOAD:         return "INST_LOAD";
    case naclbitc::FUNC_CODE_INST_VAARG:        return "INST_VAARG";
    case naclbitc::FUNC_CODE_INST_STORE:        return "INST_STORE";
    case naclbitc::FUNC_CODE_INST_EXTRACTVAL:   return "INST_EXTRACTVAL";
    case naclbitc::FUNC_CODE_INST_INSERTVAL:    return "INST_INSERTVAL";
    case naclbitc::FUNC_CODE_INST_CMP2:         return "INST_CMP2";
    case naclbitc::FUNC_CODE_INST_VSELECT:      return "INST_VSELECT";
    case naclbitc::FUNC_CODE_DEBUG_LOC_AGAIN:   return "DEBUG_LOC_AGAIN";
    case naclbitc::FUNC_CODE_INST_CALL:         return "INST_CALL";
    case naclbitc::FUNC_CODE_INST_CALL_INDIRECT: return "INST_CALL_INDIRECT";
    case naclbitc::FUNC_CODE_DEBUG_LOC:         return "DEBUG_LOC";
    case naclbitc::FUNC_CODE_INST_FORWARDTYPEREF: return "FORWARDTYPEREF";
    }
  case naclbitc::VALUE_SYMTAB_BLOCK_ID:
    switch (CodeID) {
    default: return 0;
    case naclbitc::VST_CODE_ENTRY: return "ENTRY";
    case naclbitc::VST_CODE_BBENTRY: return "BBENTRY";
    }
  case naclbitc::METADATA_ATTACHMENT_ID:
    switch(CodeID) {
    default:return 0;
    case naclbitc::METADATA_ATTACHMENT: return "METADATA_ATTACHMENT";
    }
  case naclbitc::METADATA_BLOCK_ID:
    switch(CodeID) {
    default:return 0;
    case naclbitc::METADATA_STRING:      return "METADATA_STRING";
    case naclbitc::METADATA_NAME:        return "METADATA_NAME";
    case naclbitc::METADATA_KIND:        return "METADATA_KIND";
    case naclbitc::METADATA_NODE:        return "METADATA_NODE";
    case naclbitc::METADATA_FN_NODE:     return "METADATA_FN_NODE";
    case naclbitc::METADATA_NAMED_NODE:  return "METADATA_NAMED_NODE";
    }
  case naclbitc::GLOBALVAR_BLOCK_ID:
    switch (CodeID) {
    default: return 0;
    case naclbitc::GLOBALVAR_VAR:        return "VAR";
    case naclbitc::GLOBALVAR_COMPOUND:   return "COMPOUND";
    case naclbitc::GLOBALVAR_ZEROFILL:   return "ZEROFILL";
    case naclbitc::GLOBALVAR_DATA:       return "DATA";
    case naclbitc::GLOBALVAR_RELOC:      return "RELOC";
    case naclbitc::GLOBALVAR_COUNT:      return "COUNT";
    }
  }
}

struct PerRecordStats {
  unsigned NumInstances;
  unsigned NumAbbrev;
  uint64_t TotalBits;

  PerRecordStats() : NumInstances(0), NumAbbrev(0), TotalBits(0) {}
};

struct PerBlockIDStats {
  /// NumInstances - This the number of times this block ID has been seen.
  unsigned NumInstances;

  /// NumBits - The total size in bits of all of these blocks.
  uint64_t NumBits;

  /// NumSubBlocks - The total number of blocks these blocks contain.
  unsigned NumSubBlocks;

  /// NumAbbrevs - The total number of abbreviations.
  unsigned NumAbbrevs;

  /// NumRecords - The total number of records these blocks contain, and the
  /// number that are abbreviated.
  unsigned NumRecords, NumAbbreviatedRecords;

  /// CodeFreq - Keep track of the number of times we see each code.
  std::vector<PerRecordStats> CodeFreq;

  PerBlockIDStats()
    : NumInstances(0), NumBits(0),
      NumSubBlocks(0), NumAbbrevs(0), NumRecords(0), NumAbbreviatedRecords(0) {}
};

/// Error - All bitcode analysis errors go through this function, making this a
/// good place to breakpoint if debugging.
static bool Error(const Twine &Err) {
  errs() << Err << "\n";
  return true;
}

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
  std::map<unsigned, PerBlockIDStats> BlockIDStats;

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
    BlockStats = &Context->BlockIDStats[BlockID];
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
    if (BlockStats->CodeFreq.size() <= Code)
      BlockStats->CodeFreq.resize(Code+1);
    BlockStats->CodeFreq[Code].NumInstances++;
    BlockStats->CodeFreq[Code].TotalBits += Record.GetNumBits();
    if (Record.GetEntryID() != naclbitc::UNABBREV_RECORD) {
      BlockStats->CodeFreq[Code].NumAbbrev++;
      ++BlockStats->NumAbbreviatedRecords;
    }

    if (Context->DumpOptions.DoDump) {
      raw_ostream &OS = Context->OS;
      OS << Indent << "<";
      const char *CodeName = GetCodeName(Code, GetBlockID());
      if (CodeName)
        OS << CodeName;
      else
        OS << "UnknownCode" << Code;
      if (Context->DumpOptions.NonSymbolic && CodeName)
        OS << " codeid=" << Code;
      if (!Context->DumpOptions.DumpOnlyRecords &&
          Record.GetEntryID() != naclbitc::UNABBREV_RECORD)
        OS << " abbrevid=" << Record.GetEntryID();

      const NaClBitcodeRecord::RecordVector &Values = Record.GetValues();
      for (unsigned i = 0, e = Values.size(); i != e; ++i) {
        if (Context->DumpOptions.OpsPerLine
            && (i % Context->DumpOptions.OpsPerLine) == 0
            && i > 0) {
          OS << "\n" << Indent << "   ";
          if (CodeName) {
            for (unsigned j = 0; j < strlen(CodeName); ++j)
              OS << " ";
          } else {
            OS << "   ";
          }
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
  for (std::map<unsigned, PerBlockIDStats>::iterator
           I = Parser.BlockIDStats.begin(),
           E = Parser.BlockIDStats.end();
       I != E; ++I) {
    OS << "  Block ID #" << I->first;
    if (const char *BlockName = GetBlockName(I->first))
      OS << " (" << BlockName << ")";
    OS << ":\n";

    const PerBlockIDStats &Stats = I->second;
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
    if (!DumpOptions.NoHistogram && !Stats.CodeFreq.empty()) {
      std::vector<std::pair<unsigned, unsigned> > FreqPairs;  // <freq,code>
      for (unsigned i = 0, e = Stats.CodeFreq.size(); i != e; ++i)
        if (unsigned Freq = Stats.CodeFreq[i].NumInstances)
          FreqPairs.push_back(std::make_pair(Freq, i));
      std::stable_sort(FreqPairs.begin(), FreqPairs.end());
      std::reverse(FreqPairs.begin(), FreqPairs.end());

      OS << "\tRecord Histogram:\n";
      OS << "\t\t  Count    # Bits %% Abv  Record Kind\n";
      for (unsigned i = 0, e = FreqPairs.size(); i != e; ++i) {
        const PerRecordStats &RecStats = Stats.CodeFreq[FreqPairs[i].second];

        OS << format("\t\t%7d %9lu", RecStats.NumInstances,
                     (unsigned long)RecStats.TotalBits);

        if (RecStats.NumAbbrev)
          OS << format("%7.2f  ",
                       (double)RecStats.NumAbbrev/RecStats.NumInstances*100);
        else
          OS << "         ";

        if (const char *CodeName = GetCodeName(FreqPairs[i].second, I->first))
          OS << CodeName << "\n";
        else
          OS << "UnknownCode" << FreqPairs[i].second << "\n";
      }
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
