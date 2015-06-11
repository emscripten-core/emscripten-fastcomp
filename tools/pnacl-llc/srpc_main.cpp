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

#if defined(PNACL_BROWSER_TRANSLATOR)

// Headers which are not properly part of the SDK are included by their
// path in the NaCl tree.
#include "native_client/src/shared/srpc/nacl_srpc.h"
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
#include <string>

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
// Maximum number of modules supported for splitting. Can't be changed without
// also changing the SRPC signature for StreamInitWithSplit
const int kMaxModuleSplit = 16;
// Object which manages streaming bitcode over SRPC and threading.
SRPCStreamer *srpc_streamer;
// FDs of the object file(s).
int object_file_fd[kMaxModuleSplit];

DataStreamer *NaClBitcodeStreamer;

int DoTranslate(ArgStringList *CmdLineArgs) {
  if (CmdLineArgs == NULL) {
    return 1;
  }
  // Make an argv array from the input vector.
  size_t argc = CmdLineArgs->size();
  char **argv = new char *[argc + 1];
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
  while (entry != NULL && str_len) {
    // Call strdup(entry) since the str argument will ultimately be
    // freed by the SRPC message sender.
    CmdLineArgs->push_back(strdup(entry));
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
    "-mtriple=armv7a-none-nacl-gnueabi", "-mattr=+neon",
    "-float-abi=hard", NULL
  };
  static const char *llc_args_mips32[] = {
    "-mtriple=mipsel-none-nacl-gnu", NULL
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
  case PnaclTargetArchitectureMips_32: {
    llc_args = llc_args_mips32;
    break;
  }
  default:
    fprintf(stderr, "no target architecture match.\n");
    delete command_line;
    return NULL;
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
  for (size_t i = 0; llc_args[i] != NULL; i++)
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
    srpc_streamer->setFatalError("llc_main unspecified failure");
    return NULL;
  }
  delete data;
  return NULL;
}

// Actually do the work for stream initialization.
void do_stream_init(NaClSrpcRpc *rpc, NaClSrpcArg **out_args,
                    NaClSrpcClosure *done, StreamingThreadData* thread_data) {
  NaClSrpcClosureRunner runner(done);
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  srpc_streamer = new SRPCStreamer();
  std::string StrError;

  NaClBitcodeStreamer = srpc_streamer->init(
      run_streamed, reinterpret_cast<void *>(thread_data), &StrError);
  if (NaClBitcodeStreamer) {
    rpc->result = NACL_SRPC_RESULT_OK;
    out_args[0]->arrays.str = strdup("no error");
  } else {
    out_args[0]->arrays.str = strdup(StrError.c_str());
  }
}

void stream_init_with_split(NaClSrpcRpc *rpc, NaClSrpcArg **in_args,
                            NaClSrpcArg **out_args, NaClSrpcClosure *done) {
  ArgStringList *cmd_line_vec = GetDefaultCommandLine();
  if (!cmd_line_vec) {
    NaClSrpcClosureRunner runner(done);
    rpc->result = NACL_SRPC_RESULT_APP_ERROR;
    out_args[0]->arrays.str = strdup("Failed to get default commandline.");
    return;
  }
  AddFixedArguments(cmd_line_vec);

  int num_modules = in_args[0]->u.ival;
  if (num_modules < 1 || num_modules > kMaxModuleSplit) {
    NaClSrpcClosureRunner runner(done);
    rpc->result = NACL_SRPC_RESULT_APP_ERROR;
    out_args[0]->arrays.str = strdup("Invalid module split count.");
    return;
  }

  StreamingThreadData *thread_data =
      new StreamingThreadData(num_modules, cmd_line_vec);

  for (int i = 1; i <= num_modules; i++) {
    object_file_fd[i - 1] = in_args[i]->u.hval;
  }

  char *command_line = in_args[kMaxModuleSplit + 1]->arrays.carr;
  size_t command_line_len = in_args[kMaxModuleSplit + 1]->u.count;
  std::unique_ptr<ArgStringList> extra_vec(
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
  do_stream_init(rpc, out_args, done, thread_data);
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
  // TODO(eliben): We don't really use shared libraries now. At some
  // point this should be cleaned up from SRPC as well.
  out_args[0]->u.ival = false;
  out_args[1]->arrays.str = strdup("");
  out_args[2]->arrays.str = strdup("");
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  std::string StrError;
  if (srpc_streamer->streamEnd(&StrError)) {
    out_args[3]->arrays.str = strdup(StrError.c_str());
    return;
  }
  // SRPC deletes the strings returned when the closure is invoked.
  // Therefore we need to use strdup.
  out_args[3]->arrays.str = strdup("");
  rpc->result = NACL_SRPC_RESULT_OK;
}

const struct NaClSrpcHandlerDesc srpc_methods[] = {
  // Protocol for streaming:
  // StreamInitWithSplit(num_split, obj_fd x 16, cmdline_flags) -> error_str
  // StreamChunk(data) +
  // StreamEnd() -> (is_shared_lib,soname,dependencies,error_str)
  { "StreamInitWithSplit:ihhhhhhhhhhhhhhhhC:s", stream_init_with_split },
  { "StreamChunk:C:", stream_chunk },
  { "StreamEnd::isss", stream_end },
  { NULL, NULL },
};

} // namespace

int getObjectFileFD(unsigned Index) {
  assert(Index < kMaxModuleSplit);
  return object_file_fd[Index];
}

DataStreamer *getNaClBitcodeStreamer() { return NaClBitcodeStreamer; }

// Called from the compilation thread
void FatalErrorHandler(void *user_data, const std::string& reason,
                       bool gen_crash_diag) {
  srpc_streamer->setFatalError(reason);
}

fatal_error_handler_t getSRPCErrorHandler() { return FatalErrorHandler; }

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
