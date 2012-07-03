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

namespace llvm {
  class MCInst;
  class MCStreamer;
  bool CustomExpandInstNaClARM(const MCInst &Inst, MCStreamer &Out);
}

#endif
