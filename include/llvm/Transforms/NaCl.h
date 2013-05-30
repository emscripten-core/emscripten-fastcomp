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

namespace llvm {

class BasicBlockPass;
class Function;
class FunctionPass;
class FunctionType;
class Instruction;
class ModulePass;
class PassManager;
class Use;
class Value;

ModulePass *createAddPNaClExternalDeclsPass();
ModulePass *createExpandArithWithOverflowPass();
ModulePass *createExpandByValPass();
FunctionPass *createExpandConstantExprPass();
ModulePass *createExpandCtorsPass();
BasicBlockPass *createExpandGetElementPtrPass();
ModulePass *createExpandSmallArgumentsPass();
FunctionPass *createExpandStructRegsPass();
ModulePass *createExpandTlsPass();
ModulePass *createExpandTlsConstantExprPass();
ModulePass *createExpandVarArgsPass();
ModulePass *createFlattenGlobalsPass();
ModulePass *createGlobalCleanupPass();
FunctionPass *createPromoteIntegersPass();
ModulePass *createReplacePtrsWithIntsPass();
ModulePass *createResolveAliasesPass();
FunctionPass *createResolvePNaClIntrinsicsPass();
ModulePass *createRewritePNaClLibraryCallsPass();
ModulePass *createStripMetadataPass();
FunctionPass *createInsertDivideCheckPass();

void PNaClABISimplifyAddPreOptPasses(PassManager &PM);
void PNaClABISimplifyAddPostOptPasses(PassManager &PM);

Instruction *PhiSafeInsertPt(Use *U);
void PhiSafeReplaceUses(Use *U, Value *NewVal);

// Copy debug information from Original to NewInst, and return NewInst.
Instruction *CopyDebug(Instruction *NewInst, Instruction *Original);

// In order to change a function's type, the function must be
// recreated.  RecreateFunction() recreates Func with type NewType.
// It copies or moves across everything except the argument values,
// which the caller must update because the argument types might be
// different.
Function *RecreateFunction(Function *Func, FunctionType *NewType);

// Given a value of struct type, StructVal, this replaces all uses of
// StructVal with the given struct fields.  This involves replacing
// extractvalue instructions that refer to StructVal.
void ReplaceUsesOfStructWithFields(Value *StructVal,
                                   const SmallVectorImpl<Value *> &Fields);

}

#endif
