//===-- SRPCStreamer.h - Stream bitcode over SRPC  ------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Manages a Data stream where the producer pushes bytes via SRPC.
//
//===----------------------------------------------------------------------===//

#ifndef SRPCSTREAMER_H
#define SRPCSTREAMER_H

#include <pthread.h>
#include <string>
#include "llvm/Support/QueueStreamer.h"

// Class to manage the compliation thread and serve as the interface from
// the SRPC thread
class SRPCStreamer  {
  SRPCStreamer(const SRPCStreamer &) = delete;
  SRPCStreamer &operator=(const SRPCStreamer &) = delete;

public:
  SRPCStreamer() : Error(false) {}
  // Initialize streamer, create a new thread running Callback, and
  // return a pointer to the DataStreamer the threads will use to
  // synchronize. On error, return NULL and fill in the ErrorMsg string
  llvm::DataStreamer *init(void *(*Callback)(void *),
                           void *arg, std::string *ErrMsg);
  // Called by the RPC thread. Copy len bytes from buf. Return bytes copied.
  size_t gotChunk(unsigned char *bytes, size_t len);
  // Called by the RPC thread. Wait for the compilation thread to finish.
  int streamEnd(std::string *ErrMsg);
  // Called by the compilation thread. Set the error condition and also
  // terminate the thread.
  void setFatalError(const std::string& message);
private:
  int Error;
  std::string ErrorMessage;
  llvm::QueueStreamer Q;
  pthread_t CompileThread;
};

#endif  // SRPCSTREAMER_H
