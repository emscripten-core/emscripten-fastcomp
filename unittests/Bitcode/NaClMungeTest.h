//===- llvm/unittest/Bitcode/NaClMungeTest.h - Test munging utils ---------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// Contains common utilities used in bitcode munge tests.

#ifndef LLVM_UNITTEST_BITCODE_NACLMUNGETEST_H
#define LLVM_UNITTEST_BITCODE_NACLMUNGETEST_H

#include "llvm/ADT/STLExtras.h"
#include "llvm/Bitcode/NaCl/NaClBitcodeMunge.h"

#include "gtest/gtest.h"

namespace naclmungetest {

const uint64_t Terminator = 0x5768798008978675LL;

#define ARRAY(name) name, array_lengthof(name)

#define ARRAY_TERM(name) ARRAY(name), Terminator

inline std::string stringify(llvm::NaClMungedBitcode &MungedBitcode) {
  std::string Buffer;
  llvm::raw_string_ostream StrBuf(Buffer);
  MungedBitcode.print(StrBuf);
  return StrBuf.str();
}

inline std::string stringify(llvm::NaClBitcodeMunger &Munger) {
  return stringify(Munger.getMungedBitcode());
}

} // end of namespace naclmungetest

#endif // end LLVM_UNITTEST_BITCODE_NACLMUNGETEST_H
