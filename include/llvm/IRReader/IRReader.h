//===---- llvm/IRReader/IRReader.h - Reader for LLVM IR files ---*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines functions for reading LLVM IR. They support both
// Bitcode and Assembly, automatically detecting the input format.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_IRREADER_IRREADER_H
#define LLVM_IRREADER_IRREADER_H

#include <string>

namespace llvm {

class Module;
class MemoryBuffer;
class SMDiagnostic;
class LLVMContext;

/// If the given MemoryBuffer holds a bitcode image, return a Module for it
/// which does lazy deserialization of function bodies.  Otherwise, attempt to
/// parse it as LLVM Assembly and return a fully populated Module. This
/// function *always* takes ownership of the given MemoryBuffer.
Module *getLazyIRModule(MemoryBuffer *Buffer, SMDiagnostic &Err,
                        LLVMContext &Context);

/// If the given file holds a bitcode image, return a Module
/// for it which does lazy deserialization of function bodies.  Otherwise,
/// attempt to parse it as LLVM Assembly and return a fully populated
/// Module.
Module *getLazyIRFileModule(const std::string &Filename, SMDiagnostic &Err,
                            LLVMContext &Context);

/// If the given MemoryBuffer holds a bitcode image, return a Module
/// for it.  Otherwise, attempt to parse it as LLVM Assembly and return
/// a Module for it. This function *always* takes ownership of the given
/// MemoryBuffer.
Module *ParseIR(MemoryBuffer *Buffer, SMDiagnostic &Err, LLVMContext &Context);

/// If the given file holds a bitcode image, return a Module for it.
/// Otherwise, attempt to parse it as LLVM Assembly and return a Module
/// for it.
Module *ParseIRFile(const std::string &Filename, SMDiagnostic &Err,
                    LLVMContext &Context);

// @LOCALMOD-BEGIN
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
// @LOCALMOD-END
}

#endif
