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
#include "llvm/MC/MCSectionELF.h"
#include "llvm/MC/MCStreamer.h"
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

  // Set bundle-alignment as required by the NaCl ABI for the target.
  Streamer.EmitBundleAlignMode(BundleAlign);

  // Emit an ELF Note section in its own COMDAT group which identifies NaCl
  // object files to the gold linker, so it can use the NaCl layout.
  const MCSection *Note = Ctx.getELFSection(
      NoteName, ELF::SHT_NOTE, ELF::SHF_ALLOC | ELF::SHF_GROUP,
      SectionKind::getReadOnly(), 0, NoteName);

  // TODO(dschuff) This should probably use PushSection and PopSection, but
  // PopSection will assert if there haven't been any other sections switched to
  // yet.
  Streamer.SwitchSection(Note);
  Streamer.EmitIntValue(strlen(NoteNamespace) + 1, 4);
  Streamer.EmitIntValue(strlen(NoteArch) + 1, 4);
  Streamer.EmitIntValue(ELF::NT_VERSION, 4);
  Streamer.EmitBytes(NoteNamespace);
  Streamer.EmitIntValue(0, 1); // NUL terminator
  Streamer.EmitValueToAlignment(4);
  Streamer.EmitBytes(NoteArch);
  Streamer.EmitIntValue(0, 1); // NUL terminator
  Streamer.EmitValueToAlignment(4);
}
} // namespace llvm
