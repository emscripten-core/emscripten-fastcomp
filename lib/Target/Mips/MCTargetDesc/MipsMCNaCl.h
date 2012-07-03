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

namespace llvm {
  class MCInst;
  class MCStreamer;
  bool CustomExpandInstNaClMips(const MCInst &Inst, MCStreamer &Out);
}

#endif
