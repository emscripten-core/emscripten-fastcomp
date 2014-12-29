//===-- NaClBitcodeSubblockDist.cpp ---------------------------------------===//
//      implements distribution maps for subblock values within an
//      (externally specified) block.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/NaCl/NaClBitcodeSubblockDist.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeBlockDist.h"

using namespace llvm;

NaClBitcodeSubblockDistElement NaClBitcodeSubblockDist::DefaultSentinal;

NaClBitcodeSubblockDistElement::~NaClBitcodeSubblockDistElement() {}

NaClBitcodeDistElement *NaClBitcodeSubblockDistElement::
CreateElement(NaClBitcodeDistValue Value) const {
  return new NaClBitcodeSubblockDistElement();
}

const char *NaClBitcodeSubblockDistElement::GetTitle() const {
  return "Subblocks";
}

const char *NaClBitcodeSubblockDistElement::GetValueHeader() const {
  return "Subblock";
}

void NaClBitcodeSubblockDistElement::
PrintRowValue(raw_ostream &Stream, NaClBitcodeDistValue Value,
              const NaClBitcodeDist *Distribution) const {
  Stream << NaClBitcodeBlockDist::GetName(Value);
}

NaClBitcodeSubblockDist::~NaClBitcodeSubblockDist() {}
