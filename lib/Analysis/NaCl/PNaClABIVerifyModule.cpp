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

#include "llvm/ADT/Twine.h"
#include "llvm/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/NaCl.h"
using namespace llvm;

namespace {

// This pass should not touch function bodies, to stay streaming-friendly
struct PNaClABIVerifyModule : public ModulePass {
  static char ID;
  PNaClABIVerifyModule() : ModulePass(ID) {}
  bool runOnModule(Module &M);
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
  // Check GV linkage types
  for (Module::const_global_iterator MI = M.global_begin(), ME = M.global_end();
       MI != ME; ++MI) {
    switch(MI->getLinkage()) {
      case GlobalValue::ExternalLinkage:
      case GlobalValue::AvailableExternallyLinkage:
      case GlobalValue::InternalLinkage:
      case GlobalValue::PrivateLinkage:
        break;
      default:
        errs() << (Twine("Variable ") + MI->getName() +
            " has Disallowed linkage type: " +
                LinkageName(MI->getLinkage()) + "\n");
    }
  }
  return false;
}

char PNaClABIVerifyModule::ID = 0;

static RegisterPass<PNaClABIVerifyModule> X("pnaclabi-module",
    "Verify module for PNaCl", false, false);

ModulePass *llvm::createPNaClABIVerifyModulePass() {
  return new PNaClABIVerifyModule();
}
