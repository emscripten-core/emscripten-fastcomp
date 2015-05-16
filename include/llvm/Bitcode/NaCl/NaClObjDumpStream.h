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

#include "llvm/ADT/STLExtras.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeParser.h"
#include "llvm/Support/raw_ostream.h"
#include <vector>
#include <algorithm>

namespace llvm {
namespace naclbitc {

// The default string assumed for a tab.
static const char *DefaultTab = "        ";

/// Class that implements text indenting for pretty printing text.
class TextIndenter {
public:
  /// Creates a text indenter that indents using the given tab.
  TextIndenter(const char* Tab = DefaultTab)
    : Indent(""),
      Tab(Tab),
      TabSize(strlen(Tab)),
      NumTabs(0) {
    Values.push_back(Indent);
  }

  virtual ~TextIndenter() {}

  /// Returns the current indentation to use.
  const std::string &GetIndent() const {
    return Indent;
  }

  /// Returns the indent with the given number of tabs.
  const std::string &GetIndent(unsigned Count) {
    if (Count >= Values.size()) {
      // Indents not yet generated, fill in cache to size needed.
      std::string Results;
      if (Values.size() > 0) Results = Values.back();
      for (size_t i = Values.size(); i <= Count; ++i) {
        Results += Tab;
        Values.push_back(Results);
      }
    }
    return Values[Count];
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

  const char *GetTab() const {
    return Tab;
  }

  size_t GetTabSize() const {
    return TabSize;
  }

private:
  // The current indentation to use.
  std::string Indent;
  // The set of (previously computed) identations, based on the number
  // of tabs.
  std::vector<std::string> Values;
  // The text defining a tab.
  const char *Tab;
  // The size of the tab.
  size_t TabSize;
  // The number of tabs currently being used.
  unsigned NumTabs;
};

class TextFormatter;

/// This template class maintains a simply pool of directives for
/// a text formatter. Assumes that all elements in pool are associated
/// with the same formatter.
template<class Directive>
class DirectiveMemoryPool {
public:
  DirectiveMemoryPool() {}

  ~DirectiveMemoryPool() {
    DeleteContainerPointers(FreeList);
  }

  Directive *Allocate(TextFormatter *Fmtr) {
    if (FreeList.empty()) return new Directive(Fmtr);
    Directive *Element = FreeList.back();
    assert(&Element->GetFormatter() == Fmtr
           && "Directive memory pool formatter mismatch");
    FreeList.pop_back();
    return Element;
  }

  void Free(Directive *Dir) {
    FreeList.push_back(Dir);
  }

private:
  std::vector<Directive*> FreeList;
};

/// This class defines a simple formatter for a stream that consists
/// of a sequence of instructions. In general, this class assumes that
/// instructions are a single line. In addition, some instructions
/// define a block around a sequence of instructions. Each block of
/// instructions is indented to clarify the block structure over that
/// set of instructions.
///
/// To handle line indentation of blocks, this class inherits class
/// TextIndenter.  The TextIndenter controls how blocks of lines are
/// indented. At the beginning of a block, the user should call method
/// Inc.  It should then write out the sequence of instructions to be
/// indented.  Then, after the last indented instruction of the block
/// is printed, the user should call method Dec. Nested blocks are
/// handled by nesting methods Inc and Dec appropriately.
///
/// This class mainly focuses on the tools needed to format an
/// instruction, given a specified viewing width. The issue is that
/// while instructions should be on a single line, some instructions
/// are too wide to fit into the viewing width. Hence, we need a way
/// to deal with line overflow.
///
/// The way this class handles the line overflow problem is to
/// basically force the user to break up the output into a sequence of
/// tokens. This is done using two streams. The text stream is used to
/// buffer tokens.  The base stream is the stream to write tokens to
/// once they have been identified and positioned.  The goal of the
/// formatter is to decide where the instruction text should be cut
/// (i.e. between tokens), so that the instruction does not overflow
/// the viewing width. It also handles the automatic insertion of line
/// indentation into the base stream when needed.
///
/// To make it easy to print tokens to the text stream, we use text
/// directives.  Whenever a text directive is written to the text
/// stream, it is assumed that all text written to the text stream
/// (since the last directive) is an (indivisible) token. The first
/// thing directives do is move the token from the text stream (if
/// present) to the base stream.
///
/// In addition to defining tokens, text directives can also be
/// defined to query the formatter state and apply an appropriate
/// action. An example of this is the space text directive. It only
/// adds a space if the formatter isn't at the beginning of a new line
/// (since a newline can act as whitespace).
///
/// The text formatter also has a notion of clustering
/// tokens. Clustering forces a consecutive sequence of tokens to
/// be treated as a single token, when deciding where to wrap long
/// lines. In particular, clustered tokens will not be broken up
/// unless there is no other way to print them, because the cluster is
/// larger than what can fit in a line.
///
/// Clustering is implemented using two passes. In the first pass, the
/// sequence of tokens/directives are collected to find the clustered
/// text.  They are also applied to collect any text internal to the
/// directives. Actions that can change (intraline) indenting are
/// turned off.
///
/// Once the first pass is done, the second pass starts. It begins
/// with a check to see if the clustered text can fit on the current
/// line, and adds a newline if necessary. Then it replays the
/// directives to put the tokens of the cluster into the base stream.
/// The replay also changes (intraline) indenting as necessary.
///
/// Clusters can be nested. In such cases, they are stripped one layer
/// per pass. Nested clusters are replayed after line wrapping of
/// outer clusters have been resolved.
class TextFormatter : public TextIndenter {
public:

