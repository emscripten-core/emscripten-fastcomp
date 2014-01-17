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
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Bitcode/NaCl/NaClAnalyzerBlockDist.h"
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

// TODO(kschimpf): Separate out into two bitcode parsers, one for
// dumping records, and one for collecting distribution stats for
// printing. This should simplify the code.

/// Error - All bitcode analysis errors go through this function, making this a
/// good place to breakpoint if debugging.
static bool Error(const llvm::Twine &Err) {
  llvm::errs() << Err << "\n";
  return true;
}

namespace llvm {

// Parses all bitcode blocks, and collects distribution of records in
// each block.  Also dumps bitcode structure if specified (via global
// variables).
class PNaClBitcodeAnalyzerParser : public NaClBitcodeParser {
public:
  PNaClBitcodeAnalyzerParser(NaClBitstreamCursor &Cursor,
                             raw_ostream &OS,
                             const AnalysisDumpOptions &DumpOptions,
                             NaClBitcodeDist *Dist)
    : NaClBitcodeParser(Cursor),
      IndentLevel(0),
      OS(OS),
      DumpOptions(DumpOptions),
      Dist(Dist)
  {
  }

  virtual ~PNaClBitcodeAnalyzerParser() {}

  virtual bool Error(const std::string &Message) {
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
  // The bitcode distribution map (if defined) to update.
  NaClBitcodeDist *Dist;

private:
  // The set of cached, indentation strings. Used for indenting
  // records when dumping.
  std::vector<std::string> IndentationCache;
};

// Define the encodings for abbreviation operands that we recognize,
// based on the NaClBitCodeAbbrevOp::Encoding value.
static const char *AbbrevEncodings[] = {
  0,        // No encoding defined for 0.
  "FIXED",
  "VBR",
  "ARRAY",
  "CHAR6"
};

static std::string GetAbbrevEncoding(unsigned Encoding) {
  if (Encoding < array_lengthof(AbbrevEncodings) && AbbrevEncodings[Encoding]) {
    return AbbrevEncodings[Encoding];
  } else {
    std::string Str;
    raw_string_ostream StrStrm(Str);
    StrStrm << "UnknownEncoding" << Encoding;
    return StrStrm.str();
  }
}

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

  virtual ~PNaClBitcodeAnalyzerBlockParser() {
    if (Context->Dist) Context->Dist->AddBlock(GetBlock());
  }

  // *****************************************************
  // This subsection Defines an XML generator for the dump.
  // Tag. TagName, Attribute
  // *****************************************************

private:
  // The tag name for an element.
  std::string TagName;
  // The number of attributes associated with a tag.
  unsigned NumTagAttributes;
  // The number of (indexed attribute) operands associated with a tag.
  unsigned NumTagOperands;

protected:

  /// Initializes internal data used to emit an XML tag.
  void InitializeEmitTag() {
    TagName.clear();
    NumTagAttributes = 0;
    NumTagOperands = 0;
  }

  /// Emits the start of an XML start tag.
  void EmitBeginStartTag() {
    InitializeEmitTag();
    Context->OS << Indent << "<";
  }

  /// Emit the start of an XML end tag.
  void EmitBeginEndTag() {
    InitializeEmitTag();
    Context->OS << Indent << "</";
  }

  /// Emits the end of an empty-element XML tag.
  void EmitEndTag() {
    Context->OS << "/>\n";
  }

  /// Emits the End of a start/end tag for an XML element.
  void EmitEndElementTag() {
    Context->OS << ">\n";
  }

  /// Emits the tag name for an XML tag.
  void EmitTagName(const std::string &ElmtName) {
    TagName = ElmtName;
    Context->OS << ElmtName;
  }

  /// Emits the "name=" portion of an XML tag attribute.
  raw_ostream &EmitAttributePrefix(const std::string &AttributeName) {
    WrapOperandsLine();
    Context->OS << " " << AttributeName << "=";
    ++NumTagAttributes;
    return Context->OS;
  }

  /// Emits a string-valued XML attribute of an XML tag.
  void EmitStringAttribute(const char *AttributeName, const std::string &Str) {
    EmitAttributePrefix(AttributeName) << "'" << Str << "'";
  }

  /// Emits a string-valued XML attribute of an XML tag.
  void EmitStringAttribute(const char *AttributeName, const char*Str) {
    std::string StrStr(Str);
    EmitStringAttribute(AttributeName, StrStr);
  }

  /// Emits an unsigned integer-valued XML attribute of an XML tag.
  void EmitAttribute(const char *AttributeName, uint64_t Value) {
    EmitAttributePrefix(AttributeName) << Value;
  }

