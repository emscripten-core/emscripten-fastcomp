//===- PNaClABIVerifyFunctions.cpp - Verify PNaCl ABI rules --------===//
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

#include "llvm/ADT/Twine.h"
#include "llvm/Function.h"
#include "llvm/Instruction.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Analysis/NaCl.h"
using namespace llvm;

namespace {

// Checks that examine anything in the function body should be in
// FunctionPasses to make them streaming-friendly
struct PNaClABIVerifyFunctions : public FunctionPass {
  static char ID;
  PNaClABIVerifyFunctions() : FunctionPass(ID) {}
  bool runOnFunction(Function &F);
  // For now, this print method exists to allow us to run the pass with
  // opt -analyze to avoid dumping the result to stdout, to make testing
  // simpler. In the future we will probably want to make it do something
  // useful.
  virtual void print(llvm::raw_ostream &O, const Module *M) const {};
};
} // and anonymous namespace

bool PNaClABIVerifyFunctions::runOnFunction(Function &F) {
  for (Function::const_iterator FI = F.begin(), FE = F.end();
           FI != FE; ++FI) {
    for (BasicBlock::const_iterator BBI = FI->begin(), BBE = FI->end();
             BBI != BBE; ++BBI) {
      switch (BBI->getOpcode()) {
        // Terminator instructions
        case Instruction::Ret:
        case Instruction::Br:
        case Instruction::Switch:
        case Instruction::Resume:
        case Instruction::Unreachable:
          // indirectbr is not allowed for now.
          // invoke and call are handled separately.
          break;
        default:
          errs() << Twine("Function ") + F.getName() +
              " has disallowed instruction: " +
              BBI->getOpcodeName() + "\n";
      }
    }
  }
  return false;
}

char PNaClABIVerifyFunctions::ID = 0;

static RegisterPass<PNaClABIVerifyFunctions> X("verify-pnaclabi-functions",
    "Verify functions for PNaCl", false, false);

FunctionPass *llvm::createPNaClABIVerifyFunctionsPass() {
  return new PNaClABIVerifyFunctions();
}
