//===- PNaClABIVerifyModule.h - Verify PNaCl ABI rules --------------------===//
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
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"

namespace llvm {

// Holds the set of allowed instrinsics.
class PNaClAllowedIntrinsics {
  LLVMContext *Context;
  // Maps from an allowed intrinsic's name to its type.
  StringMap<FunctionType *> Mapping;

  // Tys is an array of type parameters for the intrinsic.  This
  // defaults to an empty array.
  void addIntrinsic(Intrinsic::ID ID,
                    ArrayRef<Type *> Tys = ArrayRef<Type*>()) {
    Mapping[Intrinsic::getName(ID, Tys)] =
        Intrinsic::getType(*Context, ID, Tys);
  }
public:
  PNaClAllowedIntrinsics(LLVMContext *Context);
  bool isAllowed(const Function *Func);
};

// This pass should not touch function bodies, to stay streaming-friendly
class PNaClABIVerifyModule : public ModulePass {
  PNaClABIVerifyModule(const PNaClABIVerifyModule&) LLVM_DELETED_FUNCTION;
  void operator=(const PNaClABIVerifyModule&) LLVM_DELETED_FUNCTION;
 public:
  static char ID;
  PNaClABIVerifyModule() :
      ModulePass(ID),
      Reporter(new PNaClABIErrorReporter),
      ReporterIsOwned(true),
      SeenEntryPoint(false) {
    initializePNaClABIVerifyModulePass(*PassRegistry::getPassRegistry());
  }
  PNaClABIVerifyModule(PNaClABIErrorReporter *Reporter_,
                       bool StreamingMode,
                       bool RegisterPass) :
      ModulePass(ID),
      Reporter(Reporter_),
      ReporterIsOwned(false),
      StreamingMode(StreamingMode),
      SeenEntryPoint(false) {
    if (RegisterPass)
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
  // Checks validity of calling convention for function with given Name.
  void checkCallingConv(CallingConv::ID Conv, const StringRef &Name);
 private:
  void checkGlobalValue(const GlobalValue *GV);
  bool isWhitelistedMetadata(const NamedMDNode *MD) const;

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