  /// Emits the "opN=" portion of an XML tag (indexable) operand attribute.
  raw_ostream &EmitOperandPrefix() {
    std::string OpName;
    raw_string_ostream OpNameStrm(OpName);
    OpNameStrm << "op" << NumTagOperands;
    ++NumTagOperands;
    return EmitAttributePrefix(OpNameStrm.str());
  }

  /// Adds line wrap if more than "OpsPerLine" XML tag attributes are
  /// emitted on the current line.
  void WrapOperandsLine() {
    if (Context->DumpOptions.OpsPerLine) {
      if (NumTagAttributes &&
          (NumTagAttributes % Context->DumpOptions.OpsPerLine) == 0) {
        raw_ostream &OS = Context->OS;
        // Last operand crossed width boundary, add newline and indent.
        OS << "\n" << Indent << " ";
        for (unsigned J = 0, SZ = TagName.size(); J < SZ; ++J)
          OS << " ";
      }
    }
  }

  // ********************************************************
  // This section defines how to parse the block and generate
  // the corresponding XML.
  // ********************************************************

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
    InitializeEmitTag();
    Context = Parser;
    if (Context->DumpOptions.DumpRecords) {
      Indent = Parser->GetIndentation();
    }
    NumWords = 0;
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

  virtual bool Error(const std::string &Message) {
    // Use local error routine so that all errors are treated uniformly.
    return ::Error(Message);
  }

  // Called once the block has been entered by the bitstream reader.
  // Argument NumWords is set to the number of words in the
  // corresponding block.
  virtual void EnterBlock(unsigned NumberWords) {
    NumWords = NumberWords;
    if (Context->DumpOptions.DumpRecords) {
      unsigned BlockID = GetBlockID();
      EmitBeginStartTag();
      EmitEnterBlockTagName(BlockID);
      if (Context->DumpOptions.DumpDetails) {
        EmitAttribute("NumWords", NumWords);
        EmitAttribute("BlockCodeSize", Record.GetCursor().getAbbrevIDWidth());
      }
      EmitEndElementTag();
      IncrementIndent();
    }
  }

  // Called when the corresponding EndBlock of the block being parsed
  // is found.
  virtual void ExitBlock() {
    if (Context->DumpOptions.DumpRecords) {
      DecrementIndent();
      EmitBeginEndTag();
      EmitExitBlockTagName(Record.GetBlockID());
      EmitEndElementTag();
    }
  }

  // Called after a BlockInfo block is parsed.
  virtual void ExitBlockInfo() {
    if (Context->DumpOptions.DumpRecords) {
      EmitBeginStartTag();
      EmitEnterBlockTagName(naclbitc::BLOCKINFO_BLOCK_ID);
      if (Context->DumpOptions.DumpDetails) {
        /// Long form. Fill out block with abbreviations read.
        EmitEndElementTag();
        IncrementIndent();
        EmitBlockInfoAbbreviations();
        DecrementIndent();
        EmitBeginEndTag();
        EmitExitBlockTagName(naclbitc::BLOCKINFO_BLOCK_ID);
        EmitEndTag();
      } else {
        EmitEndTag();
      }
    }
  }

  // Process the abbreviation just read.
  virtual void ProcessRecordAbbrev() {
    if (Context->DumpOptions.DumpDetails) {
      EmitAbbreviation(Record.GetCursor().GetNewestAbbrev());
    }
  }

  // Process the last read record in the block.
  virtual void ProcessRecord() {
    // Increment the # occurrences of this code.
    if (Context->Dist) Context->Dist->AddRecord(Record);

    if (Context->DumpOptions.DumpRecords) {
      EmitBeginStartTag();
      EmitCodeTagName(Record.GetCode(), GetBlockID(), Record.GetEntryID());
      const NaClBitcodeRecord::RecordVector &Values = Record.GetValues();
      for (unsigned i = 0, e = Values.size(); i != e; ++i) {
        EmitOperandPrefix() << (int64_t)Values[i];
      }
      EmitEndTag();
    }
  }

  virtual bool ParseBlock(unsigned BlockID) {
    PNaClBitcodeAnalyzerBlockParser Parser(BlockID, this);
    return Parser.ParseThisBlock();
  }

private:
  /// Defines the indent level of the block being parsed.
  std::string Indent;
  /// Defines the number of (32-bit) words the block occupies in
  /// the bitstream.
  unsigned NumWords;
  /// Refers to global parsing context.
  PNaClBitcodeAnalyzerParser *Context;

protected:

