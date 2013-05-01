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
				   std::string *ErrMsg = 0);

  /// getNaClStreamedBitcodeModule - Read the header of the specified stream
  /// and prepare for lazy deserialization and streaming of function bodies.
  /// On error, this returns null, and fills in *ErrMsg with an error
  /// description if ErrMsg is non-null.
  Module *getNaClStreamedBitcodeModule(const std::string &name,
				       DataStreamer *streamer,
				       LLVMContext &Context,
				       std::string *ErrMsg = 0);

  /// NaClParseBitcodeFile - Read the specified bitcode file,
  /// returning the module.  If an error occurs, this returns null and
  /// fills in *ErrMsg if it is non-null.  This method *never* takes
  /// ownership of Buffer.
  Module *NaClParseBitcodeFile(MemoryBuffer *Buffer, LLVMContext &Context,
			       std::string *ErrMsg = 0);

  /// NaClWriteBitcodeToFile - Write the specified module to the
  /// specified raw output stream, using PNaCl wire format.  For
  /// streams where it matters, the given stream should be in "binary"
  /// mode.
  void NaClWriteBitcodeToFile(const Module *M, raw_ostream &Out);

  /// isNaClBitcodeWrapper - Return true if the given bytes are the
  /// magic bytes for an LLVM IR bitcode wrapper.
  ///
  inline bool isNaClBitcodeWrapper(const unsigned char *BufPtr,
                                   const unsigned char *BufEnd) {
    // See if you can find the hidden message in the magic bytes :-).
    // (Hint: it's a little-endian encoding.)
    return BufPtr != BufEnd &&
           BufPtr[0] == 0xDE &&
           BufPtr[1] == 0xC0 &&
           BufPtr[2] == 0x17 &&
           BufPtr[3] == 0x0B;
  }

  /// isNaClRawBitcode - Return true if the given bytes are the magic
  /// bytes for raw LLVM IR bitcode (without a wrapper).
  ///
  inline bool isNaClRawBitcode(const unsigned char *BufPtr,
                               const unsigned char *BufEnd) {
    // These bytes sort of have a hidden message, but it's not in
    // little-endian this time, and it's a little redundant.
    return BufPtr != BufEnd &&
           BufPtr[0] == 'B' &&
           BufPtr[1] == 'C' &&
           BufPtr[2] == 0xc0 &&
           BufPtr[3] == 0xde;
  }

  /// isNaClBitcode - Return true if the given bytes are the magic bytes for
  /// LLVM IR bitcode, either with or without a wrapper.
  ///
  inline bool isNaClBitcode(const unsigned char *BufPtr,
                        const unsigned char *BufEnd) {
    return isNaClBitcodeWrapper(BufPtr, BufEnd) ||
           isNaClRawBitcode(BufPtr, BufEnd);
  }

  /// SkipNaClBitcodeWrapperHeader - Some systems wrap bc files with a
  /// special header for padding or other reasons.  The format of this
  /// header is:
  ///
  /// struct bc_header {
  ///   uint32_t Magic;         // 0x0B17C0DE
  ///   uint32_t Version;       // Version, currently always 0.
  ///   uint32_t BitcodeOffset; // Offset to traditional bitcode file.
  ///   uint32_t BitcodeSize;   // Size of traditional bitcode file.
  ///   ... potentially other gunk ...
  /// };
  ///
  /// TODO(kschimpf): Consider changing Magic and/or gunk to communicate
  ///     file is PNaCl wire format file (rather than LLVM bitcode).
  ///
  /// TODO(kschimpf): Add code to read gunk in, and store it so it is
  /// accessable.
  ///
  /// This function is called when we find a file with a matching magic number.
  /// In this case, skip down to the subsection of the file that is actually a
  /// BC file.
  /// If 'VerifyBufferSize' is true, check that the buffer is large enough to
  /// contain the whole bitcode file.
  inline bool SkipNaClBitcodeWrapperHeader(const unsigned char *&BufPtr,
                                           const unsigned char *&BufEnd,
                                           bool VerifyBufferSize) {
    enum {
      KnownHeaderSize = 4*4,  // Size of header we read.
      OffsetField = 2*4,      // Offset in bytes to Offset field.
      SizeField = 3*4         // Offset in bytes to Size field.
    };

    // Must contain the header!
    if (BufEnd-BufPtr < KnownHeaderSize) return true;

    unsigned Offset = ( BufPtr[OffsetField  ]        |
                       (BufPtr[OffsetField+1] << 8)  |
                       (BufPtr[OffsetField+2] << 16) |
                       (BufPtr[OffsetField+3] << 24));
    unsigned Size   = ( BufPtr[SizeField    ]        |
                       (BufPtr[SizeField  +1] << 8)  |
                       (BufPtr[SizeField  +2] << 16) |
                       (BufPtr[SizeField  +3] << 24));

    // Verify that Offset+Size fits in the file.
    if (VerifyBufferSize && Offset+Size > unsigned(BufEnd-BufPtr))
      return true;
    BufPtr += Offset;
    BufEnd = BufPtr+Size;
    return false;
  }

} // end llvm namespace
#endif
