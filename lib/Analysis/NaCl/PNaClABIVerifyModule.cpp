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
#include "llvm/ADT/Twine.h"
#include "llvm/Analysis/NaCl.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

#include "PNaClABITypeChecker.h"
using namespace llvm;

namespace {
// This pass should not touch function bodies, to stay streaming-friendly
class PNaClABIVerifyModule : public ModulePass {
 public:
  static char ID;
  PNaClABIVerifyModule() :
      ModulePass(ID),
      Reporter(new PNaClABIErrorReporter),
      ReporterIsOwned(true) {
    initializePNaClABIVerifyModulePass(*PassRegistry::getPassRegistry());
  }
  explicit PNaClABIVerifyModule(PNaClABIErrorReporter *Reporter_) :
      ModulePass(ID),
      Reporter(Reporter_),
      ReporterIsOwned(false) {
    initializePNaClABIVerifyModulePass(*PassRegistry::getPassRegistry());
  }
  ~PNaClABIVerifyModule() {
    if (ReporterIsOwned)
      delete Reporter;
  }
  bool runOnModule(Module &M);
  virtual void print(raw_ostream &O, const Module *M) const;
 private:
  PNaClABITypeChecker TC;
  PNaClABIErrorReporter *Reporter;
  bool ReporterIsOwned;
};

static const char *linkageName(GlobalValue::LinkageTypes LT) {
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
    if (!TC.isValidType(MI->getType())) {
      // GVs are pointers, so print the pointed-to type for clarity
      Reporter->addError() << "Variable " << MI->getName() <<
          " has disallowed type: " <<
          PNaClABITypeChecker::getTypeName(MI->getType()->getContainedType(0))
          + "\n";
    } else if (MI->hasInitializer()) {
      // If the type of the global is bad, no point in checking its initializer
      Type *T = TC.checkTypesInConstant(MI->getInitializer());
      if (T) {
        Reporter->addError() << "Initializer for " << MI->getName() <<
            " has disallowed type: " <<
            PNaClABITypeChecker::getTypeName(T) << "\n";
      }
    }

    // Check GV linkage types
    switch (MI->getLinkage()) {
      case GlobalValue::ExternalLinkage:
      case GlobalValue::AvailableExternallyLinkage:
      case GlobalValue::InternalLinkage:
      case GlobalValue::PrivateLinkage:
        break;
      default:
        Reporter->addError() << "Variable " << MI->getName() <<
            " has disallowed linkage type: " <<
            linkageName(MI->getLinkage()) << "\n";
    }
  }
  // No aliases allowed for now.
  for (Module::alias_iterator MI = M.alias_begin(),
           E = M.alias_end(); MI != E; ++MI) {
    Reporter->addError() << "Variable " << MI->getName() <<
        " is an alias (disallowed)\n";
  }

  for (Module::iterator MI = M.begin(), ME = M.end(); MI != ME; ++MI) {
    // Check types of functions and their arguments
    FunctionType *FT = MI->getFunctionType();
    if (!TC.isValidType(FT->getReturnType())) {
      Reporter->addError() << "Function " << MI->getName() <<
          " has disallowed return type: " <<
          PNaClABITypeChecker::getTypeName(FT->getReturnType()) << "\n";
    }
    for (unsigned I = 0, E = FT->getNumParams(); I < E; ++I) {
      Type *PT = FT->getParamType(I);
      if (!TC.isValidType(PT)) {
        Reporter->addError() << "Function " << MI->getName() << " argument " <<
            I + 1 << " has disallowed type: " <<
            PNaClABITypeChecker::getTypeName(PT) << "\n";
      }
    }
  }

  // Check named metadata nodes
  for (Module::const_named_metadata_iterator I = M.named_metadata_begin(),
           E = M.named_metadata_end(); I != E; ++I) {
    for (unsigned i = 0, e = I->getNumOperands(); i != e; i++) {
      if (Type *T = TC.checkTypesInMDNode(I->getOperand(i))) {
        Reporter->addError() << "Named metadata node " << I->getName() <<
            " refers to disallowed type: " <<
            PNaClABITypeChecker::getTypeName(T) << "\n";
      }
    }
  }
  return false;
}

// This method exists so that the passes can easily be run with opt -analyze.
// In this case the default constructor is used and we want to reset the error
// messages after each print (this is more of an issue for the FunctionPass
// than the ModulePass)
void PNaClABIVerifyModule::print(llvm::raw_ostream &O, const Module *M) const {
  Reporter->printErrors(O);
  Reporter->reset();
}

char PNaClABIVerifyModule::ID = 0;
INITIALIZE_PASS(PNaClABIVerifyModule, "verify-pnaclabi-module",
                "Verify module for PNaCl", false, true)

ModulePass *llvm::createPNaClABIVerifyModulePass(
    PNaClABIErrorReporter *Reporter) {
  return new PNaClABIVerifyModule(Reporter);
}
