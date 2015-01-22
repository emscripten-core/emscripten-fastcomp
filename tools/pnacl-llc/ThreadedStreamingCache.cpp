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

bool ThreadedStreamingCache::fetchCacheLine(uint64_t address) const {
  uint64_t Base = address & kCacheSizeMask;
  uint64_t Ret;
  ScopedLock L(StreamerLock);
  if (Streamer->isValidAddress(Base + kCacheSize - 1)) {
    Ret = Streamer->readBytes(&Cache[0], kCacheSize, Base);
    if (Ret != kCacheSize)
      return true;
    MinObjectSize = Base + kCacheSize;
  } else {
    uint64_t End = Streamer->getExtent();
    assert(End > address && End <= Base + kCacheSize);
    Ret = Streamer->readBytes(&Cache[0], End - Base, Base);
    if (Ret != (End - Base))
      return true;
    MinObjectSize = End;
  }
  CacheBase = Base;
  return false;
}

uint64_t ThreadedStreamingCache::readBytes(uint8_t* Buf, uint64_t Size,
                                           uint64_t Address) const {
  // To keep the cache fetch simple, we currently require that no request cross
  // the cache line. This isn't a problem for the bitcode reader because it only
  // fetches a byte or a word at a time.
  if (Address < CacheBase || (Address + Size) > CacheBase + kCacheSize) {
    if ((Address & kCacheSizeMask) != ((Address + Size - 1) & kCacheSizeMask))
      llvm::report_fatal_error("readBytes request spans cache lines");
    if (fetchCacheLine(Address))
      llvm::report_fatal_error("readBytes failed to fetch a full cache line");
  }
  memcpy(Buf, &Cache[Address - CacheBase], Size);
  return Size;
}

uint64_t ThreadedStreamingCache::getExtent() const {
  llvm::report_fatal_error(
      "getExtent should not be called for pnacl streaming bitcode");
  return 0;
}

bool ThreadedStreamingCache::isValidAddress(uint64_t address) const {
  if (address < MinObjectSize)
    return true;
  ScopedLock L(StreamerLock);
  bool Valid = Streamer->isValidAddress(address);
  if (Valid)
    MinObjectSize = address;
  return Valid;
}

bool ThreadedStreamingCache::dropLeadingBytes(size_t s) {
  ScopedLock L(StreamerLock);
  return Streamer->dropLeadingBytes(s);
}

void ThreadedStreamingCache::setKnownObjectSize(size_t size) {
  MinObjectSize = size;
  ScopedLock L(StreamerLock);
  Streamer->setKnownObjectSize(size);
}

const uint64_t ThreadedStreamingCache::kCacheSize;
const uint64_t ThreadedStreamingCache::kCacheSizeMask;
llvm::sys::SmartMutex<false> ThreadedStreamingCache::StreamerLock;
