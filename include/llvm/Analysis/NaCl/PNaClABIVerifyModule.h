//===- PNaClABIVerifyModule.h - Verify PNaCl ABI rules ----------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Verify module-level PNaCl ABI requirements (specifically those that do not
// require looking at the function bodies).
//
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_NACL_PNACLABIVERIFYMODULE_H
#define LLVM_ANALYSIS_NACL_PNACLABIVERIFYMODULE_H

#include "llvm/ADT/StringMap.h"
#include "llvm/Analysis/NaCl.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

namespace llvm {

class PNaClAllowedIntrinsics;

// This pass should not touch function bodies, to stay streaming-friendly
class PNaClABIVerifyModule : public ModulePass {
  PNaClABIVerifyModule(const PNaClABIVerifyModule&) = delete;
  void operator=(const PNaClABIVerifyModule&) = delete;
 public:
  static char ID;
  PNaClABIVerifyModule() :
      ModulePass(ID),
      Reporter(new PNaClABIErrorReporter),
      ReporterIsOwned(true),
      StreamingMode(false),
      SeenEntryPoint(false) {
    initializePNaClABIVerifyModulePass(*PassRegistry::getPassRegistry());
  }
  PNaClABIVerifyModule(PNaClABIErrorReporter *Reporter_,
                       bool StreamingMode) :
      ModulePass(ID),
      Reporter(Reporter_),
      ReporterIsOwned(false),
      StreamingMode(StreamingMode),
      SeenEntryPoint(false) {
    initializePNaClABIVerifyModulePass(*PassRegistry::getPassRegistry());
  }
  virtual ~PNaClABIVerifyModule();
  bool runOnModule(Module &M);
  virtual void print(raw_ostream &O, const Module *M) const;

  // Checks validity of function declaration F with given name Name.
  // (see PNaClABIVerifyFunctions.h for handling function bodies).
  void checkFunction(const Function *F, const StringRef &Name,
                     PNaClAllowedIntrinsics &Intrinsics);
  // Checks validity of global variable declaration GV.
  void checkGlobalVariable(const GlobalVariable *GV) {
    return checkGlobalValue(GV);
  }
 private:
  void checkGlobalValue(const GlobalValue *GV);
  /// Checks whether \p GV is an allowed external symbol in stable bitcode.
  void checkExternalSymbol(const GlobalValue *GV);

  void checkGlobalIsFlattened(const GlobalVariable *GV);
  PNaClABIErrorReporter *Reporter;
  bool ReporterIsOwned;
  bool StreamingMode;
  bool SeenEntryPoint;
};

}
#endif  // LLVM_ANALYSIS_NACL_PNACLABIVERIFYMODULE_H
