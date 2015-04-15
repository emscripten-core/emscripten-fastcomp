//===-- NaCl.h - NaCl Analysis ---------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_NACL_H
#define LLVM_ANALYSIS_NACL_H

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include <string>

namespace llvm {

class FunctionPass;
class ModulePass;
extern cl::opt<bool> PNaClABIAllowDebugMetadata;

class PNaClABIErrorReporter {
  PNaClABIErrorReporter(const PNaClABIErrorReporter&) = delete;
  void operator=(const PNaClABIErrorReporter&) = delete;
 public:
  PNaClABIErrorReporter() : ErrorCount(0), Errors(ErrorString),
                            UseFatalErrors(true) {}
  ~PNaClABIErrorReporter() {}
  // Return the number of verification errors from the last run.
  int getErrorCount() const { return ErrorCount; }
  // Print the error messages to O
  void printErrors(llvm::raw_ostream &O) {
    Errors.flush();
    O << ErrorString;
  }
  // Increments the error count and returns an ostream to which the error
  // message can be streamed.
  raw_ostream &addError() {
    ErrorCount++;
    return Errors;
  }
  // Reset the error count and error messages.
  void reset() {
    ErrorCount = 0;
    Errors.flush();
    ErrorString.clear();
  }
  void setNonFatal() {
    UseFatalErrors = false;
  }
  void checkForFatalErrors() {
    if (UseFatalErrors && ErrorCount != 0) {
      printErrors(errs());
      report_fatal_error("PNaCl ABI verification failed");
    }
  }
 private:
  int ErrorCount;
  std::string ErrorString;
  raw_string_ostream Errors;
  bool UseFatalErrors;
};

FunctionPass *createPNaClABIVerifyFunctionsPass(
    PNaClABIErrorReporter *Reporter);
ModulePass *createPNaClABIVerifyModulePass(PNaClABIErrorReporter *Reporter,
                                           bool StreamingMode = false);

}


#endif