  /// Creates a text formatter to print instructions onto the given
  /// Base Stream. The viewing width is defined by LineWidth. The
  /// given tab is the assumed value for tab characters and line
  /// indents.
  explicit TextFormatter(raw_ostream &BaseStream,
                         unsigned LineWidth = 80,
                         const char *Tab = DefaultTab);

  ~TextFormatter() override;

  /// Returns the user-level text stream of the formatter that tokens
  /// should be written to.
  raw_ostream &Tokens() {
    return TextStream;
  }

  /// Changes the line width to the given value.
  void SetLineWidth(unsigned NewLineWidth) {
    LineWidth = NewLineWidth;
    if (MinLineWidth > LineWidth) MinLineWidth = LineWidth;
  }

  // Changes the (default) line-wrap, continuation indent.
  void SetContinuationIndent(const std::string &Indent) {
    ContinuationIndent = Indent;
  }

  /// Base class for all directives. The basic functionality is that it
  /// has a reference to the text formatter, and is applied by calling
  /// method apply. This method apply is encorporated into the
  /// stream operator<<, so that they can be used as part of the
  /// streamed output.
  ///
  /// Method apply extracts any tokens in the text stream.  Moves them
  /// into the base stream. Finally, it calls virtual method MyApply
  /// to do the actions of the directive.
  class Directive {
    Directive(const Directive&) = delete;
    void operator=(const Directive&) = delete;
  public:
    /// Creates a directive for the given stream.
    explicit Directive(TextFormatter *Formatter)
        : Formatter(Formatter) {}

    virtual ~Directive() {}

    /// Returns the formatter associated with the directive.
    TextFormatter &GetFormatter() const {
      return *Formatter;
    }

    /// Does action of directive.
    void Apply() const {
      // Start by writing token if Tokens() buffer (if non-empty).
      Formatter->WriteToken();
      // Apply the directive.
      MyApply(false);
      // Save the directive for replay if we are clustering.
      MaybeSaveForReplay();
    }

   protected:
    // The formatter associated with the directive.
    TextFormatter *Formatter;

    // Like Apply, but called instead of Apply whey doing a replay.
    void Reapply() const;

    // Does directive specific action. Replay is true only if the
    // directive was in a cluster, and it is being called to replay
    // the directive a second time.
    virtual void MyApply(bool Replay) const = 0;

    // Adds the directive to the clustered directives if appropriate
    // (i.e. inside a cluster).
    virtual void MaybeSaveForReplay() const {
      if (IsClustering()) AppendForReplay(this);
    }

    // ***********************************************************
    // Note: The following have been added so that derived classes
    // have public access to protected text formatter methods.
    // (Otherwise, you get a compiler error that they are protected
    //  in derived classes).
    // ***********************************************************

    bool IsClustering() const {
      return Formatter->IsClustering();
    }

    unsigned GetClusteringLevel() const {
      return Formatter->GetClusteringLevel();
    }

    void AppendForReplay(const Directive *D) const {
      Formatter->AppendForReplay(D);
    }

    raw_ostream &Tokens() const {
      return Formatter->Tokens();
    }

    void WriteToken(const std::string &Token) const {
      Formatter->WriteToken(Token);
    }

    void WriteEndline() const {
      Formatter->WriteEndline();
    }

