//===-- llvm/IRReader/IRReader.h - Reader of IR  ----------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This header defines IR readers that understand LLVM and PNaCl file formats.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_IRREADER_IRREADER_H
#define LLVM_IRREADER_IRREADER_H

namespace llvm {

class LLVMContext;
class MemoryBuffer;
class Module;
class SMDiagnostic;

// \brief Define the expected format of the file.
enum NaClFileFormat {
  // LLVM IR source or bitcode file (as appropriate).
  LLVMFormat,
  // PNaCl bitcode file.
  PNaClFormat
};

// \brief If the given MemoryBuffer holds a bitcode image, return a Module
// for it.  Otherwise, attempt to parse it as LLVM Assembly and return
// a Module for it. This function *always* takes ownership of the given
// MemoryBuffer.
Module *NaClParseIR(MemoryBuffer *Buffer,
                    NaClFileFormat Format,
                    SMDiagnostic &Err,
                    LLVMContext &Context);

/// \brief If the given file holds a Bitcode image, read the file.
/// Otherwise, attempt to parse it as LLVM assembly and return a
/// Module for it.
Module *NaClParseIRFile(const std::string &Filename,
                        NaClFileFormat Format,
                        SMDiagnostic &Err,
                        LLVMContext &Context);

} // end llvm namespace
#endif
