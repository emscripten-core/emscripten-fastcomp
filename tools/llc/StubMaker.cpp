// Create a high-level representation of the needed library.

#include "StubMaker.h"

#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Triple.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Support/ELF.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/Debug.h"
#include "llvm/Module.h"
#include "llvm/Type.h"
#include "ELFStub.h"

using namespace llvm;

// Extract the Name, Version, and IsDefault flag from the FullName string.
// e.g. foo@V1  --> foo, V1, false
//      bar@@V2 --> bar, V2, true
static void ExtractVersion(StringRef FullName,
                           StringRef &Name,
                           StringRef &Version,
                           bool &IsDefault) {
  size_t atpos = FullName.find('@');
  if (atpos == StringRef::npos) {
    Name = FullName;
    Version = "";
    IsDefault = false;
    return;
  }
  Name = FullName.substr(0, atpos);
  ++atpos;
  if (FullName[atpos] == '@') {
    IsDefault = true;
    ++atpos;
  } else {
    IsDefault = false;
  }
  Version = FullName.substr(atpos);
}


// This implicitly creates a version record as a result of locating a symbol
// with this version. There is normally more information attached to a
// version definition: the parent version(s) and definition flags (weak
// or base). This information is currently not stored in the bitcode
// module. It may be necessary to add this in the future.
static Elf32_Half AddVersionDef(ELFStub *Stub, StringRef Name) {
  VersionDefinition VD;
  VD.Name = Name;
  VD.Index = Stub->NextIndex++;
  VD.IsWeak = false; // TODO(pdox): Implement
  VD.Parents.clear(); // TODO(pdox): Implement
  Stub->VerDefs.push_back(VD);
  Stub->IndexMap[VD.Name] = VD.Index;
  return VD.Index;
}

static Elf32_Half GetVersionIndex(StringRef Version, ELFStub *Stub) {
  // Handle unversioned symbols
  if (Version.empty())
    return 1; /* ELF::VER_NDX_GLOBAL */
  // Find the version definition, if it already exists.
  StringMap<Elf32_Half>::const_iterator I = Stub->IndexMap.find(Version);
  if (I != Stub->IndexMap.end()) {
    return I->second;
  }
  // If not, create it.
  return AddVersionDef(Stub, Version);
}

static Elf32_Half GetELFMachine(const Triple &T) {
  switch (T.getArch()) {
    default: llvm_unreachable("Unknown target triple in StubMaker.cpp");
    case Triple::x86_64: return ELF::EM_X86_64;
    case Triple::x86: return ELF::EM_386;
    case Triple::arm: return ELF::EM_ARM;
    case Triple::mipsel: return ELF::EM_MIPS;
  }
}

static unsigned char GetELFVisibility(const GlobalValue *GV) {
  switch (GV->getVisibility()) {
  case GlobalValue::DefaultVisibility: return ELF::STV_DEFAULT;
  case GlobalValue::HiddenVisibility: return ELF::STV_HIDDEN;
  case GlobalValue::ProtectedVisibility: return ELF::STV_PROTECTED;
  }
  llvm_unreachable("Unknown visibility in GETELFVisibility");
}

static ELF::Elf32_Word GetElfSizeForType(const GlobalValue *GV,
                                         const Type *ElemType) {
  unsigned bit_size = ElemType->getPrimitiveSizeInBits();
  if (bit_size != 0) {
    // Check against 0 to see if it was actually a primitive.
    return bit_size / 8;
  }
  if (isa<PointerType>(ElemType)) {
    // Pointers are 32-bit for NaCl.
    return 4;
  }
  if (isa<FunctionType>(ElemType)) {
    // This is not a data object, so just say unknown (0).
    return 0;
  }
  if (const ArrayType *ATy = dyn_cast<ArrayType>(ElemType)) {
    unsigned elem_size = GetElfSizeForType(GV, ATy->getElementType());
    unsigned num_elems = ATy->getNumElements();
    // TODO(jvoung): Come up with a test for what to do with 0-length arrays.
    // Not sure what to do here actually.  It may be that the 0-length
    // array is meant to be an opaque type, which you can never check the
    // "sizeof".  For now, return 0 instead of asserting.
    // Known instance of this in library code is in basic_string.h:
    //    static size_type _S_empty_rep_storage[];
    return elem_size * num_elems;
  }
  if (const VectorType *VTy = dyn_cast<VectorType>(ElemType)) {
    unsigned bit_width = VTy->getBitWidth();
    if (bit_width) {
      return bit_width / 8;
    } else {
      // It's a vector of pointers, and pointers are 32-bit in NaCl
      return VTy->getNumElements() * 4;
    }
  }
  if (const StructType *STy = dyn_cast<StructType>(ElemType)) {
    // Alignment padding should have been added to the type in the front-end.
    unsigned size_so_far = 0;
    for (unsigned i = 0; i < STy->getNumElements(); ++i) {
      size_so_far += GetElfSizeForType(GV, STy->getElementType(i));
    }
    return size_so_far;
  }
  // Unknown type!
  DEBUG({
      dbgs() << "Unknown GetELFSize for var=";
      GV->dump();
      dbgs() << " type= ";
      ElemType->dump();
      dbgs() << "\n";
    });
  llvm_unreachable("Unhandled type for GetELFSize");
  return 0;
}

