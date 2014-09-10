//===- PNaClABIVerifyModule.cpp - Verify PNaCl ABI rules ------------------===//
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

#include "llvm/Analysis/NaCl/PNaClABIVerifyModule.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Analysis/NaCl/PNaClABIProps.h"
#include "llvm/Analysis/NaCl/PNaClABITypeChecker.h"
#include "llvm/Analysis/NaCl/PNaClAllowedIntrinsics.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace llvm {
cl::opt<bool>
PNaClABIAllowDebugMetadata("pnaclabi-allow-debug-metadata",
  cl::desc("Allow debug metadata during PNaCl ABI verification."),
  cl::init(false));

cl::opt<bool>
PNaClABIAllowMinsfiSyscalls("pnaclabi-allow-minsfi-syscalls",
  cl::desc("Allow undefined references to MinSFI syscall functions."),
  cl::init(false));

}

PNaClABIVerifyModule::~PNaClABIVerifyModule() {
  if (ReporterIsOwned)
    delete Reporter;
}

// MinSFI syscalls are functions with a given prefix which are left undefined
// and later linked against their implementation inside the trusted runtime.
// If the corresponding flag is set, do allow these external symbols in the
// module.
//
// We also require the syscall declarations to have an i32 return type. This
// is meant to prevent abusing syscalls to obtain an undefined value, e.g. by
// invoking a syscall whose trusted implementation returns void as a function
// which returns an integer, leaking the value of a register (see comments in
// the SubstituteUndefs pass for more information on undef values).
static bool isAllowedMinsfiSyscall(const Function *Func) {
  return PNaClABIAllowMinsfiSyscalls &&
         Func->getName().startswith("__minsfi_syscall_") &&
         Func->getReturnType()->isIntegerTy(32);
}

// Check linkage type and section attributes, which are the same for
// GlobalVariables and Functions.
void PNaClABIVerifyModule::checkGlobalValue(const GlobalValue *GV) {
  assert(!isa<GlobalAlias>(GV));
  const char *GVTypeName = PNaClABIProps::GVTypeName(isa<Function>(GV));
  GlobalValue::LinkageTypes Linkage = GV->getLinkage();
  if (!PNaClABIProps::isValidGlobalLinkage(Linkage)) {
    Reporter->addError() << GVTypeName << " " << GV->getName()
                         << " has disallowed linkage type: "
                         << PNaClABIProps::LinkageName(Linkage) << "\n";
  }
  if (Linkage == GlobalValue::ExternalLinkage) checkExternalSymbol(GV);
  if (GV->getVisibility() != GlobalValue::DefaultVisibility) {
    std::string Text = "unknown";
    if (GV->getVisibility() == GlobalValue::HiddenVisibility) {
      Text = "hidden";
    } else if (GV->getVisibility() == GlobalValue::ProtectedVisibility) {
      Text = "protected";
    }
    Reporter->addError() << GVTypeName << " " << GV->getName()
                         << " has disallowed visibility: " << Text << "\n";
  }
  if (GV->hasSection()) {
    Reporter->addError() << GVTypeName << " " << GV->getName() <<
        " has disallowed \"section\" attribute\n";
  }
  if (GV->getType()->getAddressSpace() != 0) {
    Reporter->addError() << GVTypeName << " " << GV->getName()
                         << " has addrspace attribute (disallowed)\n";
  }
  // The "unnamed_addr" attribute can be used to merge duplicate
  // definitions, but that should be done by user-toolchain
  // optimization passes, not by the PNaCl translator.
  if (GV->hasUnnamedAddr()) {
    Reporter->addError() << GVTypeName << " " << GV->getName()
                         << " has disallowed \"unnamed_addr\" attribute\n";
  }
}

