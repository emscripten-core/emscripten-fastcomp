//===-- srpc_main.cpp - PNaCl sandboxed translator invocation -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Main invocation of the sandboxed translator through SRPC.
//
//===----------------------------------------------------------------------===//

#if defined(__native_client__)

// Headers which are not properly part of the SDK are included by their
// path in the NaCl tree.
#include "native_client/src/shared/srpc/nacl_srpc.h"
#ifdef __pnacl__
#include "native_client/src/untrusted/nacl/pnacl.h"
#endif // __pnacl__

#include "SRPCStreamer.h"

#include <argz.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include "llvm/ADT/OwningPtr.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Option/Option.h"
#include "llvm/Support/system_error.h"

using namespace llvm;
using namespace llvm::opt;
using std::string;

// Imported from pnacl-llc.cpp
extern int llc_main(int argc, char **argv);

namespace {

// The filename used internally for looking up the bitcode file.
const char kBitcodeFilename[] = "pnacl.pexe";
// The filename used internally for looking up the object code file.
const char kObjectFilename[] = "pnacl.o";
// Object which manages streaming bitcode over SRPC and threading.
SRPCStreamer *srpc_streamer;
// FD of the object file.
int object_file_fd;

DataStreamer *NaClBitcodeStreamer;

int DoTranslate(ArgStringList *CmdLineArgs, int object_fd) {
  if (CmdLineArgs == NULL) {
    return 1;
  }
  object_file_fd = object_fd;
  // Make an argv array from the input vector.
  size_t argc = CmdLineArgs->size();
  char **argv = new char *[argc];
  for (size_t i = 0; i < argc; ++i) {
    // llc_main will not mutate the command line, so this is safe.
    argv[i] = const_cast<char *>((*CmdLineArgs)[i]);
  }
  argv[argc] = NULL;
  // Call main.
  return llc_main(static_cast<int>(argc), argv);
}

ArgStringList *CommandLineFromArgz(char *str, size_t str_len) {
  char *entry = str;
  ArgStringList *CmdLineArgs = new ArgStringList;
  while (entry != NULL) {
    CmdLineArgs->push_back(entry);
    entry = argz_next(str, str_len, entry);
  }
  return CmdLineArgs;
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
    CmdLineArgs->push_back("-mcpu=pentium4");
    break;
  }
  case PnaclTargetArchitectureX86_64: {
    CmdLineArgs->push_back("-mcpu=core2");
    break;
  }
  case PnaclTargetArchitectureARM_32: {
    CmdLineArgs->push_back("-mcpu=cortex-a9");
    break;
  }
  default:
    fprintf(stderr, "no target architecture match.\n");
    return false;
  }
// Some cases for building this with nacl-gcc.
#elif defined(__i386__)
  CmdLineArgs->push_back("-mcpu=pentium4");
#elif defined(__x86_64__)
  CmdLineArgs->push_back("-mcpu=core2");
#elif defined(__arm__)
  CmdLineArgs->push_back("-mcpu=cortex-a9");
#error "Unknown architecture"
#endif
  return true;
}

