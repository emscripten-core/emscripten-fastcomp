//===- NaClObjDumpStream.h --------------------------------------*- C++ -*-===//
//     Models an objdump stream (bitcode records/assembly code).
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_BITCODE_NACL_NACLOBJDUMPSTREAM_H
#define LLVM_BITCODE_NACL_NACLOBJDUMPSTREAM_H

#include "llvm/Bitcode/NaCl/NaClBitcodeParser.h"
#include "llvm/Support/raw_ostream.h"
#include <vector>

namespace llvm {
namespace naclbitc {

/// Class that implements text indenting for pretty printing text.
class TextIndenter {
public:
  /// Creates a text indenter that indents using the given tab.
  TextIndenter(const char* Tab = "  ")
    : Indent(""),
      Tab(Tab),
      NumTabs(0) {
    Values.push_back(Indent);
  }

  ~TextIndenter() {}

  /// Returns the current indentation to use.
  const std::string &GetIndent() const {
    return Indent;
  }

  /// Increments the current indentation by one tab.
  void Inc() {
    ++NumTabs;
    if (NumTabs < Values.size()) {
      Indent = Values[NumTabs];
    } else {
      Indent += Tab;
      Values.push_back(Indent);
    }
  }

  /// Decrements the current indentation by one tab.
  void Dec() {
    // Be sure not to underflow!
    if (NumTabs) {
      --NumTabs;
      Indent = Values[NumTabs];
    }
  }

  /// Returns the current number of tabs in the current indentation.
  unsigned GetNumTabs() const {
    return NumTabs;
  }

private:
  // The current indentation to use.
  std::string Indent;
  // The set of (previously computed) identations, based on the number
  // of tabs.
  std::vector<std::string> Values;
  // The text defining a tab.
  const char *Tab;
  // The number of tabs currently being used.
  unsigned NumTabs;
};

/// Implements a stream to print out bitcode records, assembly code,
/// comments, and errors. The general format is to print records and
/// assembly side by side. Comments and errors (associated with the
/// record and/or assembly code) follow each printed record.
///
/// Alignment of records, assembly, comments, and errors is done by
/// buffering assembly, comments, and errors until a write or flush
/// occurs.  Then, the various streams are stitched together to
/// produce the corresponding output text. There are two buffers: one
/// holds the assembly, and the other holds comments and errors.
///
/// There are exactly two methods that can cause text to be added to
/// the objdump stream. Method Flush just flushes out the assembly and
/// comments/errors buffer without printing a record. If there is no
/// buffered assembly/comments/errors, nothing is done. Method Write
/// prints the given record, and also flushes out the assembly and
/// comments/errors buffer. Hence, in general, comments and errors
/// follow the record/assembly. However, if you want them to appear
/// before, use method Flush.
///
/// The constructor is defined with two flags: DumpRecords and
/// DumpAssembly.  Comments and errors are flushed on every write,
/// independent of these flags.  Records are printed out only if
/// DumpRecords is true. Assembly is flushed only if DumpAssembly is
/// true.
///
/// To buffer assembly, call method Assembly to get a (string) stream
/// to buffer the assembly code. To buffer comments, call method
/// Comments() to get a (string) stream to buffer the comments.
///
/// To buffer an error, call method Error. This method will increment
/// the error count, and return the comments stream after writing
/// "Error(byte:bit): ".
///
/// If a single line of text is buffered into the assembly stream, no
/// new line is needed. The corresponding call to Write will
/// automatically insert the newline for you, if you did not add
/// it. If multiple lines are to be buffered into the assembly stream,
/// each line must be separated with a newline character.  It is
/// always safe to end all assembly lines with a newline character.
///
/// Also note that this class takes care of formatting records to fit
/// into a calculated record width (based on value set to
/// RecordWidth).  On the other hand, we assume that the assembly is
/// formatted by the caller (i.e. owner of this object).
class ObjDumpStream {
  ObjDumpStream(const ObjDumpStream&) LLVM_DELETED_FUNCTION;
  void operator=(const ObjDumpStream&) LLVM_DELETED_FUNCTION;
public:
  /// The default number of error messages that will be printed before
  /// execution is stopped due to too many errors.
  static unsigned DefaultMaxErrors;

  /// The default value for the column that separates records and
  /// assembly, when DumpRecords and DumpAssembly is true.
  static unsigned ComboObjDumpSeparatorColumn;

  /// The default value for line width when DumpRecords is true,
  /// and DumpAssembly is false. This value is typically larger
  /// than ComboObjDumpSeparatorColumn, since the entire line
  /// can be used to print records.
  static unsigned RecordObjectDumpLength;

  /// Creates an objdump stream which will dump records, assembly,
  /// comments and errors into a single (user proved Stream).  When
  /// DumpRecords is true, the contents of records will be
  /// dumped. When DumpAssembly is true, the corresponding assembly
  /// will be printed. When both are true, the records and assembly
  /// will be printed side by side. When both are false, only comments
  /// and errors will be printed.
  ObjDumpStream(raw_ostream &Stream, bool DumpRecords, bool DumpAssembly);

  ~ObjDumpStream() { Flush(); }

