//===-- pnacl-bcanalyzer.cpp - Bitcode Analyzer -------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This tool may be invoked in the following manner:
//  pnacl-bcanalyzer [options]      - Read frozen PNaCl bitcode from stdin
//  pnacl-bcanalyzer [options] x.bc - Read frozen PNaCl bitcode from the x.bc
//                                   file
//
//  Options:
//      --help      - Output information about command line switches
//      --dump      - Dump low-level bitcode structure in readable format
//
// This tool provides analytical information about a bitcode file. It is
// intended as an aid to developers of bitcode reading and writing software. It
// produces on std::out a summary of the bitcode file that shows various
// statistics about the contents of the file. By default this information is
// detailed and contains information about individual bitcode blocks and the
// functions in the module.
// The tool is also able to print a bitcode file in a straight forward text
// format that shows the containment and relationships of the information in
// the bitcode file (-dump option).
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "pnacl-bcanalyzer"

#include "llvm/ADT/OwningPtr.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeHeader.h"
#include "llvm/Bitcode/NaCl/NaClBitstreamReader.h"
#include "llvm/Bitcode/NaCl/NaClLLVMBitCodes.h"
#include "llvm/Bitcode/NaCl/NaClReaderWriter.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/system_error.h"
#include <algorithm>
#include <map>
using namespace llvm;

static cl::opt<std::string>
  InputFilename(cl::Positional, cl::desc("<input bitcode>"), cl::init("-"));

static cl::opt<bool> Dump("dump", cl::desc("Dump low level bitcode trace"));

static cl::opt<unsigned> OpsPerLine(
    "operands-per-line",
    cl::desc("Number of operands to print per dump line. 0 implies "
             "all operands will be printed on the same line (default)"),
    cl::init(0));

//===----------------------------------------------------------------------===//
// Bitcode specific analysis.
//===----------------------------------------------------------------------===//

static cl::opt<bool> NoHistogram("disable-histogram",
                                 cl::desc("Do not print per-code histogram"));

static cl::opt<bool>
NonSymbolic("non-symbolic",
            cl::desc("Emit numeric info in dump even if"
                     " symbolic info is available"));