  /// Emit the given abbreviation as an XML tag.
  void EmitAbbreviation(const NaClBitCodeAbbrev *Abbrev) {
    EmitBeginStartTag();
    EmitTagName("DEFINE_ABBREV");
    if (Context->DumpOptions.DumpDetails) {
      EmitStringAttribute("abbrev", "DEFINE_ABBREV");
    }
    for (unsigned I = 0, IEnd = Abbrev->getNumOperandInfos(); I != IEnd; ++I) {
      EmitAbbreviationOp(Abbrev->getOperandInfo(I));
    }
    EmitEndTag();
  }

  /// Emit the given abbreviation operand as an XML tag attribute.
  void EmitAbbreviationOp(const NaClBitCodeAbbrevOp &Op) {
    if (Op.isLiteral()) {
      EmitOperandPrefix() << "'LIT(" << Op.getLiteralValue() << ")'";
    } else {
      EmitOperandPrefix() << "'" << GetAbbrevEncoding(Op.getEncoding());
      if (Op.hasEncodingData()) {
        Context->OS << "(" << Op.getEncodingData() << ")";
      }
      Context->OS << "'";
    }
  }

  /// Emit the abbreviations that were read when the BlockInfo block
  /// was parsed.
  void EmitBlockInfoAbbreviations() {
    SmallVector<unsigned, 10> BlockIDs;
    NaClBitstreamReader &Reader = Record.GetReader();
    Reader.GetBlockInfoBlockIDs(BlockIDs);
    for (size_t I=0, IEnd = BlockIDs.size(); I < IEnd; ++I) {
      unsigned BlockID = BlockIDs[I];
      if (const NaClBitstreamReader::BlockInfo *Info =
          Reader.getBlockInfo(BlockID)) {
        EmitBeginStartTag();
        EmitCodeTagName(naclbitc::BLOCKINFO_CODE_SETBID,
                        naclbitc::BLOCKINFO_BLOCK_ID);
        EmitStringAttribute("block",
                            NaClBitcodeBlockDist::GetName(Info->BlockID));
        EmitEndTag();
        for (std::vector<NaClBitCodeAbbrev*>::const_iterator
                 AbbrevIter = Info->Abbrevs.begin(),
                 AbbrevIterEnd = Info->Abbrevs.end();
             AbbrevIter != AbbrevIterEnd; ++AbbrevIter) {
          EmitAbbreviation(*AbbrevIter);
        }
      }
    }
  }

  /// Emits the symbolic name of the record code as the XML tag name.
  void EmitCodeTagName(
      unsigned CodeID, unsigned BlockID,
      unsigned AbbreviationID = naclbitc::UNABBREV_RECORD) {
    EmitTagName(NaClBitcodeCodeDist::GetCodeName(CodeID, BlockID));
    if (Context->DumpOptions.DumpDetails) {
      if (AbbreviationID == naclbitc::UNABBREV_RECORD) {
        EmitStringAttribute("abbrev", "UNABBREVIATED");
      } else {
        EmitAttribute("abbrev", AbbreviationID);
      }
    }
  }

  /// Emits the symbolic name of the block as the XML tag name.
  void EmitEnterBlockTagName(unsigned BlockID) {
    EmitTagName(NaClBitcodeBlockDist::GetName(BlockID));
    if (Context->DumpOptions.DumpDetails)
      EmitStringAttribute("abbrev", "ENTER_SUBBLOCK");
  }

  /// Emits the symbolic name of the block as the the XML tag name.
  void EmitExitBlockTagName(unsigned BlockID) {
    EmitTagName(NaClBitcodeBlockDist::GetName(BlockID));
    if (Context->DumpOptions.DumpDetails)
      EmitStringAttribute("abbrev", "END_BLOCK");
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

  // Parse the top-level structure.  We only allow blocks at the top-level.
  NaClAnalyzerBlockDistElement DistSentinel(0, DumpOptions.OrderBlocksByID);
  NaClAnalyzerBlockDist Dist(DistSentinel);
  PNaClBitcodeAnalyzerParser Parser(Stream, OS, DumpOptions, &Dist);
  while (!Stream.AtEndOfStream()) {
    ++NumTopBlocks;
    if (Parser.Parse()) return 1;
  }

  if (DumpOptions.DumpRecords) return 0;

  uint64_t BufferSizeBits = (EndBufPtr-BufPtr)*CHAR_BIT;
  // Print a summary
  OS << "Total size: ";
  PrintSize(BufferSizeBits, OS);
  OS << "\n";
  OS << "# Toplevel Blocks: " << NumTopBlocks << "\n";
  OS << "\n";

  if (Parser.Dist) Parser.Dist->Print(OS);

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
