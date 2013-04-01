//===-- pnacl-abicheck.cpp - Check PNaCl bitcode ABI ----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This tool checks files for compliance with the PNaCl bitcode ABI
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/OwningPtr.h"
#include "llvm/Analysis/NaCl.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/IRReader.h"
#include <string>

using namespace llvm;

static cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<input bitcode>"), cl::init("-"));

static cl::opt<bool, false>
Quiet("q", cl::desc("Do not print error messages"));

// Print any errors collected by the error reporter. Return true if
// there were any.
static bool CheckABIVerifyErrors(PNaClABIErrorReporter &Reporter,
                                 const Twine &Name) {
  bool HasErrors = Reporter.getErrorCount() > 0;
  if (HasErrors) {
    if (!Quiet) {
      outs() << "ERROR: " << Name << " is not valid PNaCl bitcode:\n";
      Reporter.printErrors(outs());
    }
  }
  Reporter.reset();
  return HasErrors;
}

int main(int argc, char **argv) {
  LLVMContext &Context = getGlobalContext();
  SMDiagnostic Err;
  cl::ParseCommandLineOptions(argc, argv, "PNaCl Bitcode ABI checker\n");

  OwningPtr<Module> Mod(ParseIRFile(InputFilename, Err, Context));
  if (Mod.get() == 0) {
    Err.print(argv[0], errs());
    return 1;
  }
  PNaClABIErrorReporter ABIErrorReporter;
  ABIErrorReporter.setNonFatal();
  bool ErrorsFound = false;
  // Manually run the passes so we can tell the user which function had the
  // error. No need for a pass manager since it's just one pass.
  OwningPtr<ModulePass> ModuleChecker(createPNaClABIVerifyModulePass(&ABIErrorReporter));
  ModuleChecker->runOnModule(*Mod);
  ErrorsFound |= CheckABIVerifyErrors(ABIErrorReporter, "Module");
  OwningPtr<FunctionPass> FunctionChecker(
      createPNaClABIVerifyFunctionsPass(&ABIErrorReporter));
  for (Module::iterator MI = Mod->begin(), ME = Mod->end(); MI != ME; ++MI) {
    FunctionChecker->runOnFunction(*MI);
    ErrorsFound |= CheckABIVerifyErrors(ABIErrorReporter,
                                        "Function " + MI->getName());
  }

  return ErrorsFound;
}
