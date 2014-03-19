//===-- NaClCompressCodeDist.cpp ------------------------------------------===//
//      Implements distribution maps for record codes for pnacl-bccompress.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/NaCl/NaClCompressCodeDist.h"

using namespace llvm;

NaClCompressCodeDistElement::~NaClCompressCodeDistElement() {}

NaClBitcodeDistElement *NaClCompressCodeDistElement::CreateElement(
    NaClBitcodeDistValue Value) const {
  return new NaClCompressCodeDistElement();
}

void NaClCompressCodeDistElement::AddRecord(const NaClBitcodeRecord &Record) {
  NaClBitcodeCodeDistElement::AddRecord(Record);
  SizeDist.AddRecord(Record);
}

const SmallVectorImpl<NaClBitcodeDist*> *
NaClCompressCodeDistElement::GetNestedDistributions() const {
  return &NestedDists;
}

NaClCompressCodeDistElement NaClCompressCodeDistElement::Sentinel;
