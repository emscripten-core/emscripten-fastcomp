//===-- NaCl.h - NaCl Transformations ---------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_NACL_H
#define LLVM_TRANSFORMS_NACL_H

#include "llvm/CodeGen/Passes.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/LLVMContext.h"

namespace llvm {

class BasicBlockPass;
class Function;
class FunctionPass;
class FunctionType;
class Instruction;
class ModulePass;
class Triple;
class Use;
class Value;

BasicBlockPass *createConstantInsertExtractElementIndexPass();
BasicBlockPass *createExpandGetElementPtrPass();
BasicBlockPass *createExpandShuffleVectorPass();
BasicBlockPass *createFixVectorLoadStoreAlignmentPass();
BasicBlockPass *createPromoteI1OpsPass();
BasicBlockPass *createSimplifyAllocasPass();
FunctionPass *createBackendCanonicalizePass();
FunctionPass *createExpandConstantExprPass();
FunctionPass *createExpandLargeIntegersPass();
FunctionPass *createExpandStructRegsPass();
FunctionPass *createInsertDivideCheckPass();
FunctionPass *createNormalizeAlignmentPass();
FunctionPass *createRemoveAsmMemoryPass();
FunctionPass *createResolvePNaClIntrinsicsPass();
ModulePass *createAddPNaClExternalDeclsPass();
ModulePass *createCanonicalizeMemIntrinsicsPass();
ModulePass *createCleanupUsedGlobalsMetadataPass();
ModulePass *createExpandArithWithOverflowPass();
ModulePass *createExpandByValPass();
ModulePass *createExpandCtorsPass();
ModulePass *createExpandIndirectBrPass();
ModulePass *createExpandSmallArgumentsPass();
ModulePass *createExpandTlsConstantExprPass();
ModulePass *createExpandTlsPass();
ModulePass *createExpandVarArgsPass();
ModulePass *createFlattenGlobalsPass();
ModulePass *createGlobalCleanupPass();
ModulePass *createGlobalizeConstantVectorsPass();
ModulePass *createInternalizeUsedGlobalsPass();
ModulePass *createPNaClSjLjEHPass();
ModulePass *createPromoteIntegersPass();
ModulePass *createReplacePtrsWithIntsPass();
ModulePass *createResolveAliasesPass();
ModulePass *createRewriteAtomicsPass();
ModulePass *createRewriteLLVMIntrinsicsPass();
ModulePass *createRewritePNaClLibraryCallsPass();
ModulePass *createSimplifyStructRegSignaturesPass();
ModulePass *createStripAttributesPass();
ModulePass *createStripMetadataPass();
ModulePass *createStripModuleFlagsPass();
ModulePass *createStripDanglingDISubprogramsPass();

// Emscripten passes:
FunctionPass *createExpandInsertExtractElementPass();
ModulePass *createExpandI64Pass();
ModulePass *createLowerEmAsyncifyPass();
ModulePass *createLowerEmExceptionsPass();
ModulePass *createLowerEmSetjmpPass();
ModulePass *createNoExitRuntimePass();
// Emscripten passes end.

//void PNaClABISimplifyAddPreOptPasses(Triple *T, PassManagerBase &PM);
//void PNaClABISimplifyAddPostOptPasses(Triple *T, PassManagerBase &PM);

Instruction *PhiSafeInsertPt(Use *U);
void PhiSafeReplaceUses(Use *U, Value *NewVal);

// Copy debug information from Original to New, and return New.
template <typename T> T *CopyDebug(T *New, Instruction *Original) {
  New->setDebugLoc(Original->getDebugLoc());
  return New;
}

template <class InstType>
static void CopyLoadOrStoreAttrs(InstType *Dest, InstType *Src) {
  Dest->setVolatile(Src->isVolatile());
  Dest->setAlignment(Src->getAlignment());
  Dest->setOrdering(Src->getOrdering());
  Dest->setSynchScope(Src->getSynchScope());
}

// In order to change a function's type, the function must be
// recreated.  RecreateFunction() recreates Func with type NewType.
// It copies or moves across everything except the argument values,
// which the caller must update because the argument types might be
// different.
Function *RecreateFunction(Function *Func, FunctionType *NewType);

}

#endif
