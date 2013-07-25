//===-- SRPCStreamer.h - Stream bitcode over SRPC  ------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
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
// The blocking behavior of GetBytes and PutBytes means that if the
// compilation happens faster than the bytes come in from the browser, the
// whole pipeline can block waiting for the RPC thread to put more bytes.

class QueueStreamer : public llvm::DataStreamer {
 public:
 QueueStreamer() : Done(false), Prod(0), Cons(0) {
    pthread_mutex_init(&Mutex, NULL);
    pthread_cond_init(&Cond, NULL);
    Bytes.resize(64 * 1024);
  }

  // Called by the compilation thread. Copy len bytes from the queue into
  // buf. If there are less than len bytes available, copy as many as
  // there are, signal the RPC thread, and block to wait for the rest.
  // If all bytes have been received from the browser and there are
  // fewer than len bytes available, copy all remaining bytes.
  // Return the number of bytes copied.
  virtual size_t GetBytes(unsigned char *buf, size_t len);

  // Called by the RPC thread. Copy len bytes from buf into the queue.
  // If there is not enough space in the queue, copy as many bytes as
  // will fit, signal the compilation thread, and block until there is
  // enough space for the rest.
  // Return the number of bytes copied.
  size_t PutBytes(unsigned char *buf, size_t len);

  // Called by the RPC thread. Signal that all bytes have been received,
  // so the last call to GetBytes will return the remaining bytes rather
  // than waiting for the entire requested amound.
  void SetDone();

 private:
  bool Done;
  pthread_mutex_t Mutex;
  pthread_cond_t Cond;
  // Maximum size of the queue. The limitation on the queue size means that
  // if the compilation happens slower than bytes arrive from the network,
  // the queue will fill up, the RPC thread will be blocked most of the time,
  // the RPC thread on the browser side will be waiting for the SRPC to return,
  // and the buffer on the browser side will grow unboundedly until the
  // whole bitcode file arrives (which is better than having the queue on
  // the untrusted side consume all that memory).
  // The partial-copying behavior of GetBytes and PutBytes prevents deadlock
  // even if the requested number of bytes is greater than the size limit
  // (although it will of course be less efficient).
  // The initial size of the queue is expected to be smaller than this, but
  // if not, it will simply never be resized.
  const static size_t queuesize_limit_ = 256 * 1024;

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
  void queueResize();
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
