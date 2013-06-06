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
// This header defines interfaces to read and write NaCl bitcode wire format
// files.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_BITCODE_NACL_NACLREADERWRITER_H
#define LLVM_BITCODE_NACL_NACLREADERWRITER_H

#include <string>

namespace llvm {
  class MemoryBuffer;
  class DataStreamer;
  class LLVMContext;
  class Module;
  class raw_ostream;

  /// getNaClLazyBitcodeModule - Read the header of the specified bitcode buffer
  /// and prepare for lazy deserialization of function bodies.  If successful,
  /// this takes ownership of 'buffer' and returns a non-null pointer.  On
  /// error, this returns null, *does not* take ownership of Buffer, and fills
  /// in *ErrMsg with an error description if ErrMsg is non-null.
  Module *getNaClLazyBitcodeModule(MemoryBuffer *Buffer,
                                   LLVMContext &Context,
                                   std::string *ErrMsg = 0,
                                   bool AcceptSupportedOnly = true);

  /// getNaClStreamedBitcodeModule - Read the header of the specified stream
  /// and prepare for lazy deserialization and streaming of function bodies.
  /// On error, this returns null, and fills in *ErrMsg with an error
  /// description if ErrMsg is non-null.
  Module *getNaClStreamedBitcodeModule(const std::string &name,
                                       DataStreamer *streamer,
                                       LLVMContext &Context,
                                       std::string *ErrMsg = 0,
                                       bool AcceptSupportedOnly = true);

  /// NaClParseBitcodeFile - Read the specified bitcode file,
  /// returning the module.  If an error occurs, this returns null and
  /// fills in *ErrMsg if it is non-null.  This method *never* takes
  /// ownership of Buffer.
  Module *NaClParseBitcodeFile(MemoryBuffer *Buffer, LLVMContext &Context,
                               std::string *ErrMsg = 0,
                               bool AcceptSupportedOnly = true);

  /// NaClWriteBitcodeToFile - Write the specified module to the
  /// specified raw output stream, using PNaCl wire format.  For
  /// streams where it matters, the given stream should be in "binary"
  /// mode.
  void NaClWriteBitcodeToFile(const Module *M, raw_ostream &Out);

  /// isNaClBitcode - Return true if the given bytes are the magic bytes for
  /// PNaCl bitcode wire format.
  ///
  inline bool isNaClBitcode(const unsigned char *BufPtr,
                        const unsigned char *BufEnd) {
    return BufPtr+4 <= BufEnd &&
        BufPtr[0] == 'P' &&
        BufPtr[1] == 'E' &&
        BufPtr[2] == 'X' &&
        BufPtr[3] == 'E';
  }

} // end llvm namespace
#endif