/// GetBlockName - Return a symbolic block name if known, otherwise return
/// null.
static const char *GetBlockName(unsigned BlockID,
                                const NaClBitstreamReader &StreamFile) {
  // Standard blocks for all bitcode files.
  if (BlockID < naclbitc::FIRST_APPLICATION_BLOCKID) {
    if (BlockID == naclbitc::BLOCKINFO_BLOCK_ID)
      return "BLOCKINFO_BLOCK";
    return 0;
  }

  // Check to see if we have a blockinfo record for this block, with a name.
  if (const NaClBitstreamReader::BlockInfo *Info =
        StreamFile.getBlockInfo(BlockID)) {
    if (!Info->Name.empty())
      return Info->Name.c_str();
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
static const char *GetCodeName(unsigned CodeID, unsigned BlockID,
                               const NaClBitstreamReader &StreamFile) {
  // Standard blocks for all bitcode files.
  if (BlockID < naclbitc::FIRST_APPLICATION_BLOCKID) {
    if (BlockID == naclbitc::BLOCKINFO_BLOCK_ID) {
      switch (CodeID) {
      default: return 0;
      case naclbitc::BLOCKINFO_CODE_SETBID:        return "SETBID";
      case naclbitc::BLOCKINFO_CODE_BLOCKNAME:     return "BLOCKNAME";
      case naclbitc::BLOCKINFO_CODE_SETRECORDNAME: return "SETRECORDNAME";
      }
    }
    return 0;
  }

  // Check to see if we have a blockinfo record for this record, with a name.
  if (const NaClBitstreamReader::BlockInfo *Info =
        StreamFile.getBlockInfo(BlockID)) {
    for (unsigned i = 0, e = Info->RecordNames.size(); i != e; ++i)
      if (Info->RecordNames[i].first == CodeID)
        return Info->RecordNames[i].second.c_str();
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
  case naclbitc::USELIST_BLOCK_ID:
    switch(CodeID) {
    default:return 0;
    case naclbitc::USELIST_CODE_ENTRY:   return "USELIST_CODE_ENTRY";
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

static std::map<unsigned, PerBlockIDStats> BlockIDStats;



/// Error - All bitcode analysis errors go through this function, making this a
/// good place to breakpoint if debugging.
static bool Error(const std::string &Err) {
  errs() << Err << "\n";
  return true;
}

/// ParseBlock - Read a block, updating statistics, etc.
static bool ParseBlock(NaClBitstreamCursor &Stream, unsigned BlockID,
                       unsigned IndentLevel) {
  std::string Indent(IndentLevel*2, ' ');
  DEBUG(dbgs() << Indent << "-> ParseBlock(" << BlockID << ")\n");
  uint64_t BlockBitStart = Stream.GetCurrentBitNo();

  // Get the statistics for this BlockID.
  PerBlockIDStats &BlockStats = BlockIDStats[BlockID];

  BlockStats.NumInstances++;

  // BLOCKINFO is a special part of the stream.
  if (BlockID == naclbitc::BLOCKINFO_BLOCK_ID) {
    if (Dump) outs() << Indent << "<BLOCKINFO_BLOCK/>\n";
    if (Stream.ReadBlockInfoBlock())
      return Error("Malformed BlockInfoBlock");
    uint64_t BlockBitEnd = Stream.GetCurrentBitNo();
    BlockStats.NumBits += BlockBitEnd-BlockBitStart;
    DEBUG(dbgs() << Indent << "<- ParseBlock\n");
    return false;
  }

  unsigned NumWords = 0;
  if (Stream.EnterSubBlock(BlockID, &NumWords))
    return Error("Malformed block record");

  const char *BlockName = 0;
  if (Dump) {
    outs() << Indent << "<";
    if ((BlockName = GetBlockName(BlockID, *Stream.getBitStreamReader())))
      outs() << BlockName;
    else
      outs() << "UnknownBlock" << BlockID;

    if (NonSymbolic && BlockName)
      outs() << " BlockID=" << BlockID;

    outs() << " NumWords=" << NumWords
           << " BlockCodeSize=" << Stream.getAbbrevIDWidth() << ">\n";
  }

  SmallVector<uint64_t, 64> Record;

  // Read all the records for this block.
  while (1) {
    if (Stream.AtEndOfStream())
      return Error("Premature end of bitstream");

    uint64_t RecordStartBit = Stream.GetCurrentBitNo();

    NaClBitstreamEntry Entry =
      Stream.advance(NaClBitstreamCursor::AF_DontAutoprocessAbbrevs);
    
    switch (Entry.Kind) {
    case NaClBitstreamEntry::Error:
      return Error("malformed bitcode file");
    case NaClBitstreamEntry::EndBlock: {
      uint64_t BlockBitEnd = Stream.GetCurrentBitNo();
      BlockStats.NumBits += BlockBitEnd-BlockBitStart;
      if (Dump) {
        outs() << Indent << "</";
        if (BlockName)
          outs() << BlockName << ">\n";
        else
          outs() << "UnknownBlock" << BlockID << ">\n";
      }
      DEBUG(dbgs() << Indent << "<- ParseBlock\n");
      return false;
    }
        
    case NaClBitstreamEntry::SubBlock: {
      uint64_t SubBlockBitStart = Stream.GetCurrentBitNo();
      if (ParseBlock(Stream, Entry.ID, IndentLevel+1))
        return true;
      ++BlockStats.NumSubBlocks;
      uint64_t SubBlockBitEnd = Stream.GetCurrentBitNo();
      
      // Don't include subblock sizes in the size of this block.
      BlockBitStart += SubBlockBitEnd-SubBlockBitStart;
      continue;
    }
    case NaClBitstreamEntry::Record:
      // The interesting case.
      break;
    }

    if (Entry.ID == naclbitc::DEFINE_ABBREV) {
      Stream.ReadAbbrevRecord();
      ++BlockStats.NumAbbrevs;
      continue;
    }
    
    Record.clear();

    ++BlockStats.NumRecords;

    StringRef Blob;
    unsigned Code = Stream.readRecord(Entry.ID, Record, &Blob);

    // Increment the # occurrences of this code.
    if (BlockStats.CodeFreq.size() <= Code)
      BlockStats.CodeFreq.resize(Code+1);
    BlockStats.CodeFreq[Code].NumInstances++;
    BlockStats.CodeFreq[Code].TotalBits +=
      Stream.GetCurrentBitNo()-RecordStartBit;
    if (Entry.ID != naclbitc::UNABBREV_RECORD) {
      BlockStats.CodeFreq[Code].NumAbbrev++;
      ++BlockStats.NumAbbreviatedRecords;
    }

    if (Dump) {
      outs() << Indent << "  <";
      const char *CodeName =
          GetCodeName(Code, BlockID, *Stream.getBitStreamReader());
      if (CodeName)
        outs() << CodeName;
      else
        outs() << "UnknownCode" << Code;
      if (NonSymbolic && CodeName)
        outs() << " codeid=" << Code;
      if (Entry.ID != naclbitc::UNABBREV_RECORD)
        outs() << " abbrevid=" << Entry.ID;

      for (unsigned i = 0, e = Record.size(); i != e; ++i) {
        if (OpsPerLine && (i % OpsPerLine) == 0 && i > 0) {
          outs() << "\n" << Indent << "   ";
          if (CodeName) {
            for (unsigned j = 0; j < strlen(CodeName); ++j)
              outs() << " ";
          } else {
            outs() << "   ";
          }
        }
        outs() << " op" << i << "=" << (int64_t)Record[i];
      }

      outs() << "/>";

      if (Blob.data()) {
        outs() << " blob data = ";
        bool BlobIsPrintable = true;
        for (unsigned i = 0, e = Blob.size(); i != e; ++i)
          if (!isprint(static_cast<unsigned char>(Blob[i]))) {
            BlobIsPrintable = false;
            break;
          }

        if (BlobIsPrintable)
          outs() << "'" << Blob << "'";
        else
          outs() << "unprintable, " << Blob.size() << " bytes.";
      }

      outs() << "\n";
    }
  }
}

static void PrintSize(double Bits) {
  outs() << format("%.2f/%.2fB/%luW", Bits, Bits/8,(unsigned long)(Bits/32));
}
static void PrintSize(uint64_t Bits) {
  outs() << format("%lub/%.2fB/%luW", (unsigned long)Bits,
                   (double)Bits/8, (unsigned long)(Bits/32));
}


/// AnalyzeBitcode - Analyze the bitcode file specified by InputFilename.
static int AnalyzeBitcode() {
  DEBUG(dbgs() << "-> AnalyzeBitcode\n");
  // Read the input file.
  OwningPtr<MemoryBuffer> MemBuf;

  if (error_code ec =
        MemoryBuffer::getFileOrSTDIN(InputFilename.c_str(), MemBuf))
    return Error("Error reading '" + InputFilename + "': " + ec.message());

  if (MemBuf->getBufferSize() & 3)
    return Error("Bitcode stream should be a multiple of 4 bytes in length");

  const unsigned char *BufPtr = (const unsigned char *)MemBuf->getBufferStart();
  const unsigned char *EndBufPtr = BufPtr+MemBuf->getBufferSize();

  NaClBitcodeHeader Header;
  if (Header.Read(BufPtr, EndBufPtr))
    return Error("Invalid PNaCl bitcode header");

  if (!Header.IsSupported())
    errs() << "Warning: " << Header.Unsupported() << "\n";

  if (!Header.IsReadable())
    Error("Bitcode file is not readable");

  NaClBitstreamReader StreamFile(BufPtr, EndBufPtr);
  NaClBitstreamCursor Stream(StreamFile);
  StreamFile.CollectBlockInfoNames();

  unsigned NumTopBlocks = 0;

  // Print out header information.
  for (size_t i = 0, limit = Header.NumberFields(); i < limit; ++i) {
    outs() << Header.GetField(i)->Contents() << "\n";
  }
  if (Header.NumberFields()) outs() << "\n";

  // Parse the top-level structure.  We only allow blocks at the top-level.
  while (!Stream.AtEndOfStream()) {
    unsigned Code = Stream.ReadCode();
    if (Code != naclbitc::ENTER_SUBBLOCK)
      return Error("Invalid record at top-level");

    unsigned BlockID = Stream.ReadSubBlockID();

    if (ParseBlock(Stream, BlockID, 0))
      return true;
    ++NumTopBlocks;
  }

  if (Dump) outs() << "\n\n";

  uint64_t BufferSizeBits = (EndBufPtr-BufPtr)*CHAR_BIT;
  // Print a summary of the read file.
  outs() << "Summary of " << InputFilename << ":\n";
  outs() << "  Total size: ";
  PrintSize(BufferSizeBits);
  outs() << "\n";
  outs() << "  # Toplevel Blocks: " << NumTopBlocks << "\n";
  outs() << "\n";

  // Emit per-block stats.
  outs() << "Per-block Summary:\n";
  for (std::map<unsigned, PerBlockIDStats>::iterator I = BlockIDStats.begin(),
       E = BlockIDStats.end(); I != E; ++I) {
    outs() << "  Block ID #" << I->first;
    if (const char *BlockName = GetBlockName(I->first, StreamFile))
      outs() << " (" << BlockName << ")";
    outs() << ":\n";

    const PerBlockIDStats &Stats = I->second;
    outs() << "      Num Instances: " << Stats.NumInstances << "\n";
    outs() << "         Total Size: ";
    PrintSize(Stats.NumBits);
    outs() << "\n";
    double pct = (Stats.NumBits * 100.0) / BufferSizeBits;
    outs() << "    Percent of file: " << format("%2.4f%%", pct) << "\n";
    if (Stats.NumInstances > 1) {
      outs() << "       Average Size: ";
      PrintSize(Stats.NumBits/(double)Stats.NumInstances);
      outs() << "\n";
      outs() << "  Tot/Avg SubBlocks: " << Stats.NumSubBlocks << "/"
             << Stats.NumSubBlocks/(double)Stats.NumInstances << "\n";
      outs() << "    Tot/Avg Abbrevs: " << Stats.NumAbbrevs << "/"
             << Stats.NumAbbrevs/(double)Stats.NumInstances << "\n";
      outs() << "    Tot/Avg Records: " << Stats.NumRecords << "/"
             << Stats.NumRecords/(double)Stats.NumInstances << "\n";
    } else {
      outs() << "      Num SubBlocks: " << Stats.NumSubBlocks << "\n";
      outs() << "        Num Abbrevs: " << Stats.NumAbbrevs << "\n";
      outs() << "        Num Records: " << Stats.NumRecords << "\n";
    }
    if (Stats.NumRecords) {
      double pct = (Stats.NumAbbreviatedRecords * 100.0) / Stats.NumRecords;
      outs() << "    Percent Abbrevs: " << format("%2.4f%%", pct) << "\n";
    }
    outs() << "\n";

    // Print a histogram of the codes we see.
    if (!NoHistogram && !Stats.CodeFreq.empty()) {
      std::vector<std::pair<unsigned, unsigned> > FreqPairs;  // <freq,code>
      for (unsigned i = 0, e = Stats.CodeFreq.size(); i != e; ++i)
        if (unsigned Freq = Stats.CodeFreq[i].NumInstances)
          FreqPairs.push_back(std::make_pair(Freq, i));
      std::stable_sort(FreqPairs.begin(), FreqPairs.end());
      std::reverse(FreqPairs.begin(), FreqPairs.end());

      outs() << "\tRecord Histogram:\n";
      outs() << "\t\t  Count    # Bits %% Abv  Record Kind\n";
      for (unsigned i = 0, e = FreqPairs.size(); i != e; ++i) {
        const PerRecordStats &RecStats = Stats.CodeFreq[FreqPairs[i].second];

        outs() << format("\t\t%7d %9lu",
                         RecStats.NumInstances,
                         (unsigned long)RecStats.TotalBits);

        if (RecStats.NumAbbrev)
          outs() <<
              format("%7.2f  ",
                     (double)RecStats.NumAbbrev/RecStats.NumInstances*100);
        else
          outs() << "         ";

        if (const char *CodeName =
              GetCodeName(FreqPairs[i].second, I->first, StreamFile))
          outs() << CodeName << "\n";
        else
          outs() << "UnknownCode" << FreqPairs[i].second << "\n";
      }
      outs() << "\n";

    }
  }
  DEBUG(dbgs() << "<- AnalyzeBitcode\n");
  return 0;
}


int main(int argc, char **argv) {
  // Print a stack trace if we signal out.
  sys::PrintStackTraceOnErrorSignal();
  PrettyStackTraceProgram X(argc, argv);
  llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.
  cl::ParseCommandLineOptions(argc, argv, "pnacl-bcanalyzer file analyzer\n");

  return AnalyzeBitcode();
}
