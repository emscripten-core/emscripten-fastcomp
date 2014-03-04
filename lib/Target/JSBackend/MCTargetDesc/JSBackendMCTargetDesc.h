//===- JSBackendMCTargetDesc.h - JS Backend Target Descriptions -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file provides asm.js specific target descriptions.
//
//===----------------------------------------------------------------------===//

#ifndef JSBACKENDMCTARGETDESC_H
#define JSBACKENDMCTARGETDESC_H

#include "llvm/Support/TargetRegistry.h"

namespace llvm {

extern Target TheJSBackendTarget;

} // End llvm namespace

#endif
