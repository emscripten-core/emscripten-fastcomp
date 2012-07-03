// This file describes a simple high-level representation of an ELF stub.

#ifndef __ELF_STUB_H
#define __ELF_STUB_H

#include <llvm/Support/ELF.h>
#include <llvm/ADT/StringMap.h>
#include <string>
#include <vector>

namespace llvm {

struct SymbolStub;
struct VersionDefinition;

using ELF::Elf32_Half;

struct ELFStub {
  Elf32_Half Machine;
  std::string SOName;
  std::vector<SymbolStub> Symbols;
  std::vector<VersionDefinition> VerDefs;

  // These are used for constructing the version definitions.
  // They are not directly emitted to the ELF stub.
  StringMap<Elf32_Half> IndexMap; // Maps version name to version index.
  Elf32_Half NextIndex;           // Next available version index
};


// Dynamic symbol entries
struct SymbolStub {
  // Symbol Table info.
  std::string Name;
  unsigned char Type; // STT_*
  unsigned char Binding; // STB_*
  unsigned char Visibility; // STV_*
  ELF::Elf32_Word Size; // Guess for st_size.
  // st_value, etc. are stubbed out.

  // Version info matching each of the symbols.
  Elf32_Half VersionIndex; // vd_ndx
  bool IsDefault;
};

// Versions defined in this module
struct VersionDefinition {
  Elf32_Half Index; // vd_ndx
  bool IsWeak; // TODO(pdox): Implement this (for vd_flags)
  std::string Name; // for vda_name, etc.
  std::vector<std::string> Parents; // TODO(pdox): Implement this
};

}
#endif