    void PopIndent() const {
      Formatter->PopIndent();
    }

    void PushIndent() const {
      Formatter->PushIndent();
    }

    bool AddLineWrapIfNeeded(unsigned TextSize) const {
      return Formatter->AddLineWrapIfNeeded(TextSize);
    }

    void StartClustering() const {
      Formatter->StartClustering();
    }

    void FinishClustering() const {
      Formatter->FinishClustering();
    }
  };

protected:
  // The base stream to send formatted text to.
  raw_ostream &BaseStream;
  // Buffer that holds token text.
  std::string TextBuffer;
  // The user-level stream for (token) text and directives.
  raw_string_ostream TextStream;
  // The buffered token waiting to be flushed to the base stream.
  // std::string BufferedToken;
  // The expected line width the formatter should try and match.
  unsigned LineWidth;
  // The current position (i.e. column of previous character on line)
  // associated with the current line in the base stream.
  unsigned LinePosition;
  // The stack of (intraline) indents added by PushIndent.
  std::vector<unsigned> IntralineIndents;
  // The current intraline indent to use.
  unsigned CurrentIndent;
  // The minimum line width. Used to limit indents so that there
  // will always be at least this amount of space on the line.
  unsigned MinLineWidth;
  // True if no characters have moved to the base stream for the
  // instruction being printed.  Note: This is different than
  // LinePosition == 0. The latter can be true, when
  // AtInstructionBeginning is false, if the current line is a
  // continuation line caused by line overflow.
  bool AtInstructionBeginning;
  // The indent string to use for indenting each line of the instruction.
  std::string LineIndent;
  // The indent to add on continuation (i.e. overflow) lines for an
  // instruction.
  std::string ContinuationIndent;
  // Counts how nested we are within clustering directives.
  unsigned ClusteringLevel;
  // Contains the number of columns needed to hold the generated text,
  // while clustering. Computed so that we can test if it fits on the
  // current line.
  unsigned ClusteredTextSize;
  // Contains the sequence of directives (including tokens) during
  // clustering, so that it can be replayed after we determine if a
  // new line needs to be added.
  std::vector<const Directive*> ClusteredDirectives;

  // Returns if we are currently inside a cluster.
  bool IsClustering() const {
    return ClusteringLevel > 0;
  }

  // Returns the clustering level of the formatter.
  unsigned GetClusteringLevel() const {
    return ClusteringLevel;
  }

  // Appends the given directive to the list of directives to
  // replay, if clustering.
  void AppendForReplay(const Directive *D) {
    ClusteredDirectives.push_back(D);
  }

  /// Writes out the token in the text stream. If necessary, moves to
  // a new line first.
  void WriteToken() {
    WriteToken(GetToken());
  }

  /// Writes out the given token. If necessary, moves to a new line first.
  void WriteToken(const std::string &Token) {
    if (Token.empty()) return;
    AddLineWrapIfNeeded(Token.size());
    Write(Token);
  }

  // Extracts the text that has been added to the text stream, and
  // returns it.
  std::string GetToken();

  /// Writes out a non-newline character to the base stream.
  void Write(char ch);

  /// Writes out a string.
  void Write(const std::string &Text);

  /// Starts a new cluster of tokens.
  /// Note: We do not allow nested clusterings.
  void StartClustering();

  /// Ends the existing cluster of tokens.
  void FinishClustering();

  /// Adds a newline that ends the current instruction being printed.
  void WriteEndline();

  /// Called just before the first character on a line is printed, to
  /// add indentation text for the line.
  virtual void WriteLineIndents();

  /// Analyzes the formatter state to determine if a token of the
  /// given TextSize can fit on the current line. If it can't fit,
  /// it wraps the input line. Returns true only if line wrap was
  /// added by this method.
  bool AddLineWrapIfNeeded(unsigned TextSize) {
    if (IsClustering()) {
      // Don't consider line wrapping until all text is clustered. That
      // way we have full information on what we should do.
      return false;
    } else {
      // If the text fits on the current line, there is nothing to do.
      // Otherwise, we should add a newline, unless we are already
      // at the new line. If we are already at the newline, we can't
      // split up the token, so just allow line overflow.
      if (LinePosition + TextSize <= LineWidth
          || LinePosition == 0)
        return false;
      // If reached, it must not fit, so add a line wrap.
      Write('\n');
      return true;
    }
  }

