//===-- pnacl-bcdis.cpp - Disassemble pnacl bitcode -----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

/// TODO(kschimpf): Add disassembling abbreviations.

#include "llvm/Bitcode/NaCl/NaClReaderWriter.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Support/raw_ostream.h"
#include <system_error>

namespace {

using namespace llvm;

// The input file to read.
static cl::opt<std::string>
  InputFilename(cl::Positional, cl::desc("<input bitcode>"), cl::init("-"));

// The output file to generate.
static cl::opt<std::string>
OutputFilename("o", cl::desc("Specify output filename"),
               cl::value_desc("filename"), cl::init("-"));

static cl::opt<bool>
NoRecords("no-records",
          cl::desc("Don't include records"),
          cl::init(false));

static cl::opt<bool>
NoAssembly("no-assembly",
           cl::desc("Don't include assembly"),
           cl::init(false));

// Reads and disassembles the bitcode file. Returns false
// if successful, true otherwise.
static bool DisassembleBitcode() {
  // Open the bitcode file and put into a buffer.
  ErrorOr<std::unique_ptr<MemoryBuffer>> ErrOrFile =
      MemoryBuffer::getFileOrSTDIN(InputFilename);
  if (std::error_code EC = ErrOrFile.getError()) {
    errs() << "Error reading '" << InputFilename << "': " << EC.message()
           << "\n";
    return true;
  }

  // Create a stream to output the bitcode text to.
  std::error_code EC;
  raw_fd_ostream Output(OutputFilename, EC, sys::fs::F_None);
  if (EC) {
    errs() << EC.message() << '\n';
    return true;
  }

  // Parse the the bitcode file.
  return NaClObjDump(ErrOrFile.get().release(), Output, NoRecords, NoAssembly);
}

}

int main(int argc, char **argv) {
  // Print a stack trace if we signal out.
  sys::PrintStackTraceOnErrorSignal();
  PrettyStackTraceProgram X(argc, argv);
  llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.
  cl::ParseCommandLineOptions(argc, argv, "pnacl-bccompress file analyzer\n");

  if (DisassembleBitcode()) return 1;
  return 0;
}
