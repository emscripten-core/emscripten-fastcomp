//===-- NaClCompressBlockDist.cpp --------------------------------------------===//
//      Implements distribution maps used to collect block and record
//      distributions for tool pnacl-bccompress.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/NaCl/NaClCompressBlockDist.h"

using namespace llvm;

NaClCompressBlockDistElement::~NaClCompressBlockDistElement() {}

NaClBitcodeDistElement* NaClCompressBlockDistElement::
CreateElement(NaClBitcodeDistValue Value) const {
  return new NaClCompressBlockDistElement(Value);
}

const SmallVectorImpl<NaClBitcodeDist*> *NaClCompressBlockDistElement::
GetNestedDistributions() const {
  return &NestedDists;
}

NaClCompressBlockDistElement NaClCompressBlockDistElement::Sentinel;
