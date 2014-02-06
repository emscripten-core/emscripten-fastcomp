//===-- MipsMCNaCl.h - Prototype for CustomExpandInstNaClMips ---*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef MIPSMCNACL_H
#define MIPSMCNACL_H

#include "llvm/MC/MCInst.h"

namespace llvm {
class MCInst;
class MCStreamer;
class MipsMCNaClSFIState {
 public:
  static const int MaxSaved = 4;
  MCInst Saved[MaxSaved];
  int SaveCount;
  int I;
  bool RecursiveCall;
};

bool CustomExpandInstNaClMips(const MCInst &Inst, MCStreamer &Out,
                              MipsMCNaClSFIState &State);
}

#endif
