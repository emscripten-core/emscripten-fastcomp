//===-- NaClObjDumpStream.cpp --------------------------------------------===//
//      Implements an objdump stream (bitcode records/assembly code).
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/NaCl/NaClObjDumpStream.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Format.h"

#include <inttypes.h>

namespace llvm {
namespace naclbitc {

TextFormatter::TextFormatter(raw_ostream &BaseStream,
                             unsigned LineWidth,
                             const char *Tab)
    : TextIndenter(Tab),
      BaseStream(BaseStream),
      TextStream(TextBuffer),
      LineWidth(LineWidth),
      LinePosition(0),
      CurrentIndent(0),
      MinLineWidth(20),
      AtInstructionBeginning(true),
      LineIndent(),
      ContinuationIndent(),
      InsideCluster(false) {
  if (MinLineWidth > LineWidth) MinLineWidth = LineWidth;
}

void TextFormatter::WriteEndline() {
  assert(!InsideCluster && "Must close clustering before ending instruction");
  WriteToken();
  Write('\n');
  CurrentIndent = 0;
  AtInstructionBeginning = true;
  LineIndent.clear();
}

void TextFormatter::Write(char ch) {
  switch (ch) {
  case '\n':
    BaseStream << ch;
    LinePosition = 0;
    break;
  case '\t': {
    size_t TabWidth = GetTabSize();
    size_t NumChars = LinePosition % TabWidth;
    if (NumChars == 0) NumChars = TabWidth;
    for (size_t i = 0; i < NumChars; ++i) Write(' ');
    break;
  }
  default:
    if (LinePosition == 0) {
      WriteLineIndents();
    }
    BaseStream << ch;
    ++LinePosition;
  }
  AtInstructionBeginning = false;
}

void TextFormatter::Write(const std::string &Text) {
  if (InsideCluster) {
    ClusteredText.push_back(Text);
  } else {
    for (std::string::const_iterator
             Iter = Text.begin(), IterEnd = Text.end();
         Iter != IterEnd; ++Iter) {
      Write(*Iter);
    }
  }
}

void TextFormatter::StartClustering() {
  assert(!InsideCluster && "Clustering can't be nested!");
  WriteToken();
  InsideCluster = true;
}

void TextFormatter::FinishClustering() {
  assert(InsideCluster && "Can't finish clustering, not in cluster!");
  // Start by flushing previous token.
  WriteToken();
  InsideCluster = false;

  // Compute width of cluster, and line wrap if it doesn't fit on the
  // current line.
  size_t Size = 0;
  for (std::vector<std::string>::const_iterator
           Iter = ClusteredText.begin(), IterEnd = ClusteredText.end();
       Iter != IterEnd; ++Iter) {
    Size += Iter->size();
  }
  AddLineWrapIfNeeded(Size);

  // Use WriteToken to print tokens of cluster, and add wrap lines if too
  // long to fit.
  for (std::vector<std::string>::const_iterator
           Iter = ClusteredText.begin(), IterEnd = ClusteredText.end();
       Iter != IterEnd; ++Iter) {
    WriteToken(*Iter);
  }
  ClusteredText.clear();
}

void TextFormatter::WriteLineIndents() {
  // Add line indent to base stream.
  if (AtInstructionBeginning) LineIndent = GetIndent();
  BaseStream << LineIndent;
  LinePosition += LineIndent.size();

  // If not the first line, add continuation indent to the base stream.
  if (!AtInstructionBeginning) {
    BaseStream << ContinuationIndent;
    LinePosition += ContinuationIndent.size();
    LineIndent = GetIndent();
  }

  // Add any additional indents local to the current instruction
  // being dumped to the base stream.
  for (; LinePosition < CurrentIndent; ++LinePosition) {
    BaseStream << ' ';
  }
}

RecordTextFormatter::RecordTextFormatter(ObjDumpStream *ObjDump)
    : TextFormatter(ObjDump->Records(), 0, "  "),
      ObjDump(ObjDump),
      Label(' ', ObjDump->ObjDumpAddress(0).size()),
      OpenBrace(this, "<"),
      CloseBrace(this, ">"),
      Comma(this, ","),
      Space(this),
      Endline(this),
      StartCluster(this),
      FinishCluster(this)
{}

void RecordTextFormatter::WriteLineIndents() {
  if (AtInstructionBeginning) {
    BaseStream << Label;
  } else {
    for (size_t i = 0; i < Label.size(); ++i) {
      BaseStream << ' ';
    }
  }
  BaseStream << ' ';
  LinePosition += Label.size() + 1;
  TextFormatter::WriteLineIndents();
}

void RecordTextFormatter::WriteValues(uint64_t Bit,
                                      const llvm::NaClBitcodeValues &Values) {
  Label = ObjDump->RecordAddress(Bit);
  TextStream << OpenBrace;
  for (size_t i = 0; i < Values.size(); ++i) {
    if (i > 0) {
      TextStream << Comma << FinishCluster << Space;
    }
    TextStream << StartCluster << Values[i];
  }
  // Note: Because of record codes, Values are never empty. Hence we
  // always need to finish the cluster for the last number printed.
  TextStream << FinishCluster << CloseBrace << Endline;
}

unsigned ObjDumpStream::DefaultMaxErrors = 20;

unsigned ObjDumpStream::ComboObjDumpSeparatorColumn = 40;

unsigned ObjDumpStream::RecordObjectDumpLength = 80;

ObjDumpStream::ObjDumpStream(raw_ostream &Stream,
                             bool DumpRecords, bool DumpAssembly)
    : Stream(Stream),
      DumpRecords(DumpRecords),
      DumpAssembly(DumpAssembly),
      NumErrors(0),
      MaxErrors(DefaultMaxErrors),
      RecordWidth(0),
      StartOffset(0),
      AssemblyBuffer(),
      AssemblyStream(AssemblyBuffer),
      MessageBuffer(),
      MessageStream(MessageBuffer),
      ColumnSeparator('|'),
      LastKnownBit(0),
      AddressWriteWidth(8),
      RecordBuffer(),
      RecordStream(RecordBuffer),
      RecordFormatter(this) {
  if (DumpRecords) {
    RecordWidth = DumpAssembly
        ? ComboObjDumpSeparatorColumn
        : RecordObjectDumpLength;
    RecordFormatter.SetLineWidth(RecordWidth);
  }
}

raw_ostream &ObjDumpStream::Error(uint64_t Bit) {
  LastKnownBit = Bit;
  if (NumErrors >= MaxErrors)
    Fatal(Bit, "Too many errors");
  ++NumErrors;
  return PrintMessagePrefix("Error", Bit);
}

void ObjDumpStream::Fatal(uint64_t Bit, const std::string &Message) {
  LastKnownBit = Bit;
  PrintMessagePrefix("Error", Bit) << Message;
  Flush();
  llvm::report_fatal_error("Unable to continue");
}

void ObjDumpStream::Fatal(uint64_t Bit,
                          const llvm::NaClBitcodeRecordData &Record,
                          const std::string &Message) {
  LastKnownBit = Bit;
  PrintMessagePrefix("Error", Bit) << Message;
  Write(Bit, Record);
  llvm::report_fatal_error("Unable to continue");
}

std::string ObjDumpStream::ObjDumpAddress(uint64_t Bit,
                                          unsigned MinByteWidth) {
  Bit += StartOffset;
  std::string Buffer;
  raw_string_ostream Stream(Buffer);
  Stream << '%' << MinByteWidth << PRIu64 << ":%u";
  Stream.flush();
  std::string FormatString(Buffer);
  Buffer.clear();
  Stream << format(FormatString.c_str(),
                   (Bit / 8),
                   static_cast<unsigned>(Bit % 8));
  return Stream.str();
}

// Dumps the next line of text in the buffer. Returns the number of characters
// printed.
static size_t DumpLine(raw_ostream &Stream,
                       const std::string &Buffer,
                       size_t &Index,
                       size_t Size) {
  size_t Count = 0;
  while (Index < Size) {
    char ch = Buffer[Index];
    if (ch == '\n') {
      // At end of line, stop here.
      ++Index;
      return Count;
    } else {
      Stream << ch;
      ++Index;
      ++Count;
    }
  }
  return Count;
}

void ObjDumpStream::Flush() {
  // Start by flushing all buffers, so that we know the
  // text that must be written.
  RecordStream.flush();
  AssemblyStream.flush();
  MessageStream.flush();

  // See if there is any record/assembly lines to print.
  if ((DumpRecords && !RecordBuffer.empty())
      || (DumpAssembly && !AssemblyBuffer.empty())) {
    size_t RecordIndex = 0;
    size_t RecordSize = DumpRecords ? RecordBuffer.size() : 0;
    size_t AssemblyIndex = 0;
    size_t AssemblySize = DumpAssembly ? AssemblyBuffer.size() : 0;

    while (RecordIndex < RecordSize || AssemblyIndex < AssemblySize) {
      // Dump next record line.
      size_t Column = DumpLine(Stream, RecordBuffer, RecordIndex, RecordSize);
      // Now move to separator if assembly is to be printed also.
      if (DumpRecords && DumpAssembly) {
        for (size_t i = Column; i < RecordWidth; ++i) {
          Stream << ' ';
        }
        Stream << ColumnSeparator;
      }
      // Dump next assembly line.
      DumpLine(Stream, AssemblyBuffer, AssemblyIndex, AssemblySize);
      Stream << '\n';
    }
  }

  // Print out messages and reset buffers.
  Stream << MessageBuffer;
  ResetBuffers();
  if (NumErrors >= MaxErrors) Fatal("Too many errors");
}

}
}