void PNaClABIVerifyModule::checkExternalSymbol(const GlobalValue *GV) {
  if (const Function *Func = dyn_cast<const Function>(GV)) {
    if (Func->isIntrinsic() || isAllowedMinsfiSyscall(Func))
      return;
  }

  // We only allow __pnacl_pso_root to be a variable, not a function, to
  // reduce the number of cases that the translator needs to handle.
  bool ValidEntry =
      (isa<Function>(GV) && GV->getName().equals("_start")) ||
      (isa<GlobalVariable>(GV) && GV->getName().equals("__pnacl_pso_root"));
  if (!ValidEntry) {
    Reporter->addError()
        << GV->getName()
        << " is not a valid external symbol (disallowed)\n";
  } else {
    if (SeenEntryPoint) {
      Reporter->addError() <<
          "Module has multiple entry points (disallowed)\n";
    }
    SeenEntryPoint = true;
  }
}

static bool isPtrToIntOfGlobal(const Constant *C) {
  if (const ConstantExpr *CE = dyn_cast<ConstantExpr>(C)) {
    return CE->getOpcode() == Instruction::PtrToInt &&
           isa<GlobalValue>(CE->getOperand(0));
  }
  return false;
}

// This checks for part of the normal form produced by FlattenGlobals.
static bool isSimpleElement(const Constant *C) {
  // A SimpleElement is one of the following:
  // 1) An i8 array literal or zeroinitializer:
  //      [SIZE x i8] c"DATA"
  //      [SIZE x i8] zeroinitializer
  if (ArrayType *Ty = dyn_cast<ArrayType>(C->getType())) {
    return Ty->getElementType()->isIntegerTy(8) &&
           (isa<ConstantAggregateZero>(C) ||
            isa<ConstantDataArray>(C));
  }
  // 2) A reference to a GlobalValue (a function or global variable)
  //    with an optional byte offset added to it (the addend).
  if (C->getType()->isIntegerTy(32)) {
    const ConstantExpr *CE = dyn_cast<ConstantExpr>(C);
    if (!CE)
      return false;
    // Without addend:  ptrtoint (TYPE* @GLOBAL to i32)
    if (isPtrToIntOfGlobal(CE))
      return true;
    // With addend:  add (i32 ptrtoint (TYPE* @GLOBAL to i32), i32 ADDEND)
    if (CE->getOpcode() == Instruction::Add &&
        isPtrToIntOfGlobal(CE->getOperand(0)) &&
        isa<ConstantInt>(CE->getOperand(1)))
      return true;
  }
  return false;
}

// This checks for part of the normal form produced by FlattenGlobals.
static bool isCompoundElement(const Constant *C) {
  const ConstantStruct *CS = dyn_cast<ConstantStruct>(C);
  if (!CS || !CS->getType()->isPacked() || CS->getType()->hasName() ||
      CS->getNumOperands() <= 1)
    return false;
  for (unsigned I = 0; I < CS->getNumOperands(); ++I) {
    if (!isSimpleElement(CS->getOperand(I)))
      return false;
  }
  return true;
}

static std::string getAttributesAsString(AttributeSet Attrs) {
  std::string AttrsAsString;
  for (unsigned Slot = 0; Slot < Attrs.getNumSlots(); ++Slot) {
    for (AttributeSet::iterator Attr = Attrs.begin(Slot),
           E = Attrs.end(Slot); Attr != E; ++Attr) {
      AttrsAsString += " ";
      AttrsAsString += Attr->getAsString();
    }
  }
  return AttrsAsString;
}

// This checks that the GlobalVariable has the normal form produced by
// the FlattenGlobals pass.
void PNaClABIVerifyModule::checkGlobalIsFlattened(const GlobalVariable *GV) {
  if (!GV->hasInitializer()) {
    Reporter->addError() << "Global variable " << GV->getName()
                         << " has no initializer (disallowed)\n";
    return;
  }
  const Constant *InitVal = GV->getInitializer();
  if (isSimpleElement(InitVal) || isCompoundElement(InitVal))
    return;
  Reporter->addError() << "Global variable " << GV->getName()
                       << " has non-flattened initializer (disallowed): "
                       << *InitVal << "\n";
}