  /// Pops the current (intraline) indentation and restores it to the
  /// previous indentation. Used to restore the indentation of
  /// parenthesized subexpressions, where this should be called on the
  /// close parenthesis.
  void PopIndent() {
    // Be pragmatic. If there is underflow, assume that it was due to
    // the fact that the caller of this text formatter used a close
    // directive for a pair of matching parenthesis that crossed
    // multiple (i.e. instruction) lines.
    if (IsClustering() || IntralineIndents.empty()) return;
    CurrentIndent = IntralineIndents.back();
    IntralineIndents.pop_back();
  }

  /// Pushes the current (intraline) indentation to match the current
  /// position within the line. Used to indent parenthesized
  /// subexpressions, where this should be called on the open
  /// parenthesis.
  void PushIndent() {
    if (IsClustering()) return;
    IntralineIndents.push_back(CurrentIndent);
    CurrentIndent = FixIndentValue(LinePosition);
  }

  /// Fixes the given position, to the appropriate value for setting
  /// CurrentIndent. That is, it makes sure there is always MinLineWidth
  /// printable characters on a line.
  unsigned FixIndentValue(unsigned Position) {
    return std::min(Position, LineWidth - MinLineWidth);
  }

private:
  /// Directive that generates temporary instances to hold tokens from
  /// the Tokens() stream. Generated when GetToken() is called. Used
  /// during clustering to regenerate the corresponding extracted
  /// tokens during a replay.
  class GetTokenDirective : public Directive {
    friend class DirectiveMemoryPool<GetTokenDirective>;
    GetTokenDirective(TextFormatter *Formatter)
        : Directive(Formatter) {}

  public:
    ~GetTokenDirective() override {}

    /// Allocates an instance of a GetTokenDirective.
    /// Note: Will be reclaimed when MyApply is called.
    static Directive *Allocate(TextFormatter *Formatter,
                               const std::string &Text);

  protected:
    void MyApply(bool Replay) const override {
      WriteToken(Text);
      if (!IsClustering())
        Formatter->GetTokenFreeList.Free(const_cast<GetTokenDirective*>(this));
    }

  private:
    // The token text.
    std::string Text;
  };

  // The set of freed GetTokenDirectives, that can be reused.
  DirectiveMemoryPool<GetTokenDirective> GetTokenFreeList;
};

inline raw_ostream &operator<<(raw_ostream &Stream,
                               const TextFormatter::Directive &Directive) {
  // Be sure that the directive is defined for the given stream,
  // before applying the directive, since directives may have
  // different meanings on different text streams.
  assert(&Stream == &Directive.GetFormatter().Tokens());
  Directive.Apply();
  return Stream;
}

/// Defines a directive that only tokenizes the text in the Tokens()
/// stream.
class TokenizeTextDirective : public TextFormatter::Directive {
public:
  explicit TokenizeTextDirective(TextFormatter *Formatter)
      : TextFormatter::Directive(Formatter) {}

  ~TokenizeTextDirective() override {}

protected:
  void MyApply(bool Replay) const override {}
};

/// Defines a token which doesn't need whitespace on either side of
/// the token to be a valid token (such as punctuation).
class TokenTextDirective : public TextFormatter::Directive {
public:
  TokenTextDirective(TextFormatter *Formatter, const std::string &Str)
      : TextFormatter::Directive(Formatter), Text(Str) {
  }

  ~TokenTextDirective() override {}

protected:
  void MyApply(bool Replay) const override {
    WriteToken(Text);
  }

private:
  std::string Text;
};

/// Writes out the current token. Then adds a space if the base
/// stream is not at the beginning of a continuation line.
class SpaceTextDirective : public TextFormatter::Directive {
public:
  SpaceTextDirective(TextFormatter *Formatter, const std::string &Space)
      : TextFormatter::Directive(Formatter), Space(Space) {}

  explicit SpaceTextDirective(TextFormatter *Formatter)
      : TextFormatter::Directive(Formatter), Space(" ") {}

  ~SpaceTextDirective() {}

protected:
  void MyApply(bool Replay) const override {
    if (!AddLineWrapIfNeeded(Space.size()))
      WriteToken(Space);
  }
private:
  std::string Space;
};

/// Adds a newline that ends the current instruction being printed.
class EndlineTextDirective : public TextFormatter::Directive {
public:
  explicit EndlineTextDirective(TextFormatter *Formatter)
      : TextFormatter::Directive(Formatter) {}

