//=- ThreadedStreamingCache.cpp - Cache for StreamingMemoryObject -*- C++ -*-=//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "ThreadedStreamingCache.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Mutex.h"
#include <cstring>

using namespace llvm;
using llvm::sys::ScopedLock;

ThreadedStreamingCache::ThreadedStreamingCache(
    llvm::StreamingMemoryObject *S) : Streamer(S),
                                      Cache(kCacheSize),
                                      MinObjectSize(0),
                                      CacheBase(-1) {
  static_assert((kCacheSize & (kCacheSize - 1)) == 0,
                "kCacheSize must be a power of 2");
}

void ThreadedStreamingCache::fetchCacheLine(uint64_t Address) const {
  uint64_t Base = Address & kCacheSizeMask;
  uint64_t BytesFetched;
  ScopedLock L(StreamerLock);
  if (Streamer->isValidAddress(Base + kCacheSize - 1)) {
    BytesFetched = Streamer->readBytes(&Cache[0], kCacheSize, Base);
    if (BytesFetched != kCacheSize) {
      llvm::report_fatal_error(
          "fetchCacheLine failed to fetch a full cache line");
    }
    MinObjectSize = Base + kCacheSize;
  } else {
    uint64_t End = Streamer->getExtent();
    assert(End > Address && End <= Base + kCacheSize);
    BytesFetched = Streamer->readBytes(&Cache[0], End - Base, Base);
    if (BytesFetched != (End - Base)) {
      llvm::report_fatal_error(
          "fetchCacheLine failed to fetch rest of stream");
    }
    MinObjectSize = End;
  }
  CacheBase = Base;
}

uint64_t ThreadedStreamingCache::readBytes(uint8_t* Buf, uint64_t Size,
                                           uint64_t Address) const {
  // To keep the cache fetch simple, we currently require that no request cross
  // the cache line. This isn't a problem for the bitcode reader because it only
  // fetches a byte or a word (word may be 4 to 8 bytes) at a time.
  uint64_t Upper = Address + Size;
  if (Address < CacheBase || Upper > CacheBase + kCacheSize) {
    // If completely outside of a cacheline, fetch the cacheline.
    if ((Address & kCacheSizeMask) != ((Upper - 1) & kCacheSizeMask))
      llvm::report_fatal_error("readBytes request spans cache lines");
    // Fetch a cache line first, which may be partial.
    fetchCacheLine(Address);
  }
  // Now the start Address should at least fit in the cache line,
  // but Upper may still be beyond the Extent / MinObjectSize, so clamp.
  if (Upper > MinObjectSize) {
    // If in the cacheline but stretches beyone the MinObjectSize,
    // only read up to MinObjectSize (caller uses readBytes to check EOF,
    // and can guess / try to read more). MinObjectSize should be the same
    // as EOF in this case otherwise it would have fit in the cacheline.
    Size = MinObjectSize - Address;
  }
  memcpy(Buf, &Cache[Address - CacheBase], Size);
  return Size;
}

uint64_t ThreadedStreamingCache::getExtent() const {
  llvm::report_fatal_error(
      "getExtent should not be called for pnacl streaming bitcode");
  return 0;
}

bool ThreadedStreamingCache::isValidAddress(uint64_t Address) const {
  if (Address < MinObjectSize)
    return true;
  ScopedLock L(StreamerLock);
  bool Valid = Streamer->isValidAddress(Address);
  if (Valid)
    MinObjectSize = Address;
  return Valid;
}

bool ThreadedStreamingCache::dropLeadingBytes(size_t S) {
  ScopedLock L(StreamerLock);
  return Streamer->dropLeadingBytes(S);
}

void ThreadedStreamingCache::setKnownObjectSize(size_t Size) {
  MinObjectSize = Size;
  ScopedLock L(StreamerLock);
  Streamer->setKnownObjectSize(Size);
}

const uint64_t ThreadedStreamingCache::kCacheSize;
const uint64_t ThreadedStreamingCache::kCacheSizeMask;
llvm::sys::SmartMutex<false> ThreadedStreamingCache::StreamerLock;
