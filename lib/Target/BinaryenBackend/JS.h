//===-- JS.h - Top-level interface for JS representation ------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the entry points for global functions defined in the JS
// target library, as used by the LLVM JIT.
//
//===----------------------------------------------------------------------===//

#ifndef TARGET_JS_H
#define TARGET_JS_H

namespace llvm {

class ImmutablePass;
class JSTargetMachine;

/// createJSISelDag - This pass converts a legalized DAG into a
/// \brief Creates an JS-specific Target Transformation Info pass.
ImmutablePass *createJSTargetTransformInfoPass(const JSTargetMachine *TM);

} // End llvm namespace

#endif
