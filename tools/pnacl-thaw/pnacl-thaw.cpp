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
#include "llvm/Support/StreamableMemoryObject.h"
#include "llvm/Support/ToolOutputFile.h"

using namespace llvm;

static cl::opt<std::string>
OutputFilename("o", cl::desc("Specify thawed pexe filename"),
	       cl::value_desc("filename"), cl::init("-"));

static cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<frozen file>"), cl::init("-"));

static cl::opt<bool>
VerboseErrors(
    "verbose-parse-errors",
    cl::desc("Print out more descriptive PNaCl bitcode parse errors"),
    cl::init(false));

static void WriteOutputFile(const Module *M) {

  std::string ErrorInfo;
  std::unique_ptr<tool_output_file> Out(
      new tool_output_file(OutputFilename.c_str(), ErrorInfo, sys::fs::F_None));
  if (!ErrorInfo.empty()) {
    errs() << ErrorInfo << '\n';
    exit(1);
  }

  WriteBitcodeToFile(M, Out->os());

  // Declare success.
  Out->keep();
}

int main(int argc, char **argv) {
  // Print a stack trace if we signal out.
  sys::PrintStackTraceOnErrorSignal();
  PrettyStackTraceProgram X(argc, argv);

  LLVMContext &Context = getGlobalContext();
  llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.

  cl::ParseCommandLineOptions(
      argc, argv, "Converts NaCl pexe wire format into LLVM bitcode format\n");

  std::string ErrorMessage;
  std::auto_ptr<Module> M;

  // Use the bitcode streaming interface
  DataStreamer *streamer = getDataFileStreamer(InputFilename, &ErrorMessage);
  std::unique_ptr<StreamingMemoryObject> Buffer(
      new StreamingMemoryObjectImpl(streamer));
  if (streamer) {
    std::string DisplayFilename;
    if (InputFilename == "-")
      DisplayFilename = "<stdin>";
    else
      DisplayFilename = InputFilename;
    raw_ostream *Verbose = VerboseErrors ? &errs() : nullptr;
    M.reset(getNaClStreamedBitcodeModule(DisplayFilename, Buffer.release(),
                                         Context, Verbose,
                                         &ErrorMessage,
                                         /*AcceptSupportedOnly=*/false));
    if (M.get())
      if (std::error_code EC = M->materializeAllPermanently()) {
        ErrorMessage = EC.message();
        M.reset();
      }
  }

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
