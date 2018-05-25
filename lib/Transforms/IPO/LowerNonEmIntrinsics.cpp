//===- LowerNonEmIntrinsics - Lower non-emscripten stuff        -----------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Lowers LLVM intrinsics to libc calls where Emscripten needs that. For example,
// if LLVM has llvm.cos.f32 then lower that here to libc cosf, which then will
// get linked in properly. Otherwise, we need to link in those libc components
// after our final codegen, which requires a mechanism for that. We do have such
// a mechanism for the wasm backend, but not for asm.js and asm2wasm (for asm2wasm
// we could use the wasm backend one, but that would not solve things for asm.js
// which we need anyhow; but in theory for asm2wasm we could switch to the wasm
// backend).
//
// It makes sense to run this after optimizations, as the optimizer can do
// things with the intrinsics. However, LTO opts may be done later...
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/Scalar.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Transforms/NaCl.h"

#include "llvm/Support/raw_ostream.h"

#include "llvm/Transforms/IPO/LowerNonEmIntrinsics.h"

using namespace llvm;

#define DEBUG_TYPE "lower-non-em-intrinsics"

namespace {

  class LowerNonEmIntrinsics : public ModulePass {
    LowerNonEmIntrinsicsPass Impl;

  public:
    static char ID; // Pass identification

    LowerNonEmIntrinsics() : ModulePass(ID) {
      initializeLowerNonEmIntrinsicsPass(*PassRegistry::getPassRegistry());
    }

    bool runOnModule(Module &M) override {
      ModuleAnalysisManager MAM;
      Impl.run(M, MAM);
      return true; // XXX
    }
  };

} // end anonymous namespace

LowerNonEmIntrinsicsPass::LowerNonEmIntrinsicsPass() {}

PreservedAnalyses LowerNonEmIntrinsicsPass::run(Module &M, ModuleAnalysisManager &AM) {
  Type *f32 = Type::getFloatTy(M.getContext());
  Type *f64 = Type::getDoubleTy(M.getContext());

  // XXX bool Changed = false;

  for (auto T : { f32, f64 }) {
    for (std::string Name : { "cos", "exp", "log", "pow", "sin", "sqrt" }) {
      auto IntrinsicName = std::string("llvm.") + Name + '.' + (T == f32 ? "f32" : "f64");
      if (auto* IntrinsicFunc = M.getFunction(IntrinsicName)) {
        auto LibcName = std::string(Name) + (T == f32 ? "f" : "");
        auto* LibcFunc = M.getFunction(LibcName);
        if (!LibcFunc) {
          SmallVector<Type*, 2> Types;
          Types.push_back(T);
          // Almost all of them take a single parameter.
          if (Name == "pow") {
            Types.push_back(T);
          }
          auto* FuncType = FunctionType::get(T, Types, false);
          LibcFunc = Function::Create(FuncType, GlobalValue::ExternalLinkage, LibcName, &M);
        }
        IntrinsicFunc->replaceAllUsesWith(LibcFunc);
        IntrinsicFunc->eraseFromParent();
        // XXX Changed = true;
      }
    }
  }

  PreservedAnalyses PA;
  return PA;
}

char LowerNonEmIntrinsics::ID = 0;
INITIALIZE_PASS(LowerNonEmIntrinsics, "lower-non-em-intrinsics",
                "Lower intrinsics for libc calls for js/emscripten", false, false)

llvm::ModulePass *llvm::createLowerNonEmIntrinsicsPass() {
  return new LowerNonEmIntrinsics();
}
