//===-- llvm/Bitcode/NaCl/NaClReaderWriter.h - ------------------*- C++ -*-===//
//      NaCl Bitcode reader/writer.
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This header defines interfaces to read and write LLVM bitcode files/streams.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_BITCODE_NACL_NACLREADERWRITER_H
#define LLVM_BITCODE_NACL_NACLREADERWRITER_H

namespace llvm {
  class Module;
  class raw_ostream;

  /// NaClWriteBitcodeToFile - Write the specified module to the
  /// specified raw output stream, using PNaCl wire format.  For
  /// streams where it matters, the given stream should be in "binary"
  /// mode.
  void NaClWriteBitcodeToFile(const Module *M, raw_ostream &Out);

} // end llvm namespace
#endif
