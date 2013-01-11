/*===- pso-stub.c - Create bitcode shared object stubs  -------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Create a bitcode stub for a native shared object.
// Usage: pso-stub <input.so> -o <output.pso>
//
// The stub bitcode file contains the same dynamic symbols as the input shared
// object, with identical attributes (e.g. weak, undefined, TLS).
//
// Undefined functions become declarations in the bitcode.
// Undefined variables become external variable declarations in the bitcode.
// Defined functions become trivial stub functions in the bitcode (which do
// nothing but "ret void").
// Defined object/tls symbols became dummy variable definitions (int foo = 0).
//
// The generated bitcode is suitable for linking against (as a shared object),
// but nothing else.
//
// TODO(pdox): Implement GNU symbol versioning.
// TODO(pdox): Mark IFUNC symbols as functions, and store
//             this attribute as metadata.
//===----------------------------------------------------------------------===*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#include "llvm/GlobalValue.h"
#include "llvm/Type.h"
#include "llvm/DerivedTypes.h"
#include "llvm/Function.h"
#include "llvm/Constant.h"
#include "llvm/Constants.h"
#include "llvm/Instructions.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Object/ELF.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/ELF.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/ADT/APInt.h"

using namespace llvm;
using namespace llvm::object;

namespace {

cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<input native shared object>"),
              cl::init(""));

cl::opt<std::string>
OutputFilename("o", cl::desc("Output filename"), cl::value_desc("filename"));

// Variables / declarations to place in llvm.used array.
std::vector<GlobalValue*> LLVMUsed;

void AddUsedGlobal(GlobalValue *GV) {
  // Clang normally asserts that these are not decls.  We do need
  // decls to survive though, and those are really the ones we
  // worry about, so only add those.
  // We run verifyModule() below, so that we know this is somewhat valid.
  if (GV->isDeclaration()) {
    LLVMUsed.push_back(GV);
  }
}

// Emit llvm.used array.
// This is almost exactly like clang/lib/CodeGen/CodeGenModule.cpp::EmitLLVMUsed
void EmitLLVMUsed(Module *M) {
  // Don't create llvm.used if there is no need.
  if (LLVMUsed.empty())
    return;

  Type *Int8PtrTy = Type::getInt8PtrTy(M->getContext());
  // Convert LLVMUsed to what ConstantArray needs.
  SmallVector<llvm::Constant*, 8> UsedArray;
  UsedArray.resize(LLVMUsed.size());
  for (unsigned i = 0, e = LLVMUsed.size(); i != e; ++i) {
    UsedArray[i] =
     llvm::ConstantExpr::getBitCast(cast<llvm::Constant>(&*LLVMUsed[i]),
                                    Int8PtrTy);
  }

  if (UsedArray.empty())
    return;
  llvm::ArrayType *ATy = llvm::ArrayType::get(Int8PtrTy, UsedArray.size());

  llvm::GlobalVariable *GV =
    new llvm::GlobalVariable(*M, ATy, false,
                             llvm::GlobalValue::AppendingLinkage,
                             llvm::ConstantArray::get(ATy, UsedArray),
                             "llvm.used");

  GV->setSection("llvm.metadata");
}

// Add a stub function definition or declaration
void
AddFunction(Module *M,
            GlobalValue::LinkageTypes Linkage,
            const StringRef &Name,
            bool isDefine) {
  // Create an empty function with no arguments.
  // void Name(void);
  Type *RetTy = Type::getVoidTy(M->getContext());
  FunctionType *FT = FunctionType::get(RetTy, /*isVarArg=*/ false);
  Function *F = Function::Create(FT, Linkage, Name, M);
  if (isDefine) {
    // Add a single basic block with "ret void"
    BasicBlock *BB = BasicBlock::Create(F->getContext(), "", F);
    BB->getInstList().push_back(ReturnInst::Create(F->getContext()));
  }
  AddUsedGlobal(F);
}

// Add a stub global variable declaration or definition.
void
AddGlobalVariable(Module *M,
          GlobalValue::LinkageTypes Linkage,
          const StringRef &Name,
          bool isTLS,
          bool isDefine) {
  // Use 'int' as the dummy type.
  Type *Ty = Type::getInt32Ty(M->getContext());

  Constant *InitVal = NULL;
  if (isDefine) {
    // Define to dummy value, 0.
    InitVal = Constant::getIntegerValue(Ty, APInt(32, 0));
  }
  GlobalVariable *GV =
    new GlobalVariable(*M, Ty, /*isConstant=*/ false,
                       Linkage, /*Initializer=*/ InitVal,
                       Twine(Name), /*InsertBefore=*/ NULL,
                       isTLS ? GlobalVariable::GeneralDynamicTLSModel :
                               GlobalVariable::NotThreadLocal,
                       /*AddressSpace=*/ 0);
  AddUsedGlobal(GV);
}

// Iterate through the ObjectFile's needed libraries, and
// add them to the module.
void TransferLibrariesNeeded(Module *M, const ObjectFile *obj) {
  library_iterator it = obj->begin_libraries_needed();
  library_iterator ie = obj->end_libraries_needed();
  error_code ec;
  for (; it != ie; it.increment(ec)) {
    StringRef path;
    it->getPath(path);
    outs() << "Adding library " << path << "\n";
    M->addLibrary(path);
  }
}