bool HasCPUOverride(ArgStringList *CmdLineArgs) {
  const char *Mcpu = "-mcpu";
  size_t McpuLen = strlen(Mcpu);
  for (size_t i = 0; i < CmdLineArgs->size(); ++i) {
    if (strncmp((*CmdLineArgs)[i], Mcpu, McpuLen) == 0) {
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
  static const char *llc_args_x8632[] = { "-mtriple=i686-none-nacl-gnu", NULL };
  static const char *llc_args_x8664[] = { "-mtriple=x86_64-none-nacl-gnu",
                                          NULL };
  static const char *llc_args_arm[] = {
    "-mtriple=armv7a-none-nacl-gnueabi", "-arm-reserve-r9", "-sfi-disable-cp",
    "-sfi-store", "-sfi-load", "-sfi-stack", "-sfi-branch", "-sfi-data",
    "-mattr=+neon", "-no-inline-jumptables", "-float-abi=hard", NULL
  };

  const char **llc_args = NULL;
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
  default:
    fprintf(stderr, "no target architecture match.\n");
    delete command_line;
    return NULL;
  }
// Some cases for building this with nacl-gcc.
#elif defined(__i386__)
  llc_args = llc_args_x8632;
#elif defined(__x86_64__)
  llc_args = llc_args_x8664;
#elif defined(__arm__)
  llc_args = llc_args_arm;
#else
#error "Unknown architecture"
#endif
  for (size_t i = 0; llc_args[i] != NULL; i++)
    command_line->push_back(llc_args[i]);
  return command_line;
}

// Data passed from main thread to compile thread.
// Takes ownership of the commandline vector.
class StreamingThreadData {
public:
  StreamingThreadData(int object_fd, ArgStringList *cmd_line_vec)
      : object_fd_(object_fd), cmd_line_vec_(cmd_line_vec) {}
  int ObjectFD() const { return object_fd_; }
  ArgStringList *CmdLineVec() const { return cmd_line_vec_.get(); }
  const int object_fd_;
  const OwningPtr<ArgStringList> cmd_line_vec_;
};

void *run_streamed(void *arg) {
  StreamingThreadData *data = reinterpret_cast<StreamingThreadData *>(arg);
  data->CmdLineVec()->push_back("-streaming-bitcode");
  if (DoTranslate(data->CmdLineVec(), data->ObjectFD()) != 0) {
    fprintf(stderr, "DoTranslate failed.\n");
    srpc_streamer->setError();
    return NULL;
  }
  delete data;
  return NULL;
}

// Actually do the work for stream initialization.
void do_stream_init(NaClSrpcRpc *rpc, NaClSrpcArg **in_args,
                    NaClSrpcArg **out_args, NaClSrpcClosure *done,
                    ArgStringList *command_line_vec) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  srpc_streamer = new SRPCStreamer();
  std::string StrError;
  StreamingThreadData *thread_data =
      new StreamingThreadData(in_args[0]->u.hval, command_line_vec);
  NaClBitcodeStreamer = srpc_streamer->init(
      run_streamed, reinterpret_cast<void *>(thread_data), &StrError);
  if (NaClBitcodeStreamer) {
    rpc->result = NACL_SRPC_RESULT_OK;
    out_args[0]->arrays.str = strdup("no error");
  } else {
    out_args[0]->arrays.str = strdup(StrError.c_str());
  }
}

// Invoked by the StreamInit RPC to initialize bitcode streaming over SRPC.
// Under the hood it forks a new thread at starts the llc_main, which sets
// up the compilation and blocks when it tries to start reading the bitcode.
// Input arg is a file descriptor to write the output object file to.
// Returns a string, containing an error message if the call fails.
void stream_init(NaClSrpcRpc *rpc, NaClSrpcArg **in_args,
                 NaClSrpcArg **out_args, NaClSrpcClosure *done) {
  // cmd_line_vec allocated by GetDefaultCommandLine() is freed by the
  // translation thread in run_streamed()
  ArgStringList *cmd_line_vec = GetDefaultCommandLine();
  if (!cmd_line_vec || !AddDefaultCPU(cmd_line_vec)) {
    NaClSrpcClosureRunner runner(done);
    rpc->result = NACL_SRPC_RESULT_APP_ERROR;
    out_args[0]->arrays.str = strdup("Failed to get default commandline.");
    return;
  }
  AddFixedArguments(cmd_line_vec);
  do_stream_init(rpc, in_args, out_args, done, cmd_line_vec);
}

// Invoked by StreamInitWithCommandLine RPC. Same as stream_init, but
// provides a command line to use instead of the default.
void stream_init_with_command_line(NaClSrpcRpc *rpc, NaClSrpcArg **in_args,
                                   NaClSrpcArg **out_args,
                                   NaClSrpcClosure *done) {
  char *command_line = in_args[1]->arrays.carr;
  size_t command_line_len = in_args[1]->u.count;
  ArgStringList *cmd_line_vec =
      CommandLineFromArgz(command_line, command_line_len);
  AddFixedArguments(cmd_line_vec);
  // cmd_line_vec is freed by the translation thread in run_streamed
  do_stream_init(rpc, in_args, out_args, done, cmd_line_vec);
}

