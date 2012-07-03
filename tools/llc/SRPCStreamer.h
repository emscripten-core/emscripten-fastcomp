//===-- SRPCStreamer.cpp - Stream bitcode over SRPC  ----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//
//
//===----------------------------------------------------------------------===//

#ifndef SRPCSTREAMER_H
#define SRPCSTREAMER_H

#include <pthread.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>
#include "llvm/Support/DataStream.h"

// Implements LLVM's interface for fetching data from a stream source.
// Bitcode bytes from the RPC thread are placed here with PutBytes and buffered
// until the bitcode reader calls GetBytes to remove them.
class QueueStreamer : public llvm::DataStreamer {
 public:
 QueueStreamer() : Done(false), Prod(0), Cons(0) {
    pthread_mutex_init(&Mutex, NULL);
    pthread_cond_init(&Cond, NULL);
    Bytes.resize(64 * 1024);
  }
  // Called by the compilation thread. Wait for len bytes to become available,
  // and copy them into buf. If all bytes have been received and there are
  // fewer than len bytes available, copy all remaining bytes.
  // Return the number of bytes copied.
  virtual size_t GetBytes(unsigned char *buf, size_t len);

  // Called by the RPC thread. Copy len bytes from buf and wake up the
  // compilation thread if it is waiting. Return the number of bytes copied.
  size_t PutBytes(unsigned char *buf, size_t len);

  // Called by the RPC thread. Signal that all bytes have been received,
  // so the last call to GetBytes will return the remaining bytes rather
  // than waiting for the entire requested amound.
  void SetDone();

 private:
  bool Done;
  pthread_mutex_t Mutex;
  pthread_cond_t Cond;

  // Variables and functions to manage the circular queue
  std::vector<unsigned char> Bytes;
  size_t Prod; // Queue producer index
  size_t Cons; // Queue consumer index
  size_t queueSize() {
    return Prod >= Cons ? Prod - Cons : Bytes.size() - (Cons - Prod);
  }
  size_t capacityRemaining() {
    return (Prod >= Cons ? Bytes.size() - (Prod - Cons) : (Cons - Prod)) - 1;
  }
  void queuePut(unsigned char *buf, size_t len);
  void queueGet(unsigned char *buf, size_t len);
};

// Class to manage the compliation thread and serve as the interface from
// the SRPC thread
class SRPCStreamer  {
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
  // Called by the compilation thread. Signal that there was a compilation
  // error so the RPC thread can abort the stream.
  void setError() { Error = true; }
private:
  bool Error;
  QueueStreamer Q;
  pthread_t CompileThread;
};



#endif  // SRPCSTREAMER_H
