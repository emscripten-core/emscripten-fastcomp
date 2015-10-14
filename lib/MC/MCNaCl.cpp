//===- lib/MC/MCNaCl.cpp - NaCl-specific MC implementation ----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/MC/MCNaCl.h"
#include "llvm/ADT/Triple.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCNaClExpander.h"
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/ELF.h"

static const char NoteNamespace[] = "NaCl";

namespace llvm {

void initializeNaClMCStreamer(MCStreamer &Streamer, MCContext &Ctx,
                              const Triple &TheTriple) {
  assert(TheTriple.isOSNaCl());
  const char *NoteName;
  const char *NoteArch;
  unsigned BundleAlign;
  switch (TheTriple.getArch()) {
    case Triple::arm:
      NoteName = ".note.NaCl.ABI.arm";
      NoteArch = "arm";
      BundleAlign = 4;
      break;
    case Triple::mipsel:
      NoteName = ".note.NaCl.ABI.mipsel";
      NoteArch = "mipsel";
      BundleAlign = 4;
      break;
    case Triple::x86:
      NoteName = ".note.NaCl.ABI.x86-32";
      NoteArch = "x86-32";
      BundleAlign = 5;
      break;
    case Triple::x86_64:
      NoteName = ".note.NaCl.ABI.x86-64";
      NoteArch = "x86-64";
      BundleAlign = 5;
      break;
    default:
      report_fatal_error("Unsupported architecture for NaCl");
  }

  std::string Error; //empty
  const Target *TheTarget = 
    TargetRegistry::lookupTarget(TheTriple.getTriple(), Error);

  llvm_unreachable("code no longer maintained");
}

} // namespace llvm
