//===-- ARMMCNaCl.h - Prototype for CustomExpandInstNaClARM   ---*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef ARMMCNACL_H
#define ARMMCNACL_H
#include "llvm/MC/MCInst.h"

namespace llvm {
class MCStreamer;
class ARMMCNaClSFIState {
 public:
  static const int MaxSaved = 4;
  MCInst Saved[MaxSaved];
  int SaveCount;
  int I;
  bool RecursiveCall;
};
bool CustomExpandInstNaClARM(const MCInst &Inst, MCStreamer &Out,
                             ARMMCNaClSFIState &State);
}

#endif
