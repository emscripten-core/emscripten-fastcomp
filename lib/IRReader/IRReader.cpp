//===- IRReader.cpp - NaCl aware IR readers. ------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/OwningPtr.h"
#include "llvm/Assembly/Parser.h"
#include "llvm/Bitcode/NaCl/NaClReaderWriter.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/system_error.h"

using namespace llvm;

// Note: Code below based on ParseIR and ParseIRFile in llvm/Support/IRReader.h

Module *llvm::NaClParseIR(MemoryBuffer *Buffer,
                          NaClFileFormat Format,
                          SMDiagnostic &Err,
                          LLVMContext &Context) {
  if ((Format == PNaClFormat) &&
      isNaClBitcode((const unsigned char *)Buffer->getBufferStart(),
                    (const unsigned char *)Buffer->getBufferEnd())) {
    std::string ErrMsg;
    Module *M = NaClParseBitcodeFile(Buffer, Context, &ErrMsg);
    if (M == 0)
      Err = SMDiagnostic(Buffer->getBufferIdentifier(), SourceMgr::DK_Error,
                         ErrMsg);
    // ParseBitcodeFile does not take ownership of the Buffer.
    delete Buffer;
    return M;
  } else if (Format == LLVMFormat) {
    if (isBitcode((const unsigned char *)Buffer->getBufferStart(),
                  (const unsigned char *)Buffer->getBufferEnd())) {
      std::string ErrMsg;
      Module *M = ParseBitcodeFile(Buffer, Context, &ErrMsg);
      if (M == 0)
        Err = SMDiagnostic(Buffer->getBufferIdentifier(), SourceMgr::DK_Error,
                           ErrMsg);
      // ParseBitcodeFile does not take ownership of the Buffer.
      delete Buffer;
      return M;
    }

    return ParseAssembly(Buffer, 0, Err, Context);
  } else {
    Err = SMDiagnostic(Buffer->getBufferIdentifier(), SourceMgr::DK_Error,
                       "Did not specify correct format for file");
    return 0;
  }
}

Module *llvm::NaClParseIRFile(const std::string &Filename,
                              NaClFileFormat Format,
                              SMDiagnostic &Err,
                              LLVMContext &Context) {
  OwningPtr<MemoryBuffer> File;
  if (error_code ec = MemoryBuffer::getFileOrSTDIN(Filename.c_str(), File)) {
    Err = SMDiagnostic(Filename, SourceMgr::DK_Error,
                       "Could not open input file: " + ec.message());
    return 0;
  }

  return NaClParseIR(File.take(), Format, Err, Context);
}