// Invoked by StreamInitWithOverrides RPC. Same as stream_init, but
// provides commandline flag overrides (appended to the default).
void stream_init_with_overrides(NaClSrpcRpc *rpc, NaClSrpcArg **in_args,
                                NaClSrpcArg **out_args, NaClSrpcClosure *done) {
  ArgStringList *cmd_line_vec = GetDefaultCommandLine();
  if (!cmd_line_vec) {
    NaClSrpcClosureRunner runner(done);
    rpc->result = NACL_SRPC_RESULT_APP_ERROR;
    out_args[0]->arrays.str = strdup("Failed to get default commandline.");
    return;
  }
  AddFixedArguments(cmd_line_vec);

  char *command_line = in_args[1]->arrays.carr;
  size_t command_line_len = in_args[1]->u.count;
  OwningPtr<ArgStringList> extra_vec(
      CommandLineFromArgz(command_line, command_line_len));
  cmd_line_vec->insert(cmd_line_vec->end(), extra_vec->begin(),
                       extra_vec->end());
  // Make sure some -mcpu override exists for now to prevent
  // auto-cpu feature detection from triggering instructions that
  // we do not validate yet.
  if (!HasCPUOverride(extra_vec.get())) {
    AddDefaultCPU(cmd_line_vec);
  }
  extra_vec.reset(NULL);
  // cmd_line_vec is freed by the translation thread in run_streamed.
  do_stream_init(rpc, in_args, out_args, done, cmd_line_vec);
}

// Invoked by the StreamChunk RPC. Receives a chunk of the bitcode and
// buffers it for later retrieval by the compilation thread.
void stream_chunk(NaClSrpcRpc *rpc, NaClSrpcArg **in_args,
                  NaClSrpcArg **out_args, NaClSrpcClosure *done) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  size_t len = in_args[0]->u.count;
  unsigned char *bytes =
      reinterpret_cast<unsigned char *>(in_args[0]->arrays.carr);
  if (srpc_streamer->gotChunk(bytes, len) != len) {
    return;
  }
  rpc->result = NACL_SRPC_RESULT_OK;
}

// Invoked by the StreamEnd RPC. Waits until the compilation finishes,
// then returns. Returns an int indicating whether the bitcode is a
// shared library, a string with the soname, a string with dependencies,
// and a string which contains an error message if applicable.
void stream_end(NaClSrpcRpc *rpc, NaClSrpcArg **in_args, NaClSrpcArg **out_args,
                NaClSrpcClosure *done) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  std::string StrError;
  if (srpc_streamer->streamEnd(&StrError)) {
    out_args[3]->arrays.str = strdup(StrError.c_str());
    return;
  }
  // TODO(eliben): We don't really use shared libraries now. At some
  // point this should be cleaned up from SRPC as well.
  out_args[0]->u.ival = false;
  // SRPC deletes the strings returned when the closure is invoked.
  // Therefore we need to use strdup.
  out_args[1]->arrays.str = strdup("");
  out_args[2]->arrays.str = strdup("");
  rpc->result = NACL_SRPC_RESULT_OK;
}

const struct NaClSrpcHandlerDesc srpc_methods[] = {
  // Protocol for streaming:
  // (StreamInit(obj_fd) -> error_str |
  //    StreamInitWIthCommandLine(obj_fd, escaped_cmdline) -> error_str)
  // StreamChunk(data) +
  // StreamEnd() -> (is_shared_lib,soname,dependencies,error_str)
  { "StreamInit:h:s", stream_init },
  { "StreamInitWithCommandLine:hC:s:", stream_init_with_command_line },
  { "StreamInitWithOverrides:hC:s:", stream_init_with_overrides },
  { "StreamChunk:C:", stream_chunk }, { "StreamEnd::isss", stream_end },
  { NULL, NULL },
};

} // namespace

int getObjectFileFD() { return object_file_fd; }

DataStreamer *getNaClBitcodeStreamer() { return NaClBitcodeStreamer; }

int srpc_main(int argc, char **argv) {
  if (!NaClSrpcModuleInit()) {
    return 1;
  }

  if (!NaClSrpcAcceptClientConnection(srpc_methods)) {
    return 1;
  }
  NaClSrpcModuleFini();
  return 0;
}

#endif // __native_client__
