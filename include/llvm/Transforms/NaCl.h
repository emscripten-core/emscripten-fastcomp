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
class FunctionPass;
class ModulePass;

FunctionPass *createExpandConstantExprPass();
ModulePass *createExpandCtorsPass();
BasicBlockPass *createExpandGetElementPtrPass();
ModulePass *createExpandTlsPass();
ModulePass *createExpandTlsConstantExprPass();
ModulePass *createExpandVarArgsPass();
ModulePass *createGlobalCleanupPass();
ModulePass *createResolveAliasesPass();
ModulePass *createStripMetadataPass();

}

#endif
