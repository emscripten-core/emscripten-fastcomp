//===- NaClAsmParser.cpp - NaCl Assembly Parser ---------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/MC/MCParser/MCAsmParserExtension.h"
#include "llvm/MC/MCNaClExpander.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCTargetAsmParser.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

class NaClAsmParser : public MCAsmParserExtension {
  MCNaClExpander *Expander;
  template<bool (NaClAsmParser::*HandlerMethod)(StringRef, SMLoc)>
  void addDirectiveHandler(StringRef Directive) {
    MCAsmParser::ExtensionDirectiveHandler Handler = std::make_pair(
        this, HandleDirective<NaClAsmParser, HandlerMethod>);

    getParser().addDirectiveHandler(Directive, Handler);
  }

 public:
  NaClAsmParser(MCNaClExpander *Exp) : Expander(Exp) {}
  void Initialize(MCAsmParser &Parser) override {
    // Call the base implementation.
    MCAsmParserExtension::Initialize(Parser);
    addDirectiveHandler<&NaClAsmParser::ParseScratch>(".scratch");
    addDirectiveHandler<&NaClAsmParser::ParseUnscratch>(".unscratch");
  }

  /// ::= {.scratch} reg
  bool ParseScratch(StringRef Directive, SMLoc Loc) {
    getParser().checkForValidSection();
    unsigned RegNo;
    const char *kInvalidOptionError =
        "expected register name after '.scratch' directive";

    if (getLexer().isNot(AsmToken::EndOfStatement)) {
      if (getParser().getTargetParser().ParseRegister(RegNo, Loc, Loc))
        return Error(Loc, kInvalidOptionError);

      else if (getLexer().isNot(AsmToken::EndOfStatement))
        return Error(Loc, kInvalidOptionError);
    }
    else {
      return Error(Loc, kInvalidOptionError);
    }
    Lex();

    Expander->pushScratchReg(RegNo);
    return false;
  }

  /// ::= {.unscratch}
  bool ParseUnscratch(StringRef Directive, SMLoc Loc) {
    getParser().checkForValidSection();
    if (getLexer().isNot(AsmToken::EndOfStatement))
      return TokError("unexpected token in '.unscratch' directive");
    Lex();

    if (Expander->numScratchRegs() == 0)
      return Error(Loc, "No scratch registers specified");
    Expander->popScratchReg();

    return false;
  }
};

namespace llvm {
MCAsmParserExtension *createNaClAsmParser(MCNaClExpander *Exp) {
  return new NaClAsmParser(Exp);
}
}
