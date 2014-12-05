//===- llvm/unittest/Bitcode/NaClTextFormatterTest.cpp --------------------===//
//     Tests the text formatter for PNaCl bitcode.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// Tests if the text formatter for PNaCl bitcode works as expected.

#include "llvm/ADT/STLExtras.h"
#include "llvm/Bitcode/NaCl/NaClObjDumpStream.h"

#include "gtest/gtest.h"

#include <iostream>

using namespace llvm;
using namespace llvm::naclbitc;

namespace {

/// Simple test harness for testing a text formatter. This class takes
/// an array of tokens, parses it, and then uses the text formatter to
/// format it. To test the features of the text formatter, the parser
/// detects function calls, and inserts appropriate open/close
/// parenthesis directives, as well as clustering directives.
///
/// For clustering, we consider each argument, as well as the entire
/// function. In case the entire function can't be printed, we add two
/// additional clusters as backup strategies:
///
/// 1) Cluster the called function with it's first argument.
/// 2) Cluster the called function with the open parenthesis.
///
/// These rules can be formalized as the following cases, where:
///    '<' denotes a StartCluster.
///    '>' denotes a FinishCluster.
///    '[' represents a regular expression open parenthesis.
///    ']' represents a regular expression close parenthesis.
///    '*' denotes regular expression repeat operation.
///
/// The cases to consider are:
///
/// case 1: <<<f(>)>>
/// case 2: <<<f(><x>)>>
/// case 3: <<<f(><x,>>[<y,>]*<z>)>
///
/// Note: In cases 1 and 2, there is an unnecessary pair of clustering
/// directives.  This is intentional. It has been added so that the
/// parser need not build an AST before formatting. Also note that
/// case 3 covers all function calls with 2 or more arguments.
class FormatterTester {
private:
  // Defines states of the token parser, as it looks for function calls.
  // These states are used to determine where (and when) clustering
  // directives should be added to the Tokens() stream. If no
  // transition applies for a state, the default transition is applied,
  // which is to add the next token to the Tokens() stream.
  //
  // Note: We use '|' to denote the current position of the token parser.
  //
  // In all states, the following transition is possible (pushing the
  // current state onto the parse stack), and is applied after state
  // specific transitions (below):
  //
  //    |<<<f(>)>>                 => StartingFcn:    <<<|f(>)>>
  //    |<<<f(><x>)>>              => StartingFcn:    <<<|f(><x>)>>
  //    |<<<f(><x,>>[<y,>]*<z>)>   => StartingFcn:    <<<|f(><x,>>[<y,>]*<z>)>
  //
  // Note: We use the notation == to state that two expressions are equivalent.
  // In particular,
  //
  //          [x|]* == [|x]*
  //
  // since being at the end of a repeated instruction also means that
  // you are at the beginning of the next (unrolled) repetition.
  enum FormatterState {
    LookingForFunction,            // Start state

    StartingFcn,
    // <<<f(|>)>>                  => BeforeFirstArg: <<<f(>|)>>
    // <<<f(|><x>)>>               => BeforeFirstArg: <<<f(>|<x>(>>
    // <<<f(|><x,>>[<y,>]*<z>)>    => BeforeFirstArg: <<<f(>|<x,>>[<y,>]*<z>)>

    BeforeFirstArg,
    // <<<f(>|)>>                   => EndFcn2:       <<<f(>|)>>
    // <<<f(>|<x>)>>                => InFirstArg:    <<<f(><|x>)>>
    // <<<f(>|<x,>>[<y,>]*<z>)>     => InFirstArg     <<<f(><|x,>>[<y,>]*<z>)>

    InFirstArg,
    // <<<f(><x|>)>>                => EndFcn2:       <<<f(><x>|)>>
    // <<<f(><x,|>>[<y,>]*<z>)>     => BetweenArgs:   <<<f(><x,>>[|<y,>]*<z>)>

    InOtherArg,
    // <<<f(><x,>>[<y,|>]*<z>)>     => BetweenArgs:   <<<f(><x,>>[<y,>|]*<z>)>
    //                                             == <<<f(><x,>>[|<y,>]*<z>)>
    //                              => BetweenArgs:   <<<f(><x,>>[<y,>]*|<z>)>
    // <<<f(><x,>>[<y,>]*<z|>)>     => EndFcn1:       <<<f(><x,>>[<y,>]*<z>|)>

    BetweenArgs,
    // <<<f(><x,>>[|<y,>]*<z>)>     => InOtherArg:    <<<f(><x,>>[<|y,>]*<z>)>
    // <<<f(><x,>>[<y,>]*|<z>)>     => InOtherArg:    <<<f(><x,>>[<y,>]*<|z>)>

    EndFcn2,
    // <<<f(>)|>>                   => EndFcn1:       <<<f(>)>|>
    // <<<f(><x>)|>>                => EndFcn1:       <<<f(><x>)>|>

    EndFcn1
    // <<<f(>)>|>                   => XXX:           <<<f(>)>>|
    // <<<f(><x>)>|>                => XXX:           <<<f(><x>)>>|
    // <<<f(><x,>>[<y,>]*<z>)|>     => XXX:           <<<f(><x,>>[<y,>]*<z>)>|
    //
    // where XXX is the state popped from the parse stack.
  };

public:
  FormatterTester(unsigned LineWidth)
      : AddOpenCloseDirectives(false),
        AddClusterDirectives(false),
        Buffer(),
        BufStream(Buffer),
        Formatter(BufStream, LineWidth, "  "),
        CommaText(","),
        SpaceText(" "),
        OpenParenText("("),
        CloseParenText(")"),
        NewlineText("\n"),
        Comma(&Formatter, CommaText),
        Space(&Formatter, SpaceText),
        OpenParen(&Formatter, OpenParenText),
        CloseParen(&Formatter, CloseParenText),
        StartCluster(&Formatter),
        FinishCluster(&Formatter),
        Tokenize(&Formatter),
        Endline(&Formatter)
  {
    Reset();
    Formatter.SetContinuationIndent(Formatter.GetIndent(2));
  }