  ~EndlineTextDirective() override {}

protected:
  void MyApply(bool Replay) const override {
    WriteEndline();
  }
};

/// Inserts a token and then pushes the current indent to the position
/// after the token. Used to model a open parenthesis.
class OpenTextDirective : public TokenTextDirective {
public:

  // Creates an open using the given indent.
  OpenTextDirective(TextFormatter *Formatter, const std::string &Text)
      : TokenTextDirective(Formatter, Text) {}

  ~OpenTextDirective() override {}

protected:
  void MyApply(bool Replay) const override {
    TokenTextDirective::MyApply(Replay);
    PushIndent();
  }
};

/// Inserts a token and then pops current indent.  Used to model a
/// close parenthesis.
class CloseTextDirective : public TokenTextDirective {
public:
  CloseTextDirective(TextFormatter *Formatter, const std::string &Text)
      : TokenTextDirective(Formatter, Text) {}

  ~CloseTextDirective() override {}

protected:
  void MyApply(bool Replay) const override {
    TokenTextDirective::MyApply(Replay);
    PopIndent();
  }
};

/// Defines the beginning of a token cluster, which should be put on the
/// same line if possible.
class StartClusteringDirective : public TextFormatter::Directive {
public:
  explicit StartClusteringDirective(TextFormatter *Formatter)
      : TextFormatter::Directive(Formatter) {}

  ~StartClusteringDirective() override {}

protected:
  void MyApply(bool Replay) const override {
    StartClustering();
  }

  void MaybeSaveForReplay() const override {
    if (GetClusteringLevel() > 1) AppendForReplay(this);
  }
};

class FinishClusteringDirective : public TextFormatter::Directive {
public:
  explicit FinishClusteringDirective(TextFormatter *Formatter)
      : TextFormatter::Directive(Formatter) {}

  ~FinishClusteringDirective() override {}

protected:
  void MyApply(bool Replay) const override {
    FinishClustering();
  }
};

class ObjDumpStream;

/// Models that an abbreviation index is not specified when dumping a
/// bitcode record.
static int32_t ABBREV_INDEX_NOT_SPECIFIED = -1;

/// The formatter used for dumping records in ObjDumpStream.
class RecordTextFormatter : public TextFormatter {
  RecordTextFormatter(const RecordTextFormatter&) = delete;
  RecordTextFormatter &operator=(const RecordTextFormatter&) = delete;
public:
  /// The address write width used to print a bit address, when
  /// printing records.
  static const unsigned AddressWriteWidth = 10;

  explicit RecordTextFormatter(ObjDumpStream *ObjDump);

  ~RecordTextFormatter() override {}

  /// Writes out the given record of values as an instruction.
  void WriteValues(uint64_t Bit,
                   const llvm::NaClBitcodeValues &Values,
                   int32_t AbbrevIndex = ABBREV_INDEX_NOT_SPECIFIED);

  /// Returns text corresponding to an empty label column.
  std::string GetEmptyLabelColumn();

protected:
  void WriteLineIndents() override;

private:
  // The address label associated with the current line.
  std::string Label;
  // The open brace '<' for a record.
  OpenTextDirective OpenBrace;
  // The close brace '>' for a record.
  CloseTextDirective CloseBrace;
  // A comma between elements in a record.
  TokenTextDirective Comma;
  // A space inside a record.
  SpaceTextDirective Space;
  // End the line.
  EndlineTextDirective Endline;
  // Start clustering tokens.
  StartClusteringDirective StartCluster;
  // End clustering tokens.
  FinishClusteringDirective FinishCluster;

  // Generates an address label with padding to match AddressWriteWidth;
  std::string getBitAddress(uint64_t Bit);
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
  ObjDumpStream(const ObjDumpStream&) = delete;
  void operator=(const ObjDumpStream&) = delete;
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

  /// Returns stream to buffer records that will be printed during the
  /// next write call.
  raw_ostream &Records() {
    return RecordStream;
  }

  /// Returns stream to buffer messages that will be printed during the
  // next write call.
  raw_ostream &Comments() {
    return MessageStream;
  }

  /// Prints "Warning(Bit/8:Bit%8): " onto the comments stream, and
  /// then returns the comments stream. In general, warnings will be
  /// printed after the next record, unless a call to Flush is made.
  raw_ostream &Warning() {
    return WarningAt(LastKnownBit);
  }

