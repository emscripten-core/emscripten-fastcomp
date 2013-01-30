//===- PNaClABIVerifyModule.cpp - Verify PNaCl ABI rules --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Verify module-level PNaCl ABI requirements (specifically those that do not
// require looking at the function bodies)
//
//
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"
#include "llvm/Analysis/NaCl.h"
#include "llvm/ADT/Twine.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

#include "CheckTypes.h"
using namespace llvm;

namespace {

// This pass should not touch function bodies, to stay streaming-friendly
struct PNaClABIVerifyModule : public ModulePass {
  static char ID;
  PNaClABIVerifyModule() : ModulePass(ID) {}
  bool runOnModule(Module &M);
  // For now, this print method exists to allow us to run the pass with
  // opt -analyze to avoid dumping the result to stdout, to make testing
  // simpler. In the future we will probably want to make it do something
  // useful.
  virtual void print(llvm::raw_ostream &O, const Module *M) const {};
 private:
  // Ideally this would share an instance with the Function pass.
  // TODO: see if that's feasible when we add checking in bodies
  TypeChecker TC;
};

static const char* LinkageName(GlobalValue::LinkageTypes LT) {
  // This logic is taken from PrintLinkage in lib/VMCore/AsmWriter.cpp
  switch (LT) {
    case GlobalValue::ExternalLinkage: return "external";
    case GlobalValue::PrivateLinkage:       return "private ";
    case GlobalValue::LinkerPrivateLinkage: return "linker_private ";
    case GlobalValue::LinkerPrivateWeakLinkage: return "linker_private_weak ";
    case GlobalValue::InternalLinkage:      return "internal ";
    case GlobalValue::LinkOnceAnyLinkage:   return "linkonce ";
    case GlobalValue::LinkOnceODRLinkage:   return "linkonce_odr ";
    case GlobalValue::LinkOnceODRAutoHideLinkage:
      return "linkonce_odr_auto_hide ";
    case GlobalValue::WeakAnyLinkage:       return "weak ";
    case GlobalValue::WeakODRLinkage:       return "weak_odr ";
    case GlobalValue::CommonLinkage:        return "common ";
    case GlobalValue::AppendingLinkage:     return "appending ";
    case GlobalValue::DLLImportLinkage:     return "dllimport ";
    case GlobalValue::DLLExportLinkage:     return "dllexport ";
    case GlobalValue::ExternalWeakLinkage:  return "extern_weak ";
    case GlobalValue::AvailableExternallyLinkage:
      return "available_externally ";
    default:
      return "unknown";
  }
}

} // end anonymous namespace

bool PNaClABIVerifyModule::runOnModule(Module &M) {

  for (Module::const_global_iterator MI = M.global_begin(), ME = M.global_end();
       MI != ME; ++MI) {
    // Check types of global variables and their initializers
    if (!TC.IsValidType(MI->getType())) {
      errs() << (Twine("Variable ") + MI->getName() +
                 " has disallowed type: ");
      // GVs are pointers, so print the pointed-to type for clarity
      MI->getType()->getContainedType(0)->print(errs());
      errs() << "\n";
    } else if (MI->hasInitializer() &&
               !TC.CheckTypesInValue(MI->getInitializer())) {
      errs() << (Twine("Initializer for ") + MI->getName() +
                 " has disallowed type: ");
      MI->getInitializer()->print(errs());
      errs() << "\n";
    }

    // Check GV linkage types
    switch (MI->getLinkage()) {
      case GlobalValue::ExternalLinkage:
      case GlobalValue::AvailableExternallyLinkage:
      case GlobalValue::InternalLinkage:
      case GlobalValue::PrivateLinkage:
        break;
      default:
        errs() << (Twine("Variable ") + MI->getName() +
            " has disallowed linkage type: " +
                LinkageName(MI->getLinkage()) + "\n");
    }
  }
  // No aliases allowed for now.
  for (Module::alias_iterator MI = M.alias_begin(),
           E = M.alias_end(); MI != E; ++MI)
    errs() << (Twine("Variable ") + MI->getName() +
               " is an alias (disallowed)\n");

  return false;
}

char PNaClABIVerifyModule::ID = 0;

static RegisterPass<PNaClABIVerifyModule> X("verify-pnaclabi-module",
    "Verify module for PNaCl", false, false);

ModulePass *llvm::createPNaClABIVerifyModulePass() {
  return new PNaClABIVerifyModule();
}
