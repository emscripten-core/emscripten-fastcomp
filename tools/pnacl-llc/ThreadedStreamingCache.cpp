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

int ThreadedStreamingCache::fetchCacheLine(uint64_t address) const {
  uint64_t Base = address & kCacheSizeMask;
  int Ret;
  ScopedLock L(StreamerLock);
  if (Streamer->isValidAddress(Base + kCacheSize - 1)) {
    Ret = Streamer->readBytes(Base, kCacheSize, &Cache[0]);
    assert(Ret == 0);
    MinObjectSize = Base + kCacheSize;
  } else {
    uint64_t End = Streamer->getExtent();
    assert(End > address && End <= Base + kCacheSize);
    Ret = Streamer->readBytes(Base, End - Base, &Cache[0]);
    assert(Ret == 0);
    MinObjectSize = End;
  }
  CacheBase = Base;
  return Ret;
}

int ThreadedStreamingCache::readByte(
    uint64_t address, uint8_t* ptr) const {
  if (address < CacheBase || address >= CacheBase + kCacheSize) {
    if(fetchCacheLine(address))
      return -1;
  }
  *ptr = Cache[address - CacheBase];
  return 0;
}

int ThreadedStreamingCache::readBytes(
    uint64_t address, uint64_t size, uint8_t* buf) const {
  // To keep the cache fetch simple, we currently require that no request cross
  // the cache line. This isn't a problem for the bitcode reader because it only
  // fetches a byte or a word at a time.
  if (address < CacheBase || (address + size) > CacheBase + kCacheSize) {
    if ((address & kCacheSizeMask) != ((address + size - 1) & kCacheSizeMask))
      llvm::report_fatal_error("readBytes request spans cache lines");
    if(fetchCacheLine(address))
      return -1;
  }
  memcpy(buf, &Cache[address - CacheBase], size);
  return 0;
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

bool ThreadedStreamingCache::isObjectEnd(uint64_t address) const {
  if (address < MinObjectSize)
    return false;
  ScopedLock L(StreamerLock);
  if (Streamer->isValidAddress(address)) {
    MinObjectSize = address;
    return false;
  }
  return Streamer->isObjectEnd(address);
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
