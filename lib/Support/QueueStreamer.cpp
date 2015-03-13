//===--- llvm/Support/QueueStreamer.cpp - Producer/consumer data streamer -===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements QueueStreamer.
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/QueueStreamer.h"

#include <cassert>
#include <cstring>

#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define DEBUG_TYPE "queue-streamer"

namespace llvm {

size_t QueueStreamer::GetBytes(unsigned char *Buf, size_t Len) {
  size_t TotalCopied = 0;
  std::unique_lock<LockType> L(Mutex);
  while (!Done && queueSize() < Len - TotalCopied) {
    size_t Size = queueSize();
    DEBUG(dbgs() << "QueueStreamer::GetBytes Len " << Len << " size " <<
          Size << " << waiting\n");
    queueGet(Buf + TotalCopied, Size);
    TotalCopied += Size;
    Cond.notify_one();
    Cond.wait(L);
  }
  // If this is the last partial chunk, adjust Len such that the amount we
  // fetch will be just the remaining bytes.
  if (Done && queueSize() < Len - TotalCopied) {
    Len = queueSize() + TotalCopied;
  }
  queueGet(Buf + TotalCopied, Len - TotalCopied);
  Cond.notify_one();
  return Len;
}

size_t QueueStreamer::PutBytes(unsigned char *Buf, size_t Len) {
  size_t TotalCopied = 0;
  std::unique_lock<LockType> L(Mutex);
  while (capacityRemaining() < Len - TotalCopied) {
    if (Bytes.size() * 2 > QueueStreamer::MaxSize) {
      size_t Space = capacityRemaining();
      queuePut(Buf + TotalCopied, Space);
      TotalCopied += Space;
      Cond.notify_one();
      Cond.wait(L);
    } else {
      queueResize();
    }
  }
  queuePut(Buf + TotalCopied, Len - TotalCopied);
  Cond.notify_one();
  return Len;
}

void QueueStreamer::SetDone() {
  // Still need the lock to avoid signaling between the check and
  // the wait in GetBytes.
  std::unique_lock<LockType> L(Mutex);
  Done = true;
  Cond.notify_one();
}

// Double the size of the queue. Called with Mutex to protect Cons/Prod/Bytes.
void QueueStreamer::queueResize() {
  int Leftover = Bytes.size() - Cons;
  DEBUG(dbgs() << "resizing to " << Bytes.size() * 2 << " " << Leftover << " "
        << Prod << " " << Cons << "\n");
  Bytes.resize(Bytes.size() * 2);
  if (Cons > Prod) {
    // There are unread bytes left between Cons and the previous end of the
    // buffer. Move them to the new end of the buffer.
    memmove(&Bytes[Bytes.size() - Leftover], &Bytes[Cons], Leftover);
    Cons = Bytes.size() - Leftover;
  }
}

// Called with Mutex held to protect Cons, Prod, and Bytes
void QueueStreamer::queuePut(unsigned char *Buf, size_t Len) {
  size_t EndSpace = std::min(Len, Bytes.size() - Prod);
  DEBUG(dbgs() << "put, Len " << Len << " Endspace " << EndSpace << " p " <<
        Prod << " c " << Cons << "\n");
  // Copy up to the end of the buffer
  memcpy(&Bytes[Prod], Buf, EndSpace);
  // Wrap around if necessary
  memcpy(&Bytes[0], Buf + EndSpace, Len - EndSpace);
  Prod = (Prod + Len) % Bytes.size();
}

// Called with Mutex held to protect Cons, Prod, and Bytes
void QueueStreamer::queueGet(unsigned char *Buf, size_t Len) {
  assert(Len <= queueSize());
  size_t EndSpace = std::min(Len, Bytes.size() - Cons);
  DEBUG(dbgs() << "get, Len " << Len << " Endspace " << EndSpace << " p " <<
        Prod << " c " << Cons << "\n");
  // Copy up to the end of the buffer
  memcpy(Buf, &Bytes[Cons], EndSpace);
  // Wrap around if necessary
  memcpy(Buf + EndSpace, &Bytes[0], Len - EndSpace);
  Cons = (Cons + Len) % Bytes.size();
}

} // end of namespace llvm
