//===-- SRPCStreamer.cpp - Stream bitcode over SRPC  ----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#if defined(PNACL_BROWSER_TRANSLATOR)
#include "SRPCStreamer.h"

#include <errno.h>

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
  if (__sync_fetch_and_add(&Error, 0)) return 0; // Atomic read.
  return Q.PutBytes(bytes, len);
}

int SRPCStreamer::streamEnd(std::string *ErrMsg) {
  Q.SetDone();
  int err = pthread_join(CompileThread, NULL);
  __sync_synchronize();
  if (Error) {
    if (ErrMsg)
      *ErrMsg = std::string("PNaCl Translator Error: " + ErrorMessage);
    return 1;
  } else if (err) {
    if (ErrMsg) *ErrMsg = std::string(strerror(errno));
    return err;
  }
  return 0;
}

void SRPCStreamer::setFatalError(const std::string& message) {
  __sync_fetch_and_add(&Error, 1);
  ErrorMessage = message;
  __sync_synchronize();
  pthread_exit(NULL);
}

#endif // __native_client__
