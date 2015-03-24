//===-- QueueStreamer.h - Stream data from external source ------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Implements LLVM's interface for fetching data from a stream source
// (DataStreamer). Typically, bytes are pushed by an external source.
// Typically, the there is a handler thread that waits for push requests
// and calls PutBytes, and a consumer thread that calls GetBytes. The bytes
// are buffered until the consumer calls GetBytes to remove them.
// The blocking behavior of GetBytes and PutBytes means that if the consumer
// is faster than the producer, then the whole consumer pipeline can block
// waiting for the producer. Similarly, if the consumer is slower, then PutBytes
// will block, and the external source (producer) will know how far along
// the consumer has advanced, modulo the amount in the bounded buffer of
// the QueueStreamer.
//
//===----------------------------------------------------------------------===//


#ifndef LLVM_SUPPORT_QUEUESTREAMER_H
#define LLVM_SUPPORT_QUEUESTREAMER_H

#include <condition_variable>
#include <mutex>
#include <vector>

#include "llvm/Support/DataStream.h"

namespace llvm {

class QueueStreamer : public DataStreamer {
  QueueStreamer(const QueueStreamer &) = delete;
  QueueStreamer &operator=(const QueueStreamer &) = delete;

  enum {
    // Initial size of the queue's buffer.
    BaseSize = 64 * 1024,
    // Maximum size of the queue. Since PutBytes and GetBytes may block,
    // the partial-copying behavior of GetBytes and PutBytes allows progress
    // to be made and prevents deadlock even if the requested number of
    // bytes from Put/Get are greater than the size limit.
    // Keep the max size "small" so that the external source can approximate
    // how far along the consumer has advanced (modulo this buffer amount).
    MaxSize = 256 * 1024
  };

public:
  QueueStreamer() : Done(false), Prod(0), Cons(0) {
    Bytes.resize(BaseSize);
  }

  // Copy Len bytes from the QueueStreamer into buf. If there are less
  // than Len bytes available, copy as many as there are and signal the
  // thread that may be blocking on PutBytes, and block GetBytes to wait
  // for the rest. If all bytes have been received (SetDone is called)
  // and there are fewer than Len bytes available, copy all remaining bytes.
  // Return the number of bytes copied.
  size_t GetBytes(unsigned char *Buf, size_t Len) override;

  // Copy Len bytes from Buf into the QueueStreamer. If there is not enough
  // space in the queue, copy as many bytes as will fit, signal the thread
  // that may be blocking on GetBytes, and block until there is enough space
  // for the rest. Return the number of bytes copied.
  size_t PutBytes(unsigned char *Buf, size_t Len);

  // Called by the same thread that does PutBytes. Signals the end of the
  // data stream and may unblock GetBytes.
  void SetDone();

private:
  bool Done;
  typedef std::mutex LockType;
  LockType Mutex;
  std::condition_variable Cond;

  // Variables and functions to manage the circular queue
  std::vector<unsigned char> Bytes;
  size_t Prod; // Queue producer index
  size_t Cons; // Queue consumer index

  size_t queueSize() const {
    return Prod >= Cons ? Prod - Cons : Bytes.size() - (Cons - Prod);
  }
  size_t capacityRemaining() const {
    return (Prod >= Cons ? Bytes.size() - (Prod - Cons) : (Cons - Prod)) - 1;
  }
  void queueResize();
  void queuePut(unsigned char *Buf, size_t Len);
  void queueGet(unsigned char *Buf, size_t Len);
};

} // end of namespace llvm

#endif  // LLVM_SUPPORT_QUEUESTREAMER_H
