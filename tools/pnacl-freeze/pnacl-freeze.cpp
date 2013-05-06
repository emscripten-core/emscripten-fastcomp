/* Copyright 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

//===-- pnacl-freeze.cpp - The low-level NaCl bitcode freezer     --------===//
//
//===----------------------------------------------------------------------===//
//
// Generates NaCl pexe wire format.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/LLVMContext.h"
// Note: We need the following to provide the API for calling the NaCl
// Bitcode Writer to generate the frozen file.
#include "llvm/Bitcode/NaCl/NaClReaderWriter.h"
// Note: We need the following to provide the API for calling the (LLVM)
// Bitcode Reader to read in the corresonding pexe file to freeze.
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/DataStream.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/ToolOutputFile.h"

using namespace llvm;


static cl::opt<std::string>
OutputFilename("o", cl::desc("Specify output filename"),
	       cl::value_desc("filename"), cl::init("-"));

static cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<pexe file>"), cl::init("-"));

static void WriteOutputFile(const Module *M) {

  std::string ErrorInfo;
  OwningPtr<tool_output_file> Out
    (new tool_output_file(OutputFilename.c_str(), ErrorInfo,
			  raw_fd_ostream::F_Binary));
  if (!ErrorInfo.empty()) {
    errs() << ErrorInfo << '\n';
    exit(1);
  }

  NaClWriteBitcodeToFile(M, Out->os());

  // Declare success.
  Out->keep();
}

int main(int argc, char **argv) {
  // Print a stack trace if we signal out.
  sys::PrintStackTraceOnErrorSignal();
  PrettyStackTraceProgram X(argc, argv);

  LLVMContext &Context = getGlobalContext();
  llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.

  cl::ParseCommandLineOptions(argc, argv, "Generates NaCl pexe wire format\n");

  std::string ErrorMessage;
  std::auto_ptr<Module> M;

  // Use the bitcode streaming interface
  DataStreamer *streamer = getDataFileStreamer(InputFilename, &ErrorMessage);
  if (streamer) {
    std::string DisplayFilename;
    if (InputFilename == "-")
      DisplayFilename = "<stdin>";
    else
      DisplayFilename = InputFilename;
    M.reset(getStreamedBitcodeModule(DisplayFilename, streamer, Context,
                                     &ErrorMessage));
    if(M.get() != 0 && M->MaterializeAllPermanently(&ErrorMessage)) {
      M.reset();
    }
  }

  if (M.get() == 0) {
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