  /// Runs a test using the given sequence of tokens. If
  /// AddOpenCloseDirectives is true, then "(" and ")" tokens
  /// will change the local indent using the corresponding directives.
  /// If AddClusterDirectives is true, then the clustering rules for
  /// function calls will be applied.
  std::string Test(const char *Tokens[], size_t NumTokens,
                   bool AddOpenCloseDirectives,
                   bool AddClusterDirectives,
                   unsigned Indent);

private:
  /// Reset the formatter for next test.
  void Reset() {
    BufStream.flush();
    Buffer.clear();
    State = LookingForFunction;
    FunctionParseStack.clear();
  }

  /// Collect the sequence of tokens, starting at Index, that
  /// correspond to spaces, and return the number of spaces found.
  unsigned CollectSpaces(size_t &Index,
                         const char *Tokens[],
                         size_t NumTokens) const {
    unsigned Count = 0;
    for (; Index < NumTokens; ++Index) {
      std::string Token(Tokens[Index]);
      if (Token != SpaceText) return Count;
      ++Count;
    }
    return Count;
  }

  /// Write out the given number of spaces using a space directive.
  void WriteSpaces(unsigned Count) {
    for (unsigned i = 0; i < Count; ++i) Formatter.Tokens() << Space;
  }

  /// Insert clustering directives, based on the current state of the
  /// parser. CurToken is the current (non-whitespace) token being
  /// processed by the parser. NextToken is the next (non-whitespace)
  /// token being processed.  If BeforeCurrentToken, then the parser
  /// is just before CurToken. Otherwise, it is just after NextToken.
  ///
  /// Note: When BeforeCurrentToken is false, it isn't necessarily
  /// just before NextToken. This is because there may be space
  /// (i.e. whitespace) tokens between CurToken and NextToken.
  void InsertClusterDirectives(std::string &CurToken,
                               std::string &NextToken,
                               bool BeforeCurrentToken);

  /// Write out the given token. Implicitly uses corresponding directives
  /// if applicable.
  void WriteToken(const std::string &Token);

