//===- RenameEntryPoint.cpp - Rename _start to avoid linking collisions ---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// MinSFI compiles PNaCl bitcode into a native object file and links it into
// a standard C program. However, both C and PNaCl name their entry points
// '_start' which causes a linking collision. This pass therefore renames the
// entry function of the MinSFI module to '_start_minsfi'. By changing the name
// in the bitcode, we also avoid relying on objcopy.
//
//===----------------------------------------------------------------------===//

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/Transforms/MinSFI.h"

using namespace llvm;

static const char PNaClEntryPointName[] = "_start";
const char minsfi::EntryFunctionName[] = "_start_minsfi";

namespace {
class RenameEntryPoint : public ModulePass {
 public:
  static char ID;
  RenameEntryPoint() : ModulePass(ID) {
    initializeRenameEntryPointPass(*PassRegistry::getPassRegistry());
  }

  virtual bool runOnModule(Module &M);
};
}  // namespace

bool RenameEntryPoint::runOnModule(Module &M) {
  if (M.getNamedValue(minsfi::EntryFunctionName)) {
    report_fatal_error(std::string("RenameEntryPoint: The module already "
                       "contains a value named '") +
                       minsfi::EntryFunctionName + "'");
  }

  Function *EntryFunc = M.getFunction(PNaClEntryPointName);
  if (!EntryFunc) {
    report_fatal_error(std::string("RenameEntryPoint: The module does not "
                       "contain a function named '") +
                       PNaClEntryPointName + "'");
  }

  EntryFunc->setName(minsfi::EntryFunctionName);
  return true;
}

char RenameEntryPoint::ID = 0;
INITIALIZE_PASS(RenameEntryPoint, "minsfi-rename-entry-point",
                "Rename _start to avoid linking collisions", false, false)

ModulePass *llvm::createRenameEntryPointPass() {
  return new RenameEntryPoint();
}
