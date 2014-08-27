//===- NaClBitcodeWriterPass.cpp - Bitcode writing pass -------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// NaClBitcodeWriterPass implementation.
//
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/NaCl/NaClBitcodeWriterPass.h"
#include "llvm/Bitcode/NaCl/NaClReaderWriter.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
using namespace llvm;

PreservedAnalyses NaClBitcodeWriterPass::run(Module *M) {
  NaClWriteBitcodeToFile(M, OS);
  return PreservedAnalyses::all();
}

namespace {
  class NaClWriteBitcodePass : public ModulePass {
    raw_ostream &OS; // raw_ostream to print on
  public:
    static char ID; // Pass identification, replacement for typeid
    explicit NaClWriteBitcodePass(raw_ostream &o)
      : ModulePass(ID), OS(o) {}

    const char *getPassName() const override { return "NaCl Bitcode Writer"; }

    bool runOnModule(Module &M) override {
      NaClWriteBitcodeToFile(&M, OS);
      return false;
    }
  };
}

char NaClWriteBitcodePass::ID = 0;

ModulePass *llvm::createNaClBitcodeWriterPass(raw_ostream &Str) {
  return new NaClWriteBitcodePass(Str);
}
