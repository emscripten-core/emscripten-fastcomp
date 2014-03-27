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
      AddressWriteWidth(8) {
  if (DumpRecords) {
    RecordWidth = DumpAssembly
        ? ComboObjDumpSeparatorColumn
        : RecordObjectDumpLength;
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

void ObjDumpStream::WriteOrFlush(uint64_t Bit,
                                 const llvm::NaClBitcodeValues *Record) {
  LastKnownBit = Bit;
  // Start by flushing assembly/error buffers, so that we know the
  // text that must be written.
  AssemblyStream.flush();
  MessageStream.flush();

  // See if there is anything to print.
  if ((!DumpRecords || Record == 0)
      && (!DumpAssembly || AssemblyBuffer.empty())
      && MessageBuffer.empty()) {
    ResetBuffers();
    return;
  }

  // Something to print. Start by setting up control variables
  // for splicing records and assembly.
  size_t RecordSize = Record ? Record->size() : 0;
  // Note: We use RecordSize to denote when to print the record close
  // parenthesis. Hence, legal values go from 0 to RecordSize.
  size_t RecordIndex = (Record && DumpRecords) ? 0 : (RecordSize + 1);
  size_t RecordCol = 0;
  bool AddedRecordComma = false;
  size_t AssemblySize = AssemblyBuffer.size();
  size_t AssemblyIndex = DumpAssembly ? 0 : AssemblySize;

  // Print out records and/or assembly as appropriate.
  while (RecordIndex <= RecordSize || AssemblyIndex < AssemblySize) {
    // First fill in record information while it fits into the current line.
    while (RecordIndex < RecordSize) {
      // Fill record information.
      if (RecordIndex == 0) {
        // Beginning of record, print first element.
        std::string Buffer;
        raw_string_ostream StreamBuffer(Buffer);
        StreamBuffer << ObjDumpAddress(Bit, AddressWriteWidth) << ' '
                     << RecordIndenter.GetIndent()
                     << "<" << (*Record)[RecordIndex];
        const std::string &text = StreamBuffer.str();
        RecordCol = text.size();
        Stream << text;
        ++RecordIndex;
      } else {
        // Add comma before next element.
        if (!AddedRecordComma) {
          if (RecordCol && RecordCol + 2 >= RecordWidth) {
            // Wait to put on next line, since there is no more room.
            break;
          }
          if (RecordCol == 0) {
            std::string DumpAddress(ObjDumpAddress(Bit, AddressWriteWidth));
            const std::string &Indent = RecordIndenter.GetIndent();
            for (size_t i = 0; i < DumpAddress.size(); ++i) Stream << ' ';
            Stream << ' ' << Indent << ' ';
            RecordCol += DumpAddress.size() + Indent.size() + 2;
          }
          Stream << ", ";
          RecordCol += 2;
          AddedRecordComma = true;
        }

        // See how wide the next element is, to see if it can fit on
        // the current line.
        std::string Buffer;
        raw_string_ostream StreamBuffer(Buffer);
        StreamBuffer << (*Record)[RecordIndex];
        const std::string &Text = StreamBuffer.str();
        if (RecordCol && RecordCol + Text.size() >= RecordWidth) {
          // Wait to put on next line, since there is no more room.
          break;
        }
        // If reached, fits on current line so add.
        if (RecordCol == 0) {
          // At beginning of new line, indent appropriately first.
          std::string DumpAddress(ObjDumpAddress(Bit, AddressWriteWidth));
          const std::string &Indent = RecordIndenter.GetIndent();
          for (size_t i = 0; i < DumpAddress.size(); ++i) Stream << ' ';
          Stream << ' ' << Indent << ' ';
          RecordCol += DumpAddress.size() + Indent.size() + 2;
        }
        // Print out element.
        Stream << Text;
        RecordCol += Text.size();
        AddedRecordComma = false;
        ++RecordIndex;
      }
    }

    if (RecordIndex == RecordSize) {
      // Add end of record if there is room. Otherwise, add end of record
      // to next line.
      if (Record == 0) {
        // No record, so nothing to close.
        ++RecordIndex;
      } else if (RecordCol == 0) {
        // At beginning of new line, indent appropriately first.
        const std::string &Indent = RecordIndenter.GetIndent();
        Stream << Indent << ">";
        RecordCol = Indent.size() + 1;
        ++RecordIndex;
      } else if (RecordCol < RecordWidth) {
        // Fits on current line, close record.
        Stream << '>';
        ++RecordCol;
        ++RecordIndex;
      } else {
        // Doesn't fit on current line, wait for next line.
        RecordCol = 0;
      }
    }

    // Now move to separator if assembly is to be printed also.
    if (DumpRecords && DumpAssembly) {
      for (size_t i = RecordCol; i < RecordWidth; ++i) {
        Stream << ' ';
      }
      Stream << ColumnSeparator;
    }
    RecordCol = 0;

    // Fill in next line of assembly.
    bool DumpedAssembly = false;
    while(AssemblyIndex < AssemblySize) {
      char ch = AssemblyBuffer[AssemblyIndex];
      if (ch == '\n') {
        // At end of line, stop here.
        ++AssemblyIndex;
        break;
      } else {
        DumpedAssembly = true;
        Stream << ch;
        ++AssemblyIndex;
      }
    }

    // Line full. End line and continue on next line (if applicable).
    // Note that this is called by either Write or Flush. The first
    // test verifies that a record write is occurring. The second test
    // checks if there is assembly being flushed, that wasn't
    // preceeded with record contents.
    if ((DumpRecords && Record) || (DumpAssembly && DumpedAssembly))
      Stream << '\n';
  }

  // Printed records and assembly. Now print errors if applicable.
  Stream << MessageBuffer;
  ResetBuffers();
  if (NumErrors >= MaxErrors) Fatal(Bit, "Too many errors");
}


}
}
