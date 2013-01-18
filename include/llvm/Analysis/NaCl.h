//===-- NaCl.h - NaCl Analysis ---------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_NACL_H
#define LLVM_ANALYSIS_NACL_H

namespace llvm {

class FunctionPass;
class ModulePass;

FunctionPass *createPNaClABIVerifyFunctionsPass();
ModulePass *createPNaClABIVerifyModulePass();

}

#endif