  // True if Open and Close directives should be used for "(" and ")" tokens.
  bool AddOpenCloseDirectives;
  // True if clustering directives (for functions) should be inserted.
  bool AddClusterDirectives;
  // The buffer the formatted text is written into.
  std::string Buffer;
  // The base stream of the text formatter, which dumps text into the buffer.
  raw_string_ostream BufStream;
  // The text formatter to use.
  TextFormatter Formatter;
  const std::string CommaText;
  const std::string SpaceText;
  const std::string OpenParenText;
  const std::string CloseParenText;
  const std::string NewlineText;
  const TokenTextDirective Comma;
  SpaceTextDirective Space;
  OpenTextDirective OpenParen;
  CloseTextDirective CloseParen;
  StartClusteringDirective StartCluster;
  FinishClusteringDirective FinishCluster;
  TokenizeTextDirective Tokenize;
  EndlineTextDirective Endline;
  // The parse state of the function parser, used by the tester.
  FormatterState State;
  // The stack of parse states of the function parser. Used to handle
  // nested functions.
  std::vector<FormatterState> FunctionParseStack;
};

void FormatterTester::WriteToken(const std::string &Token) {
  if (Token == CommaText) {
    Formatter.Tokens() << Comma;
  } else if (Token == SpaceText) {
    Formatter.Tokens() << Space;
  } else if (Token == OpenParenText) {
    if (AddOpenCloseDirectives) {
      Formatter.Tokens() << OpenParen;
    } else {
      Formatter.Tokens() << Token << Tokenize;
    }
  } else if (Token == CloseParenText) {
    if (AddOpenCloseDirectives) {
      Formatter.Tokens() << CloseParen;
    } else {
      Formatter.Tokens() << Token << Tokenize;
    }
  } else if (Token == NewlineText) {
    Formatter.Tokens() << Endline;
  } else {
    Formatter.Tokens() << Token << Tokenize;
  }
}

std::string FormatterTester::Test(const char *Tokens[],
                                  size_t NumTokens,
                                  bool NewAddOpenCloseDirectives,
                                  bool NewAddClusterDirectives,
                                  unsigned Indent) {
  AddOpenCloseDirectives = NewAddOpenCloseDirectives;
  AddClusterDirectives = NewAddClusterDirectives;
  for (unsigned i = 0; i < Indent; ++i) Formatter.Inc();

  size_t Index = 0;
  unsigned SpaceCount = CollectSpaces(Index, Tokens, NumTokens);
  WriteSpaces(SpaceCount);
  SpaceCount = 0;

  // NOTE: We would use ASSERT_LT(Index, NumTokens), but it gets
  // a compile-time error if not inside the TEST macro.
  EXPECT_LT(Index, NumTokens);
  if (Index == NumTokens) return std::string("*W*R*O*N*G*");

  // Generate token sequence defined by Tokens.
  std::string CurToken(Tokens[Index++]);
  while (Index < NumTokens) {
    SpaceCount = CollectSpaces(Index, Tokens, NumTokens);
    if (Index == NumTokens) {
      WriteSpaces(SpaceCount);
      SpaceCount = 0;
      break;
    }
    std::string NextToken(Tokens[Index++]);
    InsertClusterDirectives(CurToken, NextToken, true);
    WriteToken(CurToken);
    InsertClusterDirectives(CurToken, NextToken, false);
    WriteSpaces(SpaceCount);
    SpaceCount = 0;
    CurToken = NextToken;
  }

  // When reached, all but last token (i.e. CurToken) has been written.
  // Create dummy newline token, so that the last token can be written.
  std::string NextToken(NewlineText);
  InsertClusterDirectives(CurToken, NextToken, true);
  WriteToken(CurToken);
  InsertClusterDirectives(CurToken, NextToken, false);
  WriteSpaces(SpaceCount);
  Formatter.Tokens() << Endline;

  EXPECT_TRUE(FunctionParseStack.empty())
      << "Missing close parenthesis in example";

  std::string Results = BufStream.str();
  Reset();
  return Results;
}

void FormatterTester::InsertClusterDirectives(std::string &CurToken,
                                              std::string &NextToken,
                                              bool BeforeCurToken) {
  if (!AddClusterDirectives) return;
  switch (State) {
  case LookingForFunction:
    break;
  case StartingFcn:
    if (!BeforeCurToken && CurToken == OpenParenText) {
      // context: <<<f(|>)>>
      // context: <<<f(|><x>)>>
      // context: <<<f(|><x,>><y,> ... <z>)>
      Formatter.Tokens() << FinishCluster;
      State = BeforeFirstArg;
    }
    break;
  case BeforeFirstArg:
    EXPECT_TRUE(BeforeCurToken)
        << "After open paren, but not before current token";
    if (CurToken == CloseParenText) {
      // <<<f(>|)>>                   => EndFcn2:       <<<f(>|)>>
      State = EndFcn2;
    } else {
      // <<<f(>|<x>)>>                => InFirstArg:    <<<f(><|x>)>>
      // <<<f(>|<x,>>[<y,>]*<z>)>     => InFirstArg     <<<f(><|x,>>[<y,>]*<z>)>
      State = InFirstArg;
      Formatter.Tokens() << StartCluster;
    }
    break;
  case InFirstArg:
    if (BeforeCurToken && CurToken == CloseParenText) {
      // <<<f(><x|>)>>                => EndFcn2:       <<<f(><x>|)>>
      Formatter.Tokens() << FinishCluster;
      State = EndFcn2;
    } else if (!BeforeCurToken && CurToken == CommaText) {
      // <<<f(><x,|>>[<y,>]*<z>)>     => BetweenArgs:   <<<f(><x,>>[|<y,>]*<z>)>
      Formatter.Tokens() << FinishCluster << FinishCluster;
      State = BetweenArgs;
    }
    break;
  case InOtherArg:
    if(BeforeCurToken && CurToken == CloseParenText) {
      // <<<f(><x,>>[<y,>]*<z|>)>     => EndFcn1:       <<<f(><x,>>[<y,>]*<z>|)>
      Formatter.Tokens() << FinishCluster;
      State = EndFcn1;
    } else if (!BeforeCurToken && CurToken == CommaText) {
      // <<<f(><x,>>[<y,|>]*<z>)>     => BetweenArgs:   <<<f(><x,>>[<y,>|]*<z>)>
      //                              => BetweenArgs:   <<<f(><x,>>[<y,>]*|<z>)>
      Formatter.Tokens() << FinishCluster;
      State = BetweenArgs;
    }
    break;
  case BetweenArgs:
    // <<<f(><x,>>[|<y,>]*<z>)>     => InOtherArg:    <<<f(><x,>>[<|y,>]*<z>)>
    // <<<f(><x,>>[<y,>]*|<z>)>     => InOtherArg:    <<<f(><x,>>[<y,>]*<|z>)>
    EXPECT_TRUE(BeforeCurToken)
        << "Expecting to be before next token after comma";
    Formatter.Tokens() << StartCluster;
    State = InOtherArg;
    break;
  case EndFcn2:
    // <<<f(>)|>>                   => EndFcn1:       <<<f(>)>|>
    // <<<f(><x>)|>>                => EndFcn1:       <<<f(><x>)>|>
    EXPECT_TRUE(!BeforeCurToken && CurToken == CloseParenText)
        << "Expecting to be after close paren";
    Formatter.Tokens() << FinishCluster;
    // Intentionally drop to the next case.
  case EndFcn1:
    // <<<f(>)>|>                   => XXX:           <<<f(>)>>|
    // <<<f(><x>)>|>                => XXX:           <<<f(><x>)>>|
    // <<<f(><x,>>[<y,>]*<z>)|>     => XXX:           <<<f(><x,>>[<y,>]*<z>)>|
    EXPECT_TRUE(!BeforeCurToken && CurToken == CloseParenText)
        << "Expecting to be after close paren";
    Formatter.Tokens() << FinishCluster;
    if (FunctionParseStack.empty()) {
      EXPECT_TRUE(false)
          << "No open paren for corresponding close paren";
      State = LookingForFunction;
    } else {
      State = FunctionParseStack.back();
      FunctionParseStack.pop_back();
    }
    break;
  default:
    EXPECT_TRUE(false) << "Formatter test state unknown: " << State;
    break;
  }

  // Check if we are at the beginning of a new function.
  if (BeforeCurToken && NextToken == OpenParenText) {
    // context: <<<|f(>)>>
    // context: <<<|f(><x>)>>
    // context: <<<|f(><x,>><y,> ... <z>)>
    Formatter.Tokens() << StartCluster << StartCluster << StartCluster;
    FunctionParseStack.push_back(State);
    State = StartingFcn;
  }
}

std::string RunTest(const char *Tokens[],
                    size_t NumTokens,
                    unsigned LineWidth,
                    bool AddOpenCloseDirectives,
                    bool AddClusterDirectives,
                    unsigned Indent = 0) {
  FormatterTester Tester(LineWidth);
  return Tester.Test(Tokens, NumTokens,
                     AddOpenCloseDirectives,
                     AddClusterDirectives,
                     Indent);
}

// Test simple single function call.
TEST(NaClTextFormatterTest, SimpleCall) {
  static const char *Tokens[] = {
    "foobar", "(", "Value1", ",", " ", "Value2", "," , " ", "Value3", ")"
  };

  // Print out simple call that can fit on one line.
  EXPECT_EQ(
      "foobar(Value1, Value2, Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 30, true, true));

  EXPECT_EQ(
      "foobar(Value1, Value2, Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 30, true, false));

  EXPECT_EQ(
      "foobar(Value1, Value2, Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 30, false, true));

  EXPECT_EQ(
      "foobar(Value1, Value2, Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 30, false, false));

  // Test case where it is one character too long (i.e ")" causes wrapping).
  EXPECT_EQ(
      "foobar(Value1, Value2, Value3\n"
      "       )\n",
      RunTest(Tokens, array_lengthof(Tokens), 29, true, true));

  EXPECT_EQ(
      "foobar(Value1, Value2, Value3\n"
      "       )\n",
      RunTest(Tokens, array_lengthof(Tokens), 29, true, false));

  EXPECT_EQ(
      "foobar(Value1, Value2, Value3\n"
      "    )\n",
      RunTest(Tokens, array_lengthof(Tokens), 29, false, true));

  EXPECT_EQ(
      "foobar(Value1, Value2, Value3\n"
      "    )\n",
      RunTest(Tokens, array_lengthof(Tokens), 29, false, false));

  // Test case where line length matches the beginning of "Value3".
  // Note: Only 3 indents for parenthesis directive, because we
  // stop indenting when there is only 20 columns left in the line
  // (i.e. 23 - 20 == 3).
  EXPECT_EQ(
      "foobar(Value1, Value2, \n"
      "   Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 23, true, true));

  EXPECT_EQ(
      "foobar(Value1, Value2, \n"
      "   Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 23, true, false));

  EXPECT_EQ(
      "foobar(Value1, Value2, \n"
      "   Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 23, false, true));

  EXPECT_EQ(
      "foobar(Value1, Value2, \n"
      "   Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 23, false, false));

  // Test case where line length matches the beginning of " Value3"
  // (i.e. the last test, but move the space to the next line).
  // Note: Only 2 indents for parenthesis directive, because we
  // stop indenting when there is only 20 columns left in the line
  // (i.e. 22 - 20 == 2).
  EXPECT_EQ(
      "foobar(Value1, Value2,\n"
      "  Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 22, true, true));

  EXPECT_EQ(
      "foobar(Value1, Value2,\n"
      "  Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 22, true, false));

  EXPECT_EQ(
      "foobar(Value1, Value2,\n"
      "  Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 22, false, true));

  EXPECT_EQ(
      "foobar(Value1, Value2,\n"
      "  Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 22, false, false));

  // Test case where last comma causes line wrap.
  EXPECT_EQ(
      "foobar(Value1, \n"
      " Value2, Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 21, true, true));

  EXPECT_EQ(
      "foobar(Value1, Value2\n"
      " , Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 21, true, false));

  EXPECT_EQ(
      "foobar(Value1, \n"
      " Value2, Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 21, false, true));

  EXPECT_EQ(
      "foobar(Value1, Value2\n"
      " , Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 21, false, false));

  // Test case where Value2 runs over the line width.
  EXPECT_EQ(
      "foobar(Value1, \n"
      "Value2, Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 20, true, true));

  EXPECT_EQ(
      "foobar(Value1, \n"
      "Value2, Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 20, true, false));

  EXPECT_EQ(
      "foobar(Value1, \n"
      "Value2, Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 20, false, true));

  EXPECT_EQ(
      "foobar(Value1, \n"
      "Value2, Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 20, false, false));

  // Run test where first comma (after value 1) causes line wrap.
  EXPECT_EQ(
      "foobar(\n"
      "Value1, \n"
      "Value2, \n"
      "Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 13, true, true));

  EXPECT_EQ(
      "foobar(Value1\n"
      ", Value2, \n"
      "Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 13, true, false));

  EXPECT_EQ(
      "foobar(\n"
      "Value1, \n"
      "Value2, \n"
      "Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 13, false, true));

  EXPECT_EQ(
      "foobar(Value1\n"
      ", Value2, \n"
      "Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 13, false, false));

  // Run test where only "foobar(" can fit on a line.
  EXPECT_EQ(
      "foobar(\n"
      "Value1,\n"
      "Value2,\n"
      "Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 7, true, true));

  EXPECT_EQ(
      "foobar(\n"
      "Value1,\n"
      "Value2,\n"
      "Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 7, true, false));

  EXPECT_EQ(
      "foobar(\n"
      "Value1,\n"
      "Value2,\n"
      "Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 7, false, true));

  EXPECT_EQ(
      "foobar(\n"
      "Value1,\n"
      "Value2,\n"
      "Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 7, false, false));

  // Run case where most tokens don't fit on a line.
  EXPECT_EQ(
      "foobar\n"
      "(\n"
      "Value1\n"
      ", \n"
      "Value2\n"
      ", \n"
      "Value3\n"
      ")\n",
      RunTest(Tokens, array_lengthof(Tokens), 4, true, true));

  EXPECT_EQ(
      "foobar\n"
      "(\n"
      "Value1\n"
      ", \n"
      "Value2\n"
      ", \n"
      "Value3\n"
      ")\n",
      RunTest(Tokens, array_lengthof(Tokens), 4, true, false));

  EXPECT_EQ(
      "foobar\n"
      "(\n"
      "Value1\n"
      ", \n"
      "Value2\n"
      ", \n"
      "Value3\n"
      ")\n",
      RunTest(Tokens, array_lengthof(Tokens), 4, false, true));

  EXPECT_EQ(
      "foobar\n"
      "(\n"
      "Value1\n"
      ", \n"
      "Value2\n"
      ", \n"
      "Value3\n"
      ")\n",
      RunTest(Tokens, array_lengthof(Tokens), 4, false, false));
}

// Test case where call isn't at the beginning of sequence of tokens.
TEST(NaClTextFormatterTest, TokensPlusSimpleCall) {
  static const char *Tokens[] = {
    "354", " ", "+", " ", "the", " ", "best", " ", "+", " ",
    "foobar", "(", "Value1", ",", " ", "Value2", "," , " ", "Value3", ")"
  };

  // Print out where all tokens fit on one line.
  EXPECT_EQ(
      "354 + the best + foobar(Value1, Value2, Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 47, true, true));

  EXPECT_EQ(
      "354 + the best + foobar(Value1, Value2, Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 47, true, false));

  EXPECT_EQ(
      "354 + the best + foobar(Value1, Value2, Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 47, false, true));

  EXPECT_EQ(
      "354 + the best + foobar(Value1, Value2, Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 47, false, false));

  // Format cases where buffer is one character too short to fit
  // all tokens.
  EXPECT_EQ(
      "354 + the best + \n"
      "    foobar(Value1, Value2, Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 46, true, true));

  EXPECT_EQ(
      "354 + the best + foobar(Value1, Value2, Value3\n"
      "                        )\n",
      RunTest(Tokens, array_lengthof(Tokens), 46, true, false));

  EXPECT_EQ(
      "354 + the best + \n"
      "    foobar(Value1, Value2, Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 46, false, true));

  EXPECT_EQ(
      "354 + the best + foobar(Value1, Value2, Value3\n"
      "    )\n",
      RunTest(Tokens, array_lengthof(Tokens), 46, false, false));

  // Show case where function call just fits on continuation line.
  EXPECT_EQ(
      "354 + the best + \n"
      "    foobar(Value1, Value2, Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 34, true, true));

  EXPECT_EQ(
      "354 + the best + foobar(Value1, \n"
      "              Value2, Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 34, true, false));

  EXPECT_EQ(
      "354 + the best + \n"
      "    foobar(Value1, Value2, Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 34, false, true));

  EXPECT_EQ(
      "354 + the best + foobar(Value1, \n"
      "    Value2, Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 34, false, false));

  // Show case were close parenthesis doesn't fit on continuation line.
  EXPECT_EQ(
      "354 + the best + \n"
      "    foobar(Value1, Value2, Value3\n"
      "           )\n",
      RunTest(Tokens, array_lengthof(Tokens), 33, true, true));

  EXPECT_EQ(
      "354 + the best + foobar(Value1, \n"
      "             Value2, Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 33, true, false));

  EXPECT_EQ(
      "354 + the best + \n"
      "    foobar(Value1, Value2, Value3\n"
      "    )\n",
      RunTest(Tokens, array_lengthof(Tokens), 33, false, true));

  EXPECT_EQ(
      "354 + the best + foobar(Value1, \n"
      "    Value2, Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 33, false, false));

  // Show case where "Value1," just fits on the first continuation line.
  EXPECT_EQ(
      "354 + the best\n"
      "+ \n"
      "foobar(Value1,\n"
      "Value2, Value3\n"
      ")\n",
      RunTest(Tokens, array_lengthof(Tokens), 14, true, true));

  EXPECT_EQ(
      "354 + the best\n"
      "+ foobar(\n"
      "Value1, Value2\n"
      ", Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 14, true, false));

  EXPECT_EQ(
      "354 + the best\n"
      "+ \n"
      "foobar(Value1,\n"
      "Value2, Value3\n"
      ")\n",
      RunTest(Tokens, array_lengthof(Tokens), 14, false, true));

  EXPECT_EQ(
      "354 + the best\n"
      "+ foobar(\n"
      "Value1, Value2\n,"
      " Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 14, false, false));

  // Show case where "Value1," moves to a new line.
  EXPECT_EQ(
      "354 + the \n"
      "best + \n"
      "foobar(\n"
      "Value1, \n"
      "Value2, \n"
      "Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 13, true, true));

  EXPECT_EQ(
      "354 + the \n"
      "best + foobar\n"
      "(Value1, \n"
      "Value2, \n"
      "Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 13, true, false));

  EXPECT_EQ(
      "354 + the \n"
      "best + \n"
      "foobar(\n"
      "Value1, \n"
      "Value2, \n"
      "Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 13, false, true));

  EXPECT_EQ(
      "354 + the \n"
      "best + foobar\n"
      "(Value1, \n"
      "Value2, \n"
      "Value3)\n",
      RunTest(Tokens, array_lengthof(Tokens), 13, false, false));

}

// Test case of nested functions.
TEST(NaClTextFormatterTest, NestedCalls) {
  static const char *Tokens[] = {
    "354", " ", "+", " ", "foo", "(", "g", "(", "blah", ")", ",", " ",
    "h", "(", ")", " ", "+", " ", "1", ")", " ", "+", " ", "10"
  };

  // Run test case where all text fits on one line.
  EXPECT_EQ(
      "354 + foo(g(blah), h() + 1) + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 32, true, true));

  EXPECT_EQ(
      "354 + foo(g(blah), h() + 1) + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 32, true, false));

  EXPECT_EQ(
      "354 + foo(g(blah), h() + 1) + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 32, false, true));

  EXPECT_EQ(
      "354 + foo(g(blah), h() + 1) + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 32, false, false));

  // Run test case where all text to end of top-level function call
  // fit on first line.
  EXPECT_EQ(
      "354 + foo(g(blah), h() + 1)\n"
      "    + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 27, true, true));

  EXPECT_EQ(
      "354 + foo(g(blah), h() + 1)\n"
      "    + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 27, true, false));

  EXPECT_EQ(
      "354 + foo(g(blah), h() + 1)\n"
      "    + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 27, false, true));

  EXPECT_EQ(
      "354 + foo(g(blah), h() + 1)\n"
      "    + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 27, false, false));

  // Run test where call to foo doesn't fit on first line.
  EXPECT_EQ(
      "354 + \n"
      "    foo(g(blah), h() + 1) \n"
      "    + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 26, true, true));

  EXPECT_EQ(
      "354 + foo(g(blah), h() + 1\n"
      "      ) + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 26, true, false));

  EXPECT_EQ(
      "354 + \n"
      "    foo(g(blah), h() + 1) \n"
      "    + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 26, false, true));

  EXPECT_EQ(
      "354 + foo(g(blah), h() + 1\n"
      "    ) + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 26, false, false));

  // Run test where call to foo doesn't fit on continuation line.
  EXPECT_EQ(
      "354 + \n"
      "    foo(g(blah), h() + 1\n"
      "    ) + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 24, true, true));

  EXPECT_EQ(
      "354 + foo(g(blah), h() +\n"
      "    1) + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 24, true, false));

  EXPECT_EQ(
      "354 + \n"
      "    foo(g(blah), h() + 1\n"
      "    ) + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 24, false, true));

  EXPECT_EQ(
      "354 + foo(g(blah), h() +\n"
      "    1) + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 24, false, false));

  // Run test where call to foo doesn't fit on continuation line.
  // Note: same as above, except for loss of continuation indent,
  // since we don't indent when printable space shrinks to 20.
  EXPECT_EQ(
      "354 + \n"
      "foo(g(blah), h() + 1\n"
      ") + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 20, true, true));

  EXPECT_EQ(
      "354 + foo(g(blah), h\n"
      "() + 1) + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 20, true, false));

  EXPECT_EQ(
      "354 + \n"
      "foo(g(blah), h() + 1\n"
      ") + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 20, false, true));

  EXPECT_EQ(
      "354 + foo(g(blah), h\n"
      "() + 1) + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 20, false, false));

  // Run case where first argument of foo (i.e. g(blah)) fits
  // on single continuation line.
  EXPECT_EQ(
      "354 + \n"
      "foo(g(blah), \n"
      "h() + 1) + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 19, true, true));

  EXPECT_EQ(
      "354 + foo(g(blah), \n"
      "h() + 1) + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 19, true, false));

  EXPECT_EQ(
      "354 + \n"
      "foo(g(blah), \n"
      "h() + 1) + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 19, false, true));

  EXPECT_EQ(
      "354 + foo(g(blah), \n"
      "h() + 1) + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 19, false, false));

  // Run case where no room for call to foo and its first argument.
  EXPECT_EQ(
      "354 + \n"
      "foo(\n"
      "g(blah), \n"
      "h() + 1) + \n"
      "10\n",
      RunTest(Tokens, array_lengthof(Tokens), 11, true, true));

  EXPECT_EQ(
      "354 + foo(g\n"
      "(blah), h()\n"
      "+ 1) + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 11, true, false));

  EXPECT_EQ(
      "354 + \n"
      "foo(\n"
      "g(blah), \n"
      "h() + 1) + \n"
      "10\n",
      RunTest(Tokens, array_lengthof(Tokens), 11, false, true));

  EXPECT_EQ(
      "354 + foo(g\n"
      "(blah), h()\n"
      "+ 1) + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 11, false, false));
}

// Test example with many arguments (which can't be printed on one line).
TEST(NaClTextFormatterTest, ManyArgs) {
  static const char *Tokens[] = {
    "10", " ", "+", " ", "f", "(",
    "g", "(", "a", ",", " ", "b", ")", ",", " ",
    "abcdef", " ", "+", " ", "gh1196", " ", "+", " ", "z", "(", ")", ",", " ",
    "53267", " ", "*", " ", "1234", " ", "+", " ", "567", ",", " ",
    "why", "(", "is", ",", " ", "this", ",", " ", "so", ",", " ", "hard",
    ",", " ", "to", ",", " ", "do", ")", ",", " ",
    "g", "(", "a", ",", " ", "b", ")", ",", " ",
    "abcdef", " ", "+", " ", "gh1196", " ", "+", " ", "z", "(", ")", ",", " ",
    "53267", " ", "*", " ", "1234", " ", "+", " ", "567", " ", "*", " ",
    "somemorestuff", ")", " ", "+", " ", "1"
  };

  // Show layout with linewidth 70
  EXPECT_EQ(
      "10 + \n"
      "    f(g(a, b), abcdef + gh1196 + z(), 53267 * 1234 + 567, \n"
      "      why(is, this, so, hard, to, do), g(a, b), abcdef + gh1196 + z(),\n"
      "      53267 * 1234 + 567 * somemorestuff) + 1\n",
      RunTest(Tokens, array_lengthof(Tokens), 70, true, true));

  EXPECT_EQ(
      "10 + f(g(a, b), abcdef + gh1196 + z(), 53267 * 1234 + 567, why(is, \n"
      "                                                  this, so, hard, to, \n"
      "                                                  do), g(a, b), abcdef\n"
      "       + gh1196 + z(), 53267 * 1234 + 567 * somemorestuff) + 1\n",
      RunTest(Tokens, array_lengthof(Tokens), 70, true, false));

  EXPECT_EQ(
      "10 + \n"
      "    f(g(a, b), abcdef + gh1196 + z(), 53267 * 1234 + 567, \n"
      "    why(is, this, so, hard, to, do), g(a, b), abcdef + gh1196 + z(), \n"
      "    53267 * 1234 + 567 * somemorestuff) + 1\n",
      RunTest(Tokens, array_lengthof(Tokens), 70, false, true));

  EXPECT_EQ(
      "10 + f(g(a, b), abcdef + gh1196 + z(), 53267 * 1234 + 567, why(is, \n"
      "    this, so, hard, to, do), g(a, b), abcdef + gh1196 + z(), 53267 * \n"
      "    1234 + 567 * somemorestuff) + 1\n",
      RunTest(Tokens, array_lengthof(Tokens), 70, false, false));

  // Show layout with linewidth 60
  EXPECT_EQ(
      "10 + \n"
      "    f(g(a, b), abcdef + gh1196 + z(), 53267 * 1234 + 567, \n"
      "      why(is, this, so, hard, to, do), g(a, b), \n"
      "      abcdef + gh1196 + z(), \n"
      "      53267 * 1234 + 567 * somemorestuff) + 1\n",
      RunTest(Tokens, array_lengthof(Tokens), 60, true, true));

  EXPECT_EQ(
      "10 + f(g(a, b), abcdef + gh1196 + z(), 53267 * 1234 + 567, \n"
      "       why(is, this, so, hard, to, do), g(a, b), abcdef + \n"
      "       gh1196 + z(), 53267 * 1234 + 567 * somemorestuff) + 1\n",
      RunTest(Tokens, array_lengthof(Tokens), 60, true, false));

  EXPECT_EQ(
      "10 + \n"
      "    f(g(a, b), abcdef + gh1196 + z(), 53267 * 1234 + 567, \n"
      "    why(is, this, so, hard, to, do), g(a, b), \n"
      "    abcdef + gh1196 + z(), \n"
      "    53267 * 1234 + 567 * somemorestuff) + 1\n",
      RunTest(Tokens, array_lengthof(Tokens), 60, false, true));

  EXPECT_EQ(
      "10 + f(g(a, b), abcdef + gh1196 + z(), 53267 * 1234 + 567, \n"
      "    why(is, this, so, hard, to, do), g(a, b), abcdef + \n"
      "    gh1196 + z(), 53267 * 1234 + 567 * somemorestuff) + 1\n",
      RunTest(Tokens, array_lengthof(Tokens), 60, false, false));

  // Show layout with linewidth 50.
  EXPECT_EQ(
      "10 + \n"
      "    f(g(a, b), abcdef + gh1196 + z(), \n"
      "      53267 * 1234 + 567, \n"
      "      why(is, this, so, hard, to, do), g(a, b), \n"
      "      abcdef + gh1196 + z(), \n"
      "      53267 * 1234 + 567 * somemorestuff) + 1\n",
      RunTest(Tokens, array_lengthof(Tokens), 50, true, true));

  EXPECT_EQ(
      "10 + f(g(a, b), abcdef + gh1196 + z(), 53267 * \n"
      "       1234 + 567, why(is, this, so, hard, to, do)\n"
      "       , g(a, b), abcdef + gh1196 + z(), 53267 * \n"
      "       1234 + 567 * somemorestuff) + 1\n",
      RunTest(Tokens, array_lengthof(Tokens), 50, true, false));

  EXPECT_EQ(
      "10 + \n"
      "    f(g(a, b), abcdef + gh1196 + z(), \n"
      "    53267 * 1234 + 567, \n"
      "    why(is, this, so, hard, to, do), g(a, b), \n"
      "    abcdef + gh1196 + z(), \n"
      "    53267 * 1234 + 567 * somemorestuff) + 1\n",
      RunTest(Tokens, array_lengthof(Tokens), 50, false, true));

  EXPECT_EQ(
      "10 + f(g(a, b), abcdef + gh1196 + z(), 53267 * \n"
      "    1234 + 567, why(is, this, so, hard, to, do), g\n"
      "    (a, b), abcdef + gh1196 + z(), 53267 * 1234 + \n"
      "    567 * somemorestuff) + 1\n",
      RunTest(Tokens, array_lengthof(Tokens), 50, false, false));

  // Show layout with linewidth 40
  EXPECT_EQ(
      "10 + \n"
      "    f(g(a, b), abcdef + gh1196 + z(), \n"
      "      53267 * 1234 + 567, \n"
      "      why(is, this, so, hard, to, do), \n"
      "      g(a, b), abcdef + gh1196 + z(), \n"
      "      53267 * 1234 + 567 * somemorestuff\n"
      "      ) + 1\n",
      RunTest(Tokens, array_lengthof(Tokens), 40, true, true));

  EXPECT_EQ(
      "10 + f(g(a, b), abcdef + gh1196 + z(), \n"
      "       53267 * 1234 + 567, why(is, this,\n"
      "                    so, hard, to, do), g\n"
      "       (a, b), abcdef + gh1196 + z(), \n"
      "       53267 * 1234 + 567 * \n"
      "       somemorestuff) + 1\n",
      RunTest(Tokens, array_lengthof(Tokens), 40, true, false));

  EXPECT_EQ(
      "10 + \n"
      "    f(g(a, b), abcdef + gh1196 + z(), \n"
      "    53267 * 1234 + 567, \n"
      "    why(is, this, so, hard, to, do), \n"
      "    g(a, b), abcdef + gh1196 + z(), \n"
      "    53267 * 1234 + 567 * somemorestuff) \n"
      "    + 1\n",
      RunTest(Tokens, array_lengthof(Tokens), 40, false, true));

  EXPECT_EQ(
      "10 + f(g(a, b), abcdef + gh1196 + z(), \n"
      "    53267 * 1234 + 567, why(is, this, so\n"
      "    , hard, to, do), g(a, b), abcdef + \n"
      "    gh1196 + z(), 53267 * 1234 + 567 * \n"
      "    somemorestuff) + 1\n",
      RunTest(Tokens, array_lengthof(Tokens), 40, false, false));

  // Show layout with linewidth 30
  EXPECT_EQ(
      "10 + \n"
      "    f(g(a, b), \n"
      "      abcdef + gh1196 + z(), \n"
      "      53267 * 1234 + 567, \n"
      "      why(is, this, so, hard, \n"
      "          to, do), g(a, b), \n"
      "      abcdef + gh1196 + z(), \n"
      "      53267 * 1234 + 567 * \n"
      "      somemorestuff) + 1\n",
      RunTest(Tokens, array_lengthof(Tokens), 30, true, true));

  EXPECT_EQ(
      "10 + f(g(a, b), abcdef + \n"
      "       gh1196 + z(), 53267 * \n"
      "       1234 + 567, why(is, \n"
      "          this, so, hard, to, \n"
      "          do), g(a, b), abcdef\n"
      "       + gh1196 + z(), 53267 *\n"
      "       1234 + 567 * \n"
      "       somemorestuff) + 1\n",
      RunTest(Tokens, array_lengthof(Tokens), 30, true, false));

  EXPECT_EQ(
      "10 + \n"
      "    f(g(a, b), \n"
      "    abcdef + gh1196 + z(), \n"
      "    53267 * 1234 + 567, \n"
      "    why(is, this, so, hard, \n"
      "    to, do), g(a, b), \n"
      "    abcdef + gh1196 + z(), \n"
      "    53267 * 1234 + 567 * \n"
      "    somemorestuff) + 1\n",
      RunTest(Tokens, array_lengthof(Tokens), 30, false, true));

  EXPECT_EQ(
      "10 + f(g(a, b), abcdef + \n"
      "    gh1196 + z(), 53267 * 1234\n"
      "    + 567, why(is, this, so, \n"
      "    hard, to, do), g(a, b), \n"
      "    abcdef + gh1196 + z(), \n"
      "    53267 * 1234 + 567 * \n"
      "    somemorestuff) + 1\n",
      RunTest(Tokens, array_lengthof(Tokens), 30, false, false));

  // Show layout with linewidth 20. Note: Continuation indents no
  // longer apply.
  EXPECT_EQ(
      "10 + \n"
      "f(g(a, b), \n"
      "abcdef + gh1196 + \n"
      "z(), \n"
      "53267 * 1234 + 567, \n"
      "why(is, this, so, \n"
      "hard, to, do), \n"
      "g(a, b), \n"
      "abcdef + gh1196 + \n"
      "z(), \n"
      "53267 * 1234 + 567 *\n"
      "somemorestuff) + 1\n",
      RunTest(Tokens, array_lengthof(Tokens), 20, true, true));

  EXPECT_EQ(
      "10 + f(g(a, b), \n"
      "abcdef + gh1196 + z(\n"
      "), 53267 * 1234 + \n"
      "567, why(is, this, \n"
      "so, hard, to, do), g\n"
      "(a, b), abcdef + \n"
      "gh1196 + z(), 53267 \n"
      "* 1234 + 567 * \n"
      "somemorestuff) + 1\n",
      RunTest(Tokens, array_lengthof(Tokens), 20, true, false));

  EXPECT_EQ(
      "10 + \n"
      "f(g(a, b), \n"
      "abcdef + gh1196 + \n"
      "z(), \n"
      "53267 * 1234 + 567, \n"
      "why(is, this, so, \n"
      "hard, to, do), \n"
      "g(a, b), \n"
      "abcdef + gh1196 + \n"
      "z(), \n"
      "53267 * 1234 + 567 *\n"
      "somemorestuff) + 1\n",
      RunTest(Tokens, array_lengthof(Tokens), 20, false, true));

  EXPECT_EQ(
      "10 + f(g(a, b), \n"
      "abcdef + gh1196 + z(\n"
      "), 53267 * 1234 + \n"
      "567, why(is, this, \n"
      "so, hard, to, do), g\n"
      "(a, b), abcdef + \n"
      "gh1196 + z(), 53267 \n"
      "* 1234 + 567 * \n"
      "somemorestuff) + 1\n",
      RunTest(Tokens, array_lengthof(Tokens), 20, false, false));

  // Show layout with linewidth 10, where some tokens ("somemorestuff")
  // exceed the line width requirement.
  EXPECT_EQ(
      "10 + \n"
      "f(g(a, b),\n"
      "abcdef + \n"
      "gh1196 + \n"
      "z(), \n"
      "53267 * \n"
      "1234 + 567\n"
      ", \n"
      "why(is, \n"
      "this, so, \n"
      "hard, to, \n"
      "do), \n"
      "g(a, b), \n"
      "abcdef + \n"
      "gh1196 + \n"
      "z(), \n"
      "53267 * \n"
      "1234 + 567\n"
      "* \n"
      "somemorestuff\n"
      ") + 1\n",
      RunTest(Tokens, array_lengthof(Tokens), 10, true, true));

  EXPECT_EQ(
      "10 + f(g(a\n"
      ", b), \n"
      "abcdef + \n"
      "gh1196 + z\n"
      "(), 53267 \n"
      "* 1234 + \n"
      "567, why(\n"
      "is, this, \n"
      "so, hard, \n"
      "to, do), g\n"
      "(a, b), \n"
      "abcdef + \n"
      "gh1196 + z\n"
      "(), 53267 \n"
      "* 1234 + \n"
      "567 * \n"
      "somemorestuff\n"
      ") + 1\n",
      RunTest(Tokens, array_lengthof(Tokens), 10, true, false));

  EXPECT_EQ(
      "10 + \n"
      "f(g(a, b),\n"
      "abcdef + \n"
      "gh1196 + \n"
      "z(), \n"
      "53267 * \n"
      "1234 + 567\n"
      ", \n"
      "why(is, \n"
      "this, so, \n"
      "hard, to, \n"
      "do), \n"
      "g(a, b), \n"
      "abcdef + \n"
      "gh1196 + \n"
      "z(), \n"
      "53267 * \n"
      "1234 + 567\n"
      "* \n"
      "somemorestuff\n"
      ") + 1\n",
      RunTest(Tokens, array_lengthof(Tokens), 10, false, true));

  EXPECT_EQ(
      "10 + f(g(a\n"
      ", b), \n"
      "abcdef + \n"
      "gh1196 + z\n"
      "(), 53267 \n"
      "* 1234 + \n"
      "567, why(\n"
      "is, this, \n"
      "so, hard, \n"
      "to, do), g\n"
      "(a, b), \n"
      "abcdef + \n"
      "gh1196 + z\n"
      "(), 53267 \n"
      "* 1234 + \n"
      "567 * \n"
      "somemorestuff\n"
      ") + 1\n",
      RunTest(Tokens, array_lengthof(Tokens), 10, false, false));
}

// Turn test case that checks if indenting works.
TEST(NaClTextFormatterTest, Indenting) {
  static const char *Tokens[] = {
    "354", " ", "+", " ", "foo", "(", "g", "(", "blah", ")", ",", " ",
    "h", "(", ")", " ", "+", " ", "1", ")", " ", "+", " ", "10"
  };

  // Run with no indentation.
  EXPECT_EQ(
      "354 + foo(g(blah), h() + 1) + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 32, true, true, 0));

  EXPECT_EQ(
      "354 + foo(g(blah), h() + 1) + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 32, true, false, 0));

  EXPECT_EQ(
      "354 + foo(g(blah), h() + 1) + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 32, false, true, 0));

  EXPECT_EQ(
      "354 + foo(g(blah), h() + 1) + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 32, false, false, 0));

  // Run with one indent.
  EXPECT_EQ(
      "  354 + foo(g(blah), h() + 1) + \n"
      "      10\n",
      RunTest(Tokens, array_lengthof(Tokens), 32, true, true, 1));

  EXPECT_EQ(
      "  354 + foo(g(blah), h() + 1) + \n"
      "      10\n",
      RunTest(Tokens, array_lengthof(Tokens), 32, true, false, 1));

  EXPECT_EQ(
      "  354 + foo(g(blah), h() + 1) + \n"
      "      10\n",
      RunTest(Tokens, array_lengthof(Tokens), 32, false, true, 1));

  EXPECT_EQ(
      "  354 + foo(g(blah), h() + 1) + \n"
      "      10\n",
      RunTest(Tokens, array_lengthof(Tokens), 32, false, false, 1));


  // Run with two indents.
  EXPECT_EQ(
      "    354 + foo(g(blah), h() + 1) \n"
      "        + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 32, true, true, 2));

  EXPECT_EQ(
      "    354 + foo(g(blah), h() + 1) \n"
      "        + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 32, true, false, 2));

  EXPECT_EQ(
      "    354 + foo(g(blah), h() + 1) \n"
      "        + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 32, false, true, 2));

  EXPECT_EQ(
      "    354 + foo(g(blah), h() + 1) \n"
      "        + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 32, false, false, 2));

  // Run with five indents.
  EXPECT_EQ(
      "          354 + \n"
      "            foo(g(blah), h() + 1\n"
      "            ) + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 32, true, true, 5));

  EXPECT_EQ(
      "          354 + foo(g(blah), h()\n"
      "            + 1) + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 32, true, false, 5));

  EXPECT_EQ(
      "          354 + \n"
      "            foo(g(blah), h() + 1\n"
      "            ) + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 32, false, true, 5));

  EXPECT_EQ(
      "          354 + foo(g(blah), h()\n"
      "            + 1) + 10\n",
      RunTest(Tokens, array_lengthof(Tokens), 32, false, false, 5));
}
}
