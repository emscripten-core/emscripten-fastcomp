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

#include "llvm/Pass.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Analysis/NaCl.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Support/raw_ostream.h"

#include "PNaClABITypeChecker.h"
using namespace llvm;

namespace {

// Checks that examine anything in the function body should be in
// FunctionPasses to make them streaming-friendly
class PNaClABIVerifyFunctions : public FunctionPass {
 public:
  static char ID;
  PNaClABIVerifyFunctions() :
      FunctionPass(ID),
      Reporter(new PNaClABIErrorReporter),
      ReporterIsOwned(true) {
    initializePNaClABIVerifyFunctionsPass(*PassRegistry::getPassRegistry());
  }
  explicit PNaClABIVerifyFunctions(PNaClABIErrorReporter *Reporter_) :
      FunctionPass(ID),
      Reporter(Reporter_),
      ReporterIsOwned(false) {
    initializePNaClABIVerifyFunctionsPass(*PassRegistry::getPassRegistry());
  }
  ~PNaClABIVerifyFunctions() {
    if (ReporterIsOwned)
      delete Reporter;
  }
  bool runOnFunction(Function &F);
  virtual void print(raw_ostream &O, const Module *M) const;
 private:
  PNaClABITypeChecker TC;
  PNaClABIErrorReporter *Reporter;
  bool ReporterIsOwned;
};

} // and anonymous namespace

bool PNaClABIVerifyFunctions::runOnFunction(Function &F) {
  // TODO: only report one error per instruction?
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
          Reporter->addError() << "Function " << F.getName() <<
              " has disallowed instruction: " <<
              BBI->getOpcodeName() << "\n";
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
      // Check the types. First check the type of the instruction.
      if (!TC.isValidType(BBI->getType())) {
        Reporter->addError() << "Function " << F.getName() <<
            " has instruction with disallowed type: " <<
            PNaClABITypeChecker::getTypeName(BBI->getType()) << "\n";
      }

      // Check the instruction operands. Operands which are Instructions will
      // be checked on their own here, and GlobalValues will be checked by the
      // Module verifier. That leaves Constants.
      // Switches are implemented in the in-memory IR with vectors, so don't
      // check them.
      if (!isa<SwitchInst>(*BBI))
        for (User::const_op_iterator OI = BBI->op_begin(), OE = BBI->op_end();
             OI != OE; OI++) {
          if (isa<Constant>(OI) && !isa<GlobalValue>(OI)) {
            Type *T = TC.checkTypesInConstant(cast<Constant>(*OI));
            if (T) {
              Reporter->addError() << "Function " << F.getName() <<
                  " has instruction operand with disallowed type: " <<
                  PNaClABITypeChecker::getTypeName(T) << "\n";
            }
          }
        }

      // Get types hiding in metadata attached to the instruction
      SmallVector<std::pair<unsigned, MDNode*>, 4> MDForInst;
      BBI->getAllMetadataOtherThanDebugLoc(MDForInst);
      for (unsigned i = 0, e = MDForInst.size(); i != e; i++) {
        Type *T = TC.checkTypesInMDNode(MDForInst[i].second);
        if (T) {
          Reporter->addError() << "Function " << F.getName() <<
              " has instruction metadata containing disallowed type: " <<
              PNaClABITypeChecker::getTypeName(T) << "\n";
        }
      }
    }
  }

  Reporter->checkForFatalErrors();
  return false;
}

// This method exists so that the passes can easily be run with opt -analyze.
// In this case the default constructor is used and we want to reset the error
// messages after each print.
void PNaClABIVerifyFunctions::print(llvm::raw_ostream &O, const Module *M)
    const {
  Reporter->printErrors(O);
  Reporter->reset();
}

char PNaClABIVerifyFunctions::ID = 0;
INITIALIZE_PASS(PNaClABIVerifyFunctions, "verify-pnaclabi-functions",
                "Verify functions for PNaCl", false, true)

FunctionPass *llvm::createPNaClABIVerifyFunctionsPass(
    PNaClABIErrorReporter *Reporter) {
  return new PNaClABIVerifyFunctions(Reporter);
}
