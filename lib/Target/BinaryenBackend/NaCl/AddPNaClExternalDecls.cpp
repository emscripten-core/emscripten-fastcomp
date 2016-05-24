//===- AddPNaClExternalDecls.cpp - Add decls for PNaCl external functions -===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This pass adds function declarations for external functions used by PNaCl.
// These externals are implemented in native libraries and calls to them are
// created as part of the translation process.
//
// Running this pass is a precondition for running ResolvePNaClIntrinsics. They
// are separate because one is a ModulePass and the other is a FunctionPass.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/NaClAtomicIntrinsics.h"
#include "llvm/IR/Type.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/NaCl.h"

using namespace llvm;

namespace {
  // This is a module pass because it adds declarations to the module.
  class AddPNaClExternalDecls : public ModulePass {
  public:
    static char ID;
    AddPNaClExternalDecls() : ModulePass(ID) {
      initializeAddPNaClExternalDeclsPass(*PassRegistry::getPassRegistry());
    }

    virtual bool runOnModule(Module &M);
  };
}

bool AddPNaClExternalDecls::runOnModule(Module &M) {
  // Add declarations for a pre-defined set of external functions to the module.
  // The function names must match the functions implemented in native code (in
  // pnacl/support). The function types must match the types of the LLVM
  // intrinsics.
  // We expect these declarations not to exist in the module before this pass
  // runs, but don't assert it; it will be handled by the ABI verifier.
  LLVMContext &C = M.getContext();
  M.getOrInsertFunction("setjmp",
                        // return type
                        Type::getInt32Ty(C),
                        // arguments
                        Type::getInt8Ty(C)->getPointerTo(),
                        NULL);
  M.getOrInsertFunction("longjmp",
                        // return type
                        Type::getVoidTy(C),
                        // arguments
                        Type::getInt8Ty(C)->getPointerTo(),
                        Type::getInt32Ty(C),
                        NULL);

  // Add Intrinsic declarations needed by ResolvePNaClIntrinsics up front.
  Intrinsic::getDeclaration(&M, Intrinsic::nacl_setjmp);
  Intrinsic::getDeclaration(&M, Intrinsic::nacl_longjmp);
  NaCl::AtomicIntrinsics AI(C);
  NaCl::AtomicIntrinsics::View V = AI.allIntrinsicsAndOverloads();
  for (NaCl::AtomicIntrinsics::View::iterator I = V.begin(), E = V.end();
       I != E; ++I) {
    I->getDeclaration(&M);
  }
  Intrinsic::getDeclaration(&M, Intrinsic::nacl_atomic_is_lock_free);

  return true;
}

char AddPNaClExternalDecls::ID = 0;
INITIALIZE_PASS(AddPNaClExternalDecls, "add-pnacl-external-decls",
                "Add declarations of external functions used by PNaCl",
                false, false)

ModulePass *llvm::createAddPNaClExternalDeclsPass() {
  return new AddPNaClExternalDecls();
}
