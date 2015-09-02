//===-- srpc_main.cpp - PNaCl sandboxed translator invocation -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Main entry point and callback handler code for the in-browser sandboxed
// translator.  The interface between this code and the browser is through
// the NaCl IRT.
//
//===----------------------------------------------------------------------===//

#if defined(PNACL_BROWSER_TRANSLATOR)

// Headers which are not properly part of the SDK are included by their
// path in the NaCl tree.
#ifdef __pnacl__
#include "native_client/src/untrusted/nacl/pnacl.h"
#endif // __pnacl__

#include "SRPCStreamer.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/Option/Option.h"
#include "llvm/Support/ErrorHandling.h"

#include <argz.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <irt.h>
#include <irt_dev.h>
#include <string>
#include <vector>

using namespace llvm;
using namespace llvm::opt;

// Imported from pnacl-llc.cpp
extern int llc_main(int ArgC, char **ArgV);

namespace {

// The filename used internally for looking up the bitcode file.
const char kBitcodeFilename[] = "pnacl.pexe";
// The filename used internally for looking up the object code file.
const char kObjectFilename[] = "pnacl.o";
// Object which manages streaming bitcode over IPC and threading.
// TODO(jvoung): rename this and other "SRPC" things to not refer to SRPC.
SRPCStreamer *gIPCStreamer;
// FDs of the object file(s).
std::vector<int> gObjectFiles;

DataStreamer *gNaClBitcodeStreamer;

void getIRTInterfaces(nacl_irt_private_pnacl_translator_compile &IRTFuncs) {
  size_t QueryResult =
      nacl_interface_query(NACL_IRT_PRIVATE_PNACL_TRANSLATOR_COMPILE_v0_1,
                           &IRTFuncs, sizeof(IRTFuncs));
  if (QueryResult != sizeof(IRTFuncs))
    llvm::report_fatal_error("Failed to get translator compile IRT interface");
}

int DoTranslate(ArgStringList *CmdLineArgs) {
  if (!CmdLineArgs)
    return 1;
  // Make an ArgV array from the input vector.
  size_t ArgC = CmdLineArgs->size();
  char **ArgV = new char *[ArgC + 1];
  for (size_t i = 0; i < ArgC; ++i) {
    // llc_main will not mutate the command line, so this is safe.
    ArgV[i] = const_cast<char *>((*CmdLineArgs)[i]);
  }
  ArgV[ArgC] = nullptr;
  // Call main.
  return llc_main(static_cast<int>(ArgC), ArgV);
}

void AddFixedArguments(ArgStringList *CmdLineArgs) {
  // Add fixed arguments to the command line.  These specify the bitcode
  // and object code filenames, removing them from the contract with the
  // coordinator.
  CmdLineArgs->push_back(kBitcodeFilename);
  CmdLineArgs->push_back("-o");
  CmdLineArgs->push_back(kObjectFilename);
}

bool AddDefaultCPU(ArgStringList *CmdLineArgs) {
#if defined(__pnacl__)
  switch (__builtin_nacl_target_arch()) {
  case PnaclTargetArchitectureX86_32: {
    CmdLineArgs->push_back("-mcpu=pentium4m");
    break;
  }
  case PnaclTargetArchitectureX86_64: {
    CmdLineArgs->push_back("-mcpu=x86-64");
    break;
  }
  case PnaclTargetArchitectureARM_32: {
    CmdLineArgs->push_back("-mcpu=cortex-a9");
    break;
  }
  case PnaclTargetArchitectureMips_32: {
    CmdLineArgs->push_back("-mcpu=mips32r2");
    break;
  }
  default:
    fprintf(stderr, "no target architecture match.\n");
    return false;
  }
// Some cases for building this with nacl-gcc or nacl-clang.
#elif defined(__i386__)
  CmdLineArgs->push_back("-mcpu=pentium4m");
#elif defined(__x86_64__)
  CmdLineArgs->push_back("-mcpu=x86-64");
#elif defined(__arm__)
  CmdLineArgs->push_back("-mcpu=cortex-a9");
#else
#error "Unknown architecture"
#endif
  return true;
}

bool HasCPUOverride(ArgStringList *CmdLineArgs) {
  const char *Mcpu = "-mcpu";
  size_t McpuLen = strlen(Mcpu);
  for (const char *Arg : *CmdLineArgs) {
    if (strncmp(Arg, Mcpu, McpuLen) == 0) {
      return true;
    }
  }
  return false;
}

ArgStringList *GetDefaultCommandLine() {
  ArgStringList *command_line = new ArgStringList;
  // First, those common to all architectures.
  static const char *common_args[] = { "pnacl_translator", "-filetype=obj" };
  for (size_t i = 0; i < array_lengthof(common_args); ++i) {
    command_line->push_back(common_args[i]);
  }
  // Then those particular to a platform.
  static const char *llc_args_x8632[] = {"-mtriple=i686-none-nacl-gnu",
                                         nullptr};
  static const char *llc_args_x8664[] = {"-mtriple=x86_64-none-nacl-gnu",
                                         nullptr};
  static const char *llc_args_arm[] = {"-mtriple=armv7a-none-nacl-gnueabi",
                                       "-mattr=+neon", "-float-abi=hard",
                                       nullptr};
  static const char *llc_args_mips32[] = {"-mtriple=mipsel-none-nacl-gnu",
                                          nullptr};

  const char **llc_args = nullptr;
#if defined(__pnacl__)
  switch (__builtin_nacl_target_arch()) {
  case PnaclTargetArchitectureX86_32: {
    llc_args = llc_args_x8632;
    break;
  }
  case PnaclTargetArchitectureX86_64: {
    llc_args = llc_args_x8664;
    break;
  }
  case PnaclTargetArchitectureARM_32: {
    llc_args = llc_args_arm;
    break;
  }
  case PnaclTargetArchitectureMips_32: {
    llc_args = llc_args_mips32;
    break;
  }
  default:
    fprintf(stderr, "no target architecture match.\n");
    delete command_line;
    return nullptr;
  }
// Some cases for building this with nacl-gcc.
#elif defined(__i386__)
  (void)llc_args_x8664;
  (void)llc_args_arm;
  llc_args = llc_args_x8632;
#elif defined(__x86_64__)
  (void)llc_args_x8632;
  (void)llc_args_arm;
  llc_args = llc_args_x8664;
#elif defined(__arm__)
  (void)llc_args_x8632;
  (void)llc_args_x8664;
  llc_args = llc_args_arm;
#else
#error "Unknown architecture"
#endif
  for (size_t i = 0; llc_args[i] != nullptr; i++)
    command_line->push_back(llc_args[i]);
  return command_line;
}

// Data passed from main thread to compile thread.
// Takes ownership of the commandline vector.
class StreamingThreadData {
public:
  StreamingThreadData(int module_count, ArgStringList *cmd_line_vec)
      : module_count_(module_count), cmd_line_vec_(cmd_line_vec) {}
  ArgStringList *CmdLineVec() const { return cmd_line_vec_.get(); }
  int module_count_;
  const std::unique_ptr<ArgStringList> cmd_line_vec_;
};

void *run_streamed(void *arg) {
  StreamingThreadData *data = reinterpret_cast<StreamingThreadData *>(arg);
  data->CmdLineVec()->push_back("-streaming-bitcode");
  if (DoTranslate(data->CmdLineVec()) != 0) {
    // llc_main only returns 1 (as opposed to calling report_fatal_error)
    // in conditions we never expect to see in the browser (e.g. bad
    // command-line flags).
    gIPCStreamer->setFatalError("llc_main unspecified failure");
    return nullptr;
  }
  delete data;
  return nullptr;
}

char *onInitCallback(uint32_t NumThreads, int *ObjFileFDs,
                     size_t ObjFileFDCount, char **ArgV, size_t ArgC) {
  ArgStringList *cmd_line_vec = GetDefaultCommandLine();
  if (!cmd_line_vec) {
    return "Failed to get default commandline.";
  }
  AddFixedArguments(cmd_line_vec);

  // The IRT should check if this is beyond the max.
  if (NumThreads < 1) {
    return "Invalid module split count.";
  }

  gObjectFiles.clear();
  for (uint32_t i = 0; i < NumThreads; ++i) {
    gObjectFiles.push_back(ObjFileFDs[i]);
  }

  // Make a copy of the extra commandline arguments.
  for (size_t i = 0; i < ArgC; ++i) {
    cmd_line_vec->push_back(strdup(ArgV[i]));
  }
  // Make sure some -mcpu override exists for now to prevent
  // auto-cpu feature detection from triggering instructions that
  // we do not validate yet.
  if (!HasCPUOverride(cmd_line_vec)) {
    AddDefaultCPU(cmd_line_vec);
  }

  gIPCStreamer = new SRPCStreamer();
  std::string StrError;
  // cmd_line_vec is freed by the translation thread in run_streamed.
  StreamingThreadData *thread_data =
      new StreamingThreadData(NumThreads, cmd_line_vec);
  gNaClBitcodeStreamer = gIPCStreamer->init(
      run_streamed, reinterpret_cast<void *>(thread_data), &StrError);
  if (gNaClBitcodeStreamer) {
    return nullptr;
  } else {
    return strdup(StrError.c_str());
  }
}

int onDataCallback(const void *Data, size_t NumBytes) {
  unsigned char *CharData =
      reinterpret_cast<unsigned char *>(const_cast<void *>(Data));
  return gIPCStreamer->gotChunk(CharData, NumBytes) != NumBytes;
}

char *onEndCallback() {
  std::string StrError;
  if (gIPCStreamer->streamEnd(&StrError)) {
    return strdup(StrError.c_str());
  }
  return nullptr;
}

const struct nacl_irt_pnacl_compile_funcs gLLCCallbacks = {
  &onInitCallback, &onDataCallback, &onEndCallback
};

} // namespace

int getObjectFileFD(unsigned Index) {
  assert(Index < gObjectFiles.size());
  return gObjectFiles[Index];
}

DataStreamer *getNaClBitcodeStreamer() { return gNaClBitcodeStreamer; }

// Called from the compilation thread
void FatalErrorHandler(void *user_data, const std::string& reason,
                       bool gen_crash_diag) {
  gIPCStreamer->setFatalError(reason);
}

fatal_error_handler_t getSRPCErrorHandler() { return FatalErrorHandler; }

int srpc_main(int ArgC, char **ArgV) {
  struct nacl_irt_private_pnacl_translator_compile IRTFuncs;
  getIRTInterfaces(IRTFuncs);
  IRTFuncs.serve_translate_request(&gLLCCallbacks);
  return 0;
}

#endif // PNACL_BROWSER_TRANSLATOR
