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

#include "llvm/Analysis/NaCl.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Pass.h"
#include "llvm/PassManager.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/SourceMgr.h"
#include <string>

using namespace llvm;

static cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<input bitcode>"), cl::init("-"));

static cl::opt<bool>
Quiet("q", cl::desc("Do not print error messages"));

static cl::opt<NaClFileFormat>
InputFileFormat(
    "bitcode-format",
    cl::desc("Define format of input file:"),
    cl::values(
        clEnumValN(LLVMFormat, "llvm", "LLVM file (default)"),
        clEnumValN(PNaClFormat, "pnacl", "PNaCl bitcode file"),
        clEnumValEnd),
    cl::init(LLVMFormat));

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

  std::unique_ptr<Module> Mod(
      NaClParseIRFile(InputFilename, InputFileFormat, Err, Context));
  if (Mod.get() == 0) {
    Err.print(argv[0], errs());
    return 1;
  }
  PNaClABIErrorReporter ABIErrorReporter;
  ABIErrorReporter.setNonFatal();
  bool ErrorsFound = false;

  std::unique_ptr<ModulePass> ModuleChecker(
      createPNaClABIVerifyModulePass(&ABIErrorReporter));
  ModuleChecker->doInitialization(*Mod);
  ModuleChecker->runOnModule(*Mod);
  ErrorsFound |= CheckABIVerifyErrors(ABIErrorReporter, "Module");

  std::unique_ptr<FunctionPassManager> PM(new FunctionPassManager(&*Mod));
  PM->add(new DataLayoutPass(&*Mod));
  PM->add(createPNaClABIVerifyFunctionsPass(&ABIErrorReporter));

  PM->doInitialization();
  for (Module::iterator I = Mod->begin(), E = Mod->end(); I != E; ++I) {
    PM->run(*I);
    ErrorsFound |=
        CheckABIVerifyErrors(ABIErrorReporter, "Function " + I->getName());
  }
  PM->doFinalization();

  return ErrorsFound ? 1 : 0;
}
