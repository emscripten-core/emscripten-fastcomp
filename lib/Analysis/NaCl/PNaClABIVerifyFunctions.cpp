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
        // Disallowed instructions. Default is to disallow.
        default:
        // indirectbr may interfere with streaming
        case Instruction::IndirectBr:
        // No vector instructions yet
        case Instruction::ExtractElement:
        case Instruction::InsertElement:
        case Instruction::ShuffleVector:
          errs() << Twine("Function ") + F.getName() +
              " has disallowed instruction: " +
              BBI->getOpcodeName() + "\n";
          break;

        // Terminator instructions
        case Instruction::Ret:
        case Instruction::Br:
        case Instruction::Switch:
        case Instruction::Resume:
        case Instruction::Unreachable:
        case Instruction::Invoke:
        // Binary operations
        case Instruction::Add:
        case Instruction::FAdd:
        case Instruction::Sub:
        case Instruction::FSub:
        case Instruction::Mul:
        case Instruction::FMul:
        case Instruction::UDiv:
        case Instruction::SDiv:
        case Instruction::FDiv:
        case Instruction::URem:
        case Instruction::SRem:
        case Instruction::FRem:
        // Bitwise binary operations
        case Instruction::Shl:
        case Instruction::LShr:
        case Instruction::AShr:
        case Instruction::And:
        case Instruction::Or:
        case Instruction::Xor:
        case Instruction::ExtractValue:
        case Instruction::InsertValue:
        // Memory instructions
        case Instruction::Alloca:
        case Instruction::Load:
        case Instruction::Store:
        case Instruction::Fence:
        case Instruction::AtomicCmpXchg:
        case Instruction::AtomicRMW:
        case Instruction::GetElementPtr:
        // Conversion operations
        case Instruction::Trunc:
        case Instruction::ZExt:
        case Instruction::SExt:
        case Instruction::FPTrunc:
        case Instruction::FPExt:
        case Instruction::FPToUI:
        case Instruction::FPToSI:
        case Instruction::UIToFP:
        case Instruction::SIToFP:
        case Instruction::PtrToInt:
        case Instruction::IntToPtr:
        case Instruction::BitCast:
        // Other operations
        case Instruction::ICmp:
        case Instruction::FCmp:
        case Instruction::PHI:
        case Instruction::Select:
        case Instruction::Call:
        case Instruction::VAArg:
        case Instruction::LandingPad:
          break;
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