  /// Returns stream to buffer assembly that will be printed during the
  /// next write call.
  raw_ostream &Assembly() {
    return AssemblyStream;
  }

  /// Returns stream to buffer messages that will be printed during the
  // next write call.
  raw_ostream &Comments() {
    return MessageStream;
  }

  /// Prints "Error(Bit/8:Bit%8): " onto the comments stream, records
  /// that an error has occurred, and then returns the comments
  /// stream. In general errors will be printed after the next record,
  /// unless a call to Flush is made.
  raw_ostream &Error() {
    return Error(LastKnownBit);
  }

  /// Prints "Error(Bit/8:Bit%8): " onto the comments stream, records
  /// that an error has occurred, and then returns the comments
  /// stream. In general errors will be printed after the next record,
  /// unless a call to Flush is made.
  raw_ostream &Error(uint64_t Bit);

  /// Write a fatal error message to the dump stream, and then
  /// stop the executable. If any assembly, comments, or errors have
  /// been buffered, they will be printed first.
  void Fatal(const std::string &Message) {
    Fatal(LastKnownBit, Message);
  }

  /// Write a fatal error message to the dump stream, and then
  /// stop the executable. If any assembly, comments, or errors have
  /// been buffered, they will be printed first. Associates fatal error
  /// Message with the given Bit.
  void Fatal(uint64_t Bit, const std::string &Message);

  /// Write a fatal error message to the dump stream, and then
  /// stop the executable. If any assembly, comments, or errors have
  /// been buffered, they will be printed first, along with the given record.
  void Fatal(uint64_t Bit,
             const llvm::NaClBitcodeRecordData &Record,
             const std::string &Message);

  /// Dumps a record (at the given bit), along with all buffered assembly,
  /// comments, and errors, into the objdump stream.
  void Write(uint64_t Bit,
             const llvm::NaClBitcodeRecordData &Record) {
    NaClBitcodeValues Values(Record);
    WriteOrFlush(Bit, &Values);
  }

  /// Dumps the buffered assembly, comments, and errors, without any
  /// corresponding record, into the objdump stream.
  void Flush() {
    WriteOrFlush(0, 0);
  }

  /// Returns the record indenter being used by the objdump stream.
  TextIndenter &GetRecordIndenter() {
    return RecordIndenter;
  }

  /// Returns the number of errors reported to the dump stream.
  unsigned GetNumErrors() const {
    return NumErrors;
  }

  /// Changes the default assumption that bit addresses start
  /// at index 0.
  void SetStartOffset(uint64_t Offset) {
    StartOffset = Offset;
  }

  /// Changes the maximum number of errors allowed.
  void SetMaxErrors(unsigned NewMax) {
    MaxErrors = NewMax;
  }

  /// Changes the width allowed for records (from the default).
  void SetRecordWidth(unsigned Width) {
    RecordWidth = Width;
  }

  /// Changes the column separator character to the given value.
  void SetColumnSeparator(char Separator) {
    ColumnSeparator = Separator;
  }

  // Converts the given start bit to the corresponding address to
  // print. That is, generate "Bit/8:Bit%8" value.
  std::string ObjDumpAddress(uint64_t Bit, unsigned MinByteWidth=1);

private:
  // The stream to dump to.
  raw_ostream &Stream;
  // True if records should be dumped to the dump stream.
  bool DumpRecords;
  // True if assembly text should be dumped to the dump stream.
  bool DumpAssembly;
  // The indenter for indenting records.
  TextIndenter RecordIndenter;
  // The number of errors reported.
  unsigned NumErrors;
  // The maximum number of errors before quitting.
  unsigned MaxErrors;
  // The number of columns available to print bitcode records.
  unsigned RecordWidth;
  // The number of bits to add to the record bit address, to correct
  // the record bit address passed to the write routines.
  uint64_t StartOffset;
  // The buffer for assembly to be printed during the next write.
  std::string AssemblyBuffer;
  // The stream to buffer assembly into the assembly buffer.
  raw_string_ostream AssemblyStream;
  // The buffer for comments and errors.
  std::string MessageBuffer;
  // The stream to buffer comments and errors into the message.
  raw_string_ostream MessageStream;
  // The character used to separate records from assembly.
  char ColumnSeparator;
  // The last known bit passed to the objdump object. Used as default
  // for automatically generated errors.
  uint64_t LastKnownBit;
  /// The address write width used to print the number of
  /// bytes in the record bit address, when printing records.
  unsigned AddressWriteWidth;

  // Low-level write. Dumps buffered assembly, comments, and errors,
  // and optional record. If Record is null, no record is printed.
  void WriteOrFlush(uint64_t StartBit,
                    const llvm::NaClBitcodeValues *Record);

  // Resets assembly and buffers.
  void ResetBuffers() {
    AssemblyBuffer.clear();
    MessageBuffer.clear();
  }

  // Returns the message stream with 'Label(Bit/8:Bit%8): '.
  raw_ostream &PrintMessagePrefix(const char *Label, uint64_t Bit) {
    return Comments() << Label << "(" << ObjDumpAddress(Bit) << "): ";
  }
};

}
}

#endif
