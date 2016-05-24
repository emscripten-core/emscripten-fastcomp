//===-- Binaryen.h - Top-level interface for Binaryen representation ------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the entry points for global functions defined in the Binaryen
// target library, as used by the LLVM JIT.
//
//===----------------------------------------------------------------------===//

#ifndef TARGET_BINARYEN_H
#define TARGET_BINARYEN_H

namespace llvm {

class ImmutablePass;
class BinaryenTargetMachine;

/// createBinaryenISelDag - This pass converts a legalized DAG into a
/// \brief Creates an Binaryen-specific Target Transformation Info pass.
ImmutablePass *createBinaryenTargetTransformInfoPass(const BinaryenTargetMachine *TM);

} // End llvm namespace

#endif