  /// Prints "Warning(Bit/8:Bit%8): " onto the comments stream, and
  /// then returns the comments stream. In general, warnings will be
  /// printed after the next record, unless a call to Flush is made.
  raw_ostream &WarningAt(uint64_t Bit) {
    LastKnownBit = Bit;
    return naclbitc::ErrorAt(Comments(), naclbitc::Warning, Bit);
  }

  /// Prints "Error(Bit/8:Bit%8): " onto the comments stream using the
  /// last know bit position of the input. Then, it records that an
  /// error has occurred and returns the comments stream. In general
  /// errors will be printed after the next record, unless a call to
  /// Flush is made.
  raw_ostream &Error() {
    return ErrorAt(LastKnownBit);
  }

  /// Prints "Error(Bit/8:Bit%8): " onto the comments stream at the
  /// given Bit position. Then, it records that an error has occurred
  /// and returns the comments stream. In general errors will be
  /// printed after the next record, unless a call to Flush is made.
  raw_ostream &ErrorAt(uint64_t Bit) {
    return ErrorAt(naclbitc::Error, Bit);
  }

  /// Prints "Level(Bit/8:Bit%8): " onto the comments stream at the
  /// given bit position and severity Level.  Then, it records that an
  /// error has occurred and then returns the comments stream. In
  /// general errors will be printed after the next record, unless a
  /// call to Flush is made.
  raw_ostream &ErrorAt(naclbitc::ErrorLevel Level, uint64_t Bit);

  /// Dumps a record (at the given bit), along with all buffered assembly,
  /// comments, and errors, into the objdump stream.
  void Write(uint64_t Bit,
             const llvm::NaClBitcodeRecordData &Record,
             int32_t AbbrevIndex = ABBREV_INDEX_NOT_SPECIFIED) {
    LastKnownBit = Bit;
    NaClBitcodeValues Values(Record);
    RecordFormatter.WriteValues(Bit, Values, AbbrevIndex);
    Flush();
  }

  /// Dumps the buffered assembly, comments, and errors, without any
  /// corresponding record, into the objdump stream.
  void Flush();

  /// Flushes out the last record and/or error, and then stops
  /// the executable. Should be called immediately after generating
  /// the error message using ErrorAt(naclbitc::Fatal,...).
  void FlushThenQuit();

  /// Increments the record indent by one.
  void IncRecordIndent() {
    RecordFormatter.Inc();
  }

  /// Decrements the record indent by one.
  void DecRecordIndent() {
    RecordFormatter.Dec();
  }

  /// Returns the record indenter being used by the objdump stream, for
  /// the purposes of querying state.
  const TextIndenter &GetRecordIndenter() {
    return RecordFormatter;
  }

  /// Returns the number of errors reported to the dump stream.
  unsigned GetNumErrors() const {
    return NumErrors;
  }

  // Returns true if dumping record.
  bool GetDumpRecords() const {
    return DumpRecords;
  }

  // Returns true if dumping assembly.
  bool GetDumpAssembly() const {
    return DumpAssembly;
  }

  /// Changes the maximum number of errors allowed.
  void SetMaxErrors(unsigned NewMax) {
    MaxErrors = NewMax;
  }

  /// Changes the width allowed for records (from the default).
  void SetRecordWidth(unsigned Width) {
    RecordWidth = Width;
  }

  unsigned GetRecordWidth() const {
    return RecordWidth;
  }

  /// Changes the column separator character to the given value.
  void SetColumnSeparator(char Separator) {
    ColumnSeparator = Separator;
  }

  /// Changes the internal state, to assume one is processing a record
  /// at the given bit.
  void SetRecordBitAddress(uint64_t Bit) {
    LastKnownBit = Bit;
  }

private:
  // The stream to dump to.
  raw_ostream &Stream;
  // True if records should be dumped to the dump stream.
  bool DumpRecords;
  // True if assembly text should be dumped to the dump stream.
  bool DumpAssembly;
  // The number of errors reported.
  unsigned NumErrors;
  // The maximum number of errors before quitting.
  unsigned MaxErrors;
  // The number of columns available to print bitcode records.
  unsigned RecordWidth;
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
  /// The buffer for records to be printed to during the next write.
  std::string RecordBuffer;
  /// The stream to buffer recordds into the record buffer.
  raw_string_ostream RecordStream;
  /// The text formatter for generating records.
  RecordTextFormatter RecordFormatter;

  // Resets assembly and buffers.
  void ResetBuffers() {
    RecordBuffer.clear();
    AssemblyBuffer.clear();
    MessageBuffer.clear();
  }
};

}
}

#endif