// Set the Module's SONAME from the ObjectFile
void TransferLibraryName(Module *M, const ObjectFile *obj) {
  StringRef soname = obj->getLoadName();
  outs() << "Setting soname to: " << soname << "\n";
  M->setSOName(soname);
}

// Create stubs in the module for the dynamic symbols
void TransferDynamicSymbols(Module *M, const ObjectFile *obj) {
  // Iterate through the dynamic symbols in the ObjectFile.
  symbol_iterator it = obj->begin_dynamic_symbols();
  symbol_iterator ie = obj->end_dynamic_symbols();
  error_code ec;
  for (; it != ie; it.increment(ec)) {
    const SymbolRef &sym = *it;
    StringRef Name;
    SymbolRef::Type Type;
    uint32_t Flags;

    sym.getName(Name);
    sym.getType(Type);
    sym.getFlags(Flags);

    // Ignore debug info and section labels
    if (Flags & SymbolRef::SF_FormatSpecific)
      continue;

    // Ignore local symbols
    if (!(Flags & SymbolRef::SF_Global))
      continue;
    outs() << "Transferring symbol " << Name << "\n";

    bool isFunc = (Type == SymbolRef::ST_Function);
    bool isUndef = (Flags & SymbolRef::SF_Undefined);
    bool isTLS = (Flags & SymbolRef::SF_ThreadLocal);
    bool isCommon = (Flags & SymbolRef::SF_Common);
    bool isWeak = (Flags & SymbolRef::SF_Weak);

    if (Type == SymbolRef::ST_Unknown) {
      // Weak symbols can be "v" according to NM, which are definitely
      // data, but they may also be "w", which are of unknown type.
      // Thus there is already a mechanism to say "weak object", but not
      // for weak function.  Assume unknown weak symbols are functions.
      if (isWeak) {
        outs() << "Warning: Symbol '" << Name <<
            "' has unknown type (weak). Assuming function.\n";
        Type = SymbolRef::ST_Function;
        isFunc = true;
      } else {
        // If it is undef, we likely don't care, since it won't be used
        // to bind to unresolved symbols in the real pexe and real pso.
        // Other cases seen where it is not undef: _end, __bss_start,
        // which are markers provided by the linker scripts.
        outs() << "Warning: Symbol '" << Name <<
            "' has unknown type (isUndef=" << isUndef << "). Assuming data.\n";
        Type = SymbolRef::ST_Data;
        isFunc = false;
      }
    }

    // Determine Linkage type.
    GlobalValue::LinkageTypes Linkage;
    if (isWeak)
      Linkage = isUndef ? GlobalValue::ExternalWeakLinkage :
                          GlobalValue::WeakAnyLinkage;
    else if (isCommon)
      Linkage = GlobalValue::CommonLinkage;
    else
      Linkage = GlobalValue::ExternalLinkage;

    if (isFunc)
      AddFunction(M, Linkage, Name, !isUndef);
    else
      AddGlobalVariable(M, Linkage, Name, isTLS, !isUndef);
  }
}

}  // namespace


int main(int argc, const char** argv) {
  sys::PrintStackTraceOnErrorSignal();
  PrettyStackTraceProgram X(argc, argv);
  LLVMContext &Context = getGlobalContext();
  llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.

  cl::ParseCommandLineOptions(argc, argv,
                              "Portable Shared Object Stub Maker\n");

  if (InputFilename.empty()) {
    errs() << "Please specify an input filename\n";
    return 1;
  }
  if (OutputFilename.empty()) {
    errs() << "Please specify an output filename with -o\n";
    return 1;
  }

  // Open the object file
  OwningPtr<MemoryBuffer> File;
  if (MemoryBuffer::getFile(InputFilename, File)) {
    errs() << InputFilename << ": Open failed\n";
    return 1;
  }

  ObjectFile *obj = ObjectFile::createObjectFile(File.take());
  if (!obj) {
    errs() << InputFilename << ": Object type not recognized\n";
  }

  // Create the new module
  OwningPtr<Module> M(new Module(InputFilename, Context));

  // Transfer the relevant ELF information
  M->setOutputFormat(Module::SharedOutputFormat);
  TransferLibrariesNeeded(M.get(), obj);
  TransferLibraryName(M.get(), obj);
  TransferDynamicSymbols(M.get(), obj);
  EmitLLVMUsed(M.get());

  // Verify the module
  std::string Err;
  if (verifyModule(*M.get(), ReturnStatusAction, &Err)) {
    errs() << "Module created is invalid:\n";
    errs() << Err;
    return 1;
  }

  // Write the module to a file
  std::string ErrorInfo;
  OwningPtr<tool_output_file> Out(
      new tool_output_file(OutputFilename.c_str(), ErrorInfo,
                           raw_fd_ostream::F_Binary));
  if (!ErrorInfo.empty()) {
    errs() << ErrorInfo << '\n';
    return 1;
  }
  WriteBitcodeToFile(M.get(), Out->os());
  Out->keep();
  return 0;
}
