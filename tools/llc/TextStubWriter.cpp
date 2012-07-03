// Using the high-level representation of an ELF stub, create a text version
// of the ELF stub object.

#include "TextStubWriter.h"

#include <sstream>

#include "ELFStub.h"
#include "llvm/Support/ELF.h"

using namespace llvm;

namespace {

std::string LibShortname(const std::string &fullname) {
  std::string result = fullname;
  if (result.find("lib") != std::string::npos) {
    result = result.substr(3);
  }
  size_t so_pos = result.find(".so");
  if (so_pos != std::string::npos) {
    result = result.substr(0, so_pos);
  }
  return result;
}

const ELF::Elf32_Half kDummyCodeShndx = 5;
const ELF::Elf32_Half kDummyDataShndx = 6;

}  // namespace

namespace llvm {

// Write out the dynamic symbol table information.  The format must be kept
// in sync with the changes in NaCl's version of gold (see gold/metadata.cc).
void WriteTextELFStub(const ELFStub *Stub, std::string *output) {
  std::stringstream ss;

  ss << "#### Symtab for " << Stub->SOName << "\n";
  ss << "@obj " << LibShortname(Stub->SOName) << " " << Stub->SOName << "\n";

  // st_value is usually a relative address for .so, and .exe files.
  // So, make some up.
  ELF::Elf32_Addr fake_relative_addr = 0;
  for (size_t i = 0; i < Stub->Symbols.size(); ++i) {
    const SymbolStub &sym = Stub->Symbols[i];

    ELF::Elf32_Addr st_value = fake_relative_addr;
    ELF::Elf32_Word st_size = sym.Size;
    unsigned int st_info = sym.Type | (sym.Binding << 4);
    unsigned int st_other = sym.Visibility;
    ELF::Elf32_Half st_shndx = sym.Type == ELF::STT_FUNC ?
      kDummyCodeShndx : kDummyDataShndx;
    ELF::Elf32_Half vd_ndx = sym.VersionIndex;
    // Mark non-default versions hidden.
    if (!sym.IsDefault) {
      vd_ndx |= ELF::VERSYM_HIDDEN;
    }

    ss << "@sym "
       << sym.Name << " " // Representative for st_name.
       << (st_value) << " "
       << (st_size) << " "
       << (st_info) << " "
       << (st_other) << " "
       << (st_shndx) << " "
       << (vd_ndx) << " "
       << "\n";
    fake_relative_addr += (sym.Size == 0 ? 4 : sym.Size);
  }

  // Now dump the version map.
  ss << "#### VerDefs for " << Stub->SOName << "\n";
  for (size_t i = 0; i < Stub->VerDefs.size(); ++i) {
    const VersionDefinition &verdef = Stub->VerDefs[i];
    ss << "@ver " << (Elf32_Half)(verdef.Index) << " " << verdef.Name << "\n";
  }

  ss << "\n";

  output->append(ss.str());
}

} // namespace llvm
