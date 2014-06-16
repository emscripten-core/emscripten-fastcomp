//===- PNaClABIVerifyFunctions.h - Verify PNaCl ABI rules -----------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Verify function-level PNaCl ABI requirements.
//
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_NACL_PNACLABIVERIFYFUNCTIONS_H
#define LLVM_ANALYSIS_NACL_PNACLABIVERIFYFUNCTIONS_H

#include "llvm/ADT/APInt.h"
#include "llvm/ADT/OwningPtr.h"
#include "llvm/Analysis/NaCl.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/NaClAtomicIntrinsics.h"
#include "llvm/Pass.h"

namespace llvm {

// Checks that examine anything in the function body should be in
// FunctionPasses to make them streaming-friendly.
class PNaClABIVerifyFunctions : public FunctionPass {
  PNaClABIVerifyFunctions(const PNaClABIVerifyFunctions&) LLVM_DELETED_FUNCTION;
  void operator=(const PNaClABIVerifyFunctions&) LLVM_DELETED_FUNCTION;
 public:
  static char ID;
  PNaClABIVerifyFunctions() :
      FunctionPass(ID),
      Reporter(new PNaClABIErrorReporter),
      ReporterIsOwned(true) {
    initializePNaClABIVerifyFunctionsPass(*PassRegistry::getPassRegistry());
  }
  PNaClABIVerifyFunctions(PNaClABIErrorReporter *Reporter_,
                          bool RegisterPass) :
      FunctionPass(ID),
      Reporter(Reporter_),
      ReporterIsOwned(false) {
    if (RegisterPass)
      initializePNaClABIVerifyFunctionsPass(*PassRegistry::getPassRegistry());
  }
  virtual ~PNaClABIVerifyFunctions();
  virtual bool doInitialization(Module &M) {
    AtomicIntrinsics.reset(new NaCl::AtomicIntrinsics(M.getContext()));
    return false;
  }
  virtual void getAnalysisUsage(AnalysisUsage &Info) const {
    Info.setPreservesAll();
    Info.addRequired<DataLayout>();
  }
  bool runOnFunction(Function &F);
  virtual void print(raw_ostream &O, const Module *M) const;
  const char *verifyArithmeticType(Type *Ty) const;
  const char *verifyVectorIndexSafe(const APInt &Idx,
                                    unsigned NumElements) const {
    if (!Idx.ult(NumElements)) {
      return "out of range vector insert/extract index";
    }
    return 0;
  }
  bool isAllowedAlignment(const DataLayout *DL, uint64_t Alignment,
                          Type *Ty) const;
  // Returns 0 if valid. Otherwise returns an error message.
  const char *verifyAllocaAllocatedType(Type *Ty) const {
    if (!Ty->isIntegerTy(8))
      return "non-i8 alloca";
    return 0;
  }
  // Returns 0 if valid. Otherwise returns an error message.
  const char *verifyAllocaSizeType(Type *Ty) const {
    if (!Ty->isIntegerTy(32))
      return "alloca array size is not i32";
    return 0;
  }
  // Returns 0 if valid. Otherwise returns an error message.
  const char *verifyCallingConv(CallingConv::ID CallingConv) const {
    if (CallingConv != CallingConv::C)
      return "bad calling convention";
    return 0;
  }
  // Returns 0 if valid. Otherwise returns an error message.
  const char *verifySwitchConditionType(Type *Ty) const {
    if (!Ty->isIntegerTy())
      return "switch not on integer type";
    if (Ty->isIntegerTy(1))
      return "switch on i1";
    return 0;
  }

private:
  const char *checkInstruction(const DataLayout *DL, const Instruction *Inst);
  bool IsWhitelistedMetadata(unsigned MDKind);
  PNaClABIErrorReporter *Reporter;
  bool ReporterIsOwned;
  OwningPtr<NaCl::AtomicIntrinsics> AtomicIntrinsics;
};

}

#endif  // LLVM_ANALYSIS_NACL_PNACLABIVERIFYFUNCTIONS_H
