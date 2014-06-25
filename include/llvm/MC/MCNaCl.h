//===- MCNaCl.h - NaCl-specific code for MC  --------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/CommandLine.h"

namespace llvm {

class MCContext;
class MCStreamer;
class Triple;
/// Initialize target-specific bundle alignment and emit target-specific NaCl
/// ELF note sections.
void initializeNaClMCStreamer(MCStreamer &Streamer, MCContext &Ctx,
                              const Triple &TheTriple);

}
