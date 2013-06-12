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

#if defined(__native_client__)
#define DEBUG_TYPE "bitcode-stream"
#include "SRPCStreamer.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include <errno.h>

using llvm::dbgs;

const size_t QueueStreamer::queuesize_limit_;

size_t QueueStreamer::GetBytes(unsigned char *buf, size_t len) {
  size_t total_copied = 0;
  pthread_mutex_lock(&Mutex);
  while (!Done && queueSize() < len - total_copied) {
    size_t size = queueSize();
    DEBUG(dbgs() << "QueueStreamer::GetBytes len " << len << " size " <<
          size << " << waiting\n");
    queueGet(buf + total_copied, size);
    total_copied += size;
    pthread_cond_signal(&Cond);
    pthread_cond_wait(&Cond, &Mutex);
  }
  // If this is the last partial chunk, adjust len such that the amount we
  // fetch will be just the remaining bytes.
  if (Done && queueSize() < len - total_copied) {
    len = queueSize() + total_copied;
  }
  queueGet(buf + total_copied, len - total_copied);
  pthread_cond_signal(&Cond);
  pthread_mutex_unlock(&Mutex);
  return len;
}

size_t QueueStreamer::PutBytes(unsigned char *buf, size_t len) {
  size_t total_copied = 0;
  pthread_mutex_lock(&Mutex);
  while (capacityRemaining() < len - total_copied) {
    if (Bytes.size() * 2 > queuesize_limit_) {
      size_t space = capacityRemaining();
      queuePut(buf + total_copied, space);
      total_copied += space;
      pthread_cond_signal(&Cond);
      pthread_cond_wait(&Cond, &Mutex);
    } else {
      queueResize();
    }
  }
  queuePut(buf + total_copied, len - total_copied);
  pthread_cond_signal(&Cond);
  pthread_mutex_unlock(&Mutex);
  return len;
}

void QueueStreamer::SetDone() {
  // Still need the lock to avoid signaling between the check and
  // the wait in GetBytes.
  pthread_mutex_lock(&Mutex);
  Done = true;
  pthread_cond_signal(&Cond);
  pthread_mutex_unlock(&Mutex);
}

// Double the size of the queue. Called with Mutex to protect Cons/Prod/Bytes.
void QueueStreamer::queueResize() {
  int leftover = Bytes.size() - Cons;
  DEBUG(dbgs() << "resizing to " << Bytes.size() * 2 << " " << leftover << " "
        << Prod << " " << Cons << "\n");
  Bytes.resize(Bytes.size() * 2);
  if (Cons > Prod) {
    // There are unread bytes left between Cons and the previous end of the
    // buffer. Move them to the new end of the buffer.
    memmove(&Bytes[Bytes.size() - leftover], &Bytes[Cons], leftover);
    Cons = Bytes.size() - leftover;
  }
}

// Called with Mutex held to protect Cons, Prod, and Bytes
void QueueStreamer::queuePut(unsigned char *buf, size_t len) {
  size_t EndSpace = std::min(len, Bytes.size() - Prod);
  DEBUG(dbgs() << "put, len " << len << " Endspace " << EndSpace << " p " <<
        Prod << " c " << Cons << "\n");
  // Copy up to the end of the buffer
  memcpy(&Bytes[Prod], buf, EndSpace);
  // Wrap around if necessary
  memcpy(&Bytes[0], buf + EndSpace, len - EndSpace);
  Prod = (Prod + len) % Bytes.size();
}

// Called with Mutex held to protect Cons, Prod, and Bytes
void QueueStreamer::queueGet(unsigned char *buf, size_t len) {
  assert(len <= queueSize());
  size_t EndSpace = std::min(len, Bytes.size() - Cons);
  DEBUG(dbgs() << "get, len " << len << " Endspace " << EndSpace << " p " <<
        Prod << " c " << Cons << "\n");
  // Copy up to the end of the buffer
  memcpy(buf, &Bytes[Cons], EndSpace);
  // Wrap around if necessary
  memcpy(buf + EndSpace, &Bytes[0], len - EndSpace);
  Cons = (Cons + len) % Bytes.size();
}

llvm::DataStreamer *SRPCStreamer::init(void *(*Callback)(void *), void *arg,
                                       std::string *ErrMsg) {
  int err = pthread_create(&CompileThread, NULL, Callback, arg);
  if (err) {
    if (ErrMsg) *ErrMsg = std::string(strerror(errno));
    return NULL;
  }
  return &Q;
}

size_t SRPCStreamer::gotChunk(unsigned char *bytes, size_t len) {
  if (Error) return 0;
  return Q.PutBytes(bytes, len);
}

int SRPCStreamer::streamEnd(std::string *ErrMsg) {
  Q.SetDone();
  int err = pthread_join(CompileThread, NULL);
  if (err) {
    if (ErrMsg) *ErrMsg = std::string(strerror(errno));
    return err;
  }
  if (Error && ErrMsg) *ErrMsg = std::string("compile failed.");
  return Error;
}

#endif