// Return a value for the symbol table's st_size, which is the number of bytes
// in a data object.  Functions may report unknown size 0 (not data objects).
// This is known to be important for symbols that may sit in BSS
// with copy relocations (to know how much to copy).
static ELF::Elf32_Word GetELFSize(const GlobalValue *GV) {
  const class PointerType *PT = GV->getType();
  const Type *ElemType = PT->getElementType();
  return GetElfSizeForType(GV, ElemType);
}

static unsigned char GetELFType(const GlobalValue *GV) {
  if (const GlobalVariable *GVar = dyn_cast<GlobalVariable>(GV)) {
    return GVar->isThreadLocal() ? ELF::STT_TLS : ELF::STT_OBJECT;
  } else if (isa<Function>(GV)) {
    // TODO(pdox): Handle STT_GNU_IFUNC
    return ELF::STT_FUNC;
  }
  // TODO(pdox): Do we need to resolve GlobalAliases?
  llvm_unreachable("Unknown GlobalValue type in GetELFType!");
}

static unsigned char GetELFBinding(const GlobalValue *GV) {
  // TODO(pdox):
  // This information would ideally be made to match the symbol binding
  // as declared in the original shared object. However, GV is only the
  // declaration for this symbol, so we cannot derive the definition's
  // binding here. But it seems like it should be fine to always set it to
  // STB_GLOBAL, since we already know this symbol is the prevailing
  // definition.
  return ELF::STB_GLOBAL;
}

static void MakeOneStub(const Module &M,
                        const Module::NeededRecord &NR,
                        ELFStub *Stub) {
  Stub->SOName = NR.DynFile;
  Stub->NextIndex = 2; // 0,1 are reserved
  for (unsigned j = 0; j < NR.Symbols.size(); ++j) {
    StringRef FullName = NR.Symbols[j];
    GlobalValue *GV = M.getNamedValue(FullName);
    if (!GV) {
      // The symbol may have been removed by optimization or dead code
      // elimination, so this is not an error.
      continue;
    }
    StringRef Name;
    StringRef Version;
    bool IsDefault;
    ExtractVersion(FullName, Name, Version, IsDefault);

    SymbolStub SS;
    SS.Name = Name;
    SS.Type = GetELFType(GV);
    SS.Binding = GetELFBinding(GV);
    SS.Visibility = GetELFVisibility(GV);
    SS.Size = GetELFSize(GV);
    SS.VersionIndex = GetVersionIndex(Version, Stub);
    SS.IsDefault = IsDefault;
    Stub->Symbols.push_back(SS);
  }
}

namespace llvm {

// For module M, make all the stubs neededs and insert them into StubList.
void MakeAllStubs(const Module &M, const Triple &T,
                  SmallVectorImpl<ELFStub*> *StubList) {
  std::vector<Module::NeededRecord> NRList;
  M.getNeededRecords(&NRList);
  Elf32_Half Machine = GetELFMachine(T);
  for (unsigned i = 0; i < NRList.size(); ++i) {
    const Module::NeededRecord &NR = NRList[i];
    ELFStub *Stub = new ELFStub();
    Stub->Machine = Machine;
    MakeOneStub(M, NR, Stub);
    StubList->push_back(Stub);
  }
}

void FreeStubList(llvm::SmallVectorImpl<ELFStub*> *StubList) {
  for (unsigned i = 0; i < StubList->size(); ++i) {
    delete (*StubList)[i];
  }
  StubList->clear();
}

} // namespace
