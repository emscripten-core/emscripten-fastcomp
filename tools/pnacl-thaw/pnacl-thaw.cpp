/* Copyright 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

//===-- pnacl-thaw.cpp - The low-level NaCl bitcode thawer ----------------===//
//
//===----------------------------------------------------------------------===//
//
// Converts NaCl wire format back to LLVM bitcode.
//
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/NaCl/NaClReaderWriter.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/DataStream.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/StreamingMemoryObject.h"
#include "llvm/Support/ToolOutputFile.h"

using namespace llvm;

namespace {

static cl::opt<std::string>
OutputFilename("o", cl::desc("Specify thawed pexe filename"),
	       cl::value_desc("filename"), cl::init("-"));

static cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<frozen file>"), cl::init("-"));

static cl::opt<bool>
AcceptBitcodeRecordsAsText(
    "bitcode-as-text",
    cl::desc(
        "Accept textual form of PNaCl bitcode records (i.e. not .ll assembly)"),
    cl::init(false));

static cl::opt<bool>
VerboseErrors(
    "verbose-parse-errors",
    cl::desc("Print out more descriptive PNaCl bitcode parse errors"),
    cl::init(false));

static void WriteOutputFile(const Module *M) {

  std::error_code EC;
  std::unique_ptr<tool_output_file> Out(
      new tool_output_file(OutputFilename, EC, sys::fs::F_None));
  if (EC) {
    errs() << EC.message() << '\n';
    exit(1);
  }

  WriteBitcodeToFile(M, Out->os());

  // Declare success.
  Out->keep();
}

static Module *readAsBitcodeText(std::string &Filename,
                                 LLVMContext &Context,
                                 raw_ostream *Verbose,
                                 std::string &ErrorMessage) {
  ErrorOr<Module *> M = parseNaClBitcodeText(Filename, Context, Verbose);
  if (M)
    return M.get();
  ErrorMessage = M.getError().message();
  return nullptr;
}

static Module *readAsBinaryBitcode(
    std::string &Filename, LLVMContext &Context, raw_ostream *Verbose,
    std::string &ErrorMessage) {
  // Use the bitcode streaming interface
  DataStreamer *Streamer = getDataFileStreamer(InputFilename, &ErrorMessage);
  if (Streamer == nullptr)
    return nullptr;
  std::unique_ptr<StreamingMemoryObject> Buffer(
      new StreamingMemoryObjectImpl(Streamer));
  std::string DisplayFilename;
  if (Filename == "-")
    DisplayFilename = "<stdin>";
  else
    DisplayFilename = Filename;
  Module *M = getNaClStreamedBitcodeModule(DisplayFilename, Buffer.release(),
                                           Context, Verbose,
                                           &ErrorMessage,
                                           /*AcceptSupportedOnly=*/false);
  if (!M)
    return nullptr;
  if (std::error_code EC = M->materializeAllPermanently()) {
    ErrorMessage = EC.message();
    delete M;
    return nullptr;
  }
  return M;
}

} // end of anonymous namespace

int main(int argc, char **argv) {
  // Print a stack trace if we signal out.
  sys::PrintStackTraceOnErrorSignal();
  PrettyStackTraceProgram X(argc, argv);

  LLVMContext &Context = getGlobalContext();
  llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.

  cl::ParseCommandLineOptions(
      argc, argv, "Converts NaCl pexe wire format into LLVM bitcode format\n");

  std::string ErrorMessage;
  std::unique_ptr<Module> M;
  raw_ostream *Verbose = VerboseErrors ? &errs() : nullptr;

  if (AcceptBitcodeRecordsAsText)
    M.reset(readAsBitcodeText(InputFilename, Context, Verbose, ErrorMessage));
  else
    M.reset(readAsBinaryBitcode(InputFilename, Context, Verbose, ErrorMessage));

  if (!M.get()) {
    errs() << argv[0] << ": ";
    if (ErrorMessage.size())
      errs() << ErrorMessage << "\n";
    else
      errs() << "bitcode didn't read correctly.\n";
    return 1;
  }

  WriteOutputFile(M.get());
  return 0;
}