void PNaClABIVerifyModule::checkFunction(const Function *F,
                                         const StringRef &Name,
                                         PNaClAllowedIntrinsics &Intrinsics) {
  if (F->isIntrinsic()) {
    // Check intrinsics.
    if (!Intrinsics.isAllowed(F)) {
      Reporter->addError() << "Function " << F->getName()
                           << " is a disallowed LLVM intrinsic\n";
    }
  } else {
    // Check types of functions and their arguments.  Not necessary
    // for intrinsics, whose types are fixed anyway, and which have
    // argument types that we disallow such as i8.
    if (!PNaClABITypeChecker::isValidFunctionType(F->getFunctionType())) {
      Reporter->addError()
          << "Function " << Name << " has disallowed type: "
          << PNaClABITypeChecker::getTypeName(F->getFunctionType())
          << "\n";
    }
    // This check is disabled in streaming mode because it would
    // reject a function that is defined but not read in yet.
    // Unfortunately this means we simply don't check this property
    // when translating a pexe in the browser.
    // TODO(mseaborn): Enforce this property in the bitcode reader.
    if (!StreamingMode && F->isDeclaration() && !isAllowedMinsfiSyscall(F)) {
      Reporter->addError() << "Function " << Name
                           << " is declared but not defined (disallowed)\n";
    }
    if (!F->getAttributes().isEmpty()) {
      Reporter->addError()
          << "Function " << Name << " has disallowed attributes:"
          << getAttributesAsString(F->getAttributes()) << "\n";
    }
    if (!PNaClABIProps::isValidCallingConv(F->getCallingConv())) {
      Reporter->addError()
          << "Function " << Name << " has disallowed calling convention: "
          << PNaClABIProps::CallingConvName(F->getCallingConv()) << " ("
          << F->getCallingConv() << ")\n";
    }
  }

  checkGlobalValue(F);

  if (F->hasGC()) {
    Reporter->addError() << "Function " << Name <<
        " has disallowed \"gc\" attribute\n";
  }
  // Knowledge of what function alignments are useful is
  // architecture-specific and sandbox-specific, so PNaCl pexes
  // should not be able to specify function alignment.
  if (F->getAlignment() != 0) {
    Reporter->addError() << "Function " << Name <<
        " has disallowed \"align\" attribute\n";
  }
}

bool PNaClABIVerifyModule::runOnModule(Module &M) {
  SeenEntryPoint = false;
  PNaClAllowedIntrinsics Intrinsics(&M.getContext());

  if (!M.getModuleInlineAsm().empty()) {
    Reporter->addError() <<
        "Module contains disallowed top-level inline assembly\n";
  }

  for (Module::const_global_iterator MI = M.global_begin(), ME = M.global_end();
       MI != ME; ++MI) {
    checkGlobalIsFlattened(MI);
    checkGlobalVariable(MI);

    if (MI->isThreadLocal()) {
      Reporter->addError() << "Variable " << MI->getName() <<
          " has disallowed \"thread_local\" attribute\n";
    }
    if (MI->isExternallyInitialized()) {
      Reporter->addError() << "Variable " << MI->getName() <<
          " has disallowed \"externally_initialized\" attribute\n";
    }
  }

  // No aliases allowed for now.
  for (Module::alias_iterator MI = M.alias_begin(),
           E = M.alias_end(); MI != E; ++MI) {
    Reporter->addError() << "Variable " << MI->getName() <<
        " is an alias (disallowed)\n";
  }

  for (Module::const_iterator MI = M.begin(), ME = M.end(); MI != ME; ++MI) {
    checkFunction(MI, MI->getName(), Intrinsics);
  }

  // Check named metadata nodes
  for (Module::const_named_metadata_iterator I = M.named_metadata_begin(),
           E = M.named_metadata_end(); I != E; ++I) {
    if (!PNaClABIProps::isWhitelistedMetadata(I)) {
      Reporter->addError() << "Named metadata node " << I->getName()
                           << " is disallowed\n";
    }
  }

  if (!SeenEntryPoint) {
    Reporter->addError() << "Module has no entry point (disallowed)\n";
  }
  Reporter->checkForFatalErrors();
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
    PNaClABIErrorReporter *Reporter, bool StreamingMode) {
  return new PNaClABIVerifyModule(Reporter, StreamingMode);
}
