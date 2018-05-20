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
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Target/TargetLowering.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/NaCl.h"
#include "llvm/IR/Dominators.h"
#include "llvm/Transforms/Utils/PromoteMemToReg.h"
#include <vector>
#include <set>
#include <list>

#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace {
  class LowerNonEmIntrinsics : public ModulePass {
    Module *TheModule;

  public:
    static char ID; // Pass identification, replacement for typeid
    explicit LowerNonEmIntrinsics() : ModulePass(ID), TheModule(NULL) {
      initializeLowerNonEmIntrinsicsPass(*PassRegistry::getPassRegistry());
    }
    bool runOnModule(Module &M);
  };
}

char LowerNonEmIntrinsics::ID = 0;
INITIALIZE_PASS(LowerNonEmIntrinsics, "LowerNonEmIntrinsics",
                "Lower intrinsics for libc calls for js/emscripten",
                false, false)

bool LowerNonEmIntrinsics::runOnModule(Module &M) {
  TheModule = &M;

  Type *f32 = Type::getFloatTy(M.getContext());
  Type *f64 = Type::getDoubleTy(M.getContext());

  bool Changed = false;

  for (auto Type : { f32, f64 }) {
    for (auto Name : { "cos", "exp", "log", "pow", "sin", "sqrt" }) {
      auto IntrinsicName = std::string("llvm.") + Name + '.' + (Type == f32 ? "f32" : "f64");
      if (auto* IntrinsicFunc = TheModule->getFunction(IntrinsicName)) {
        auto LibcName = std::string(name) + (Type == f32 ? "f" : "");
        auto* LibcFunc = TheModule->getFunction(LibcName);
        if (!LibcFunc) {
          SmallVector<Type*, 2> Types;
          Types.push_back(type);
          // Almost all of them take a single parameter.
          if (Name == "pow") {
            Types.push_back(type);
          }
          auto* FuncType = FunctionType::get(Type, Types, false);
          LibcFunc = Function::Create(FuncType, GlobalValue::ExternalLinkage, LibcName, TheModule);
        }
        IntrinsicFunc->replaceAllUsesWith(LibcFunc);
        Changed = true;
      }
    }
  }

  return Changed;
}

ModulePass *llvm::createLowerNonEmIntrinsicsPass() {
  return new LowerNonEmIntrinsics();
}
