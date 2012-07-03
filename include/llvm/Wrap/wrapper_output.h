/* Copyright 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// Defines a generic interface to a file/memory region that
// contains a generated wrapped bitcode file, bitcode file,
// or data file.

#ifndef LLVM_WRAP_WRAPPER_OUTPUT_H__
#define LLVM_WRAP_WRAPPER_OUTPUT_H__

#include <stdint.h>
#include <stddef.h>

#include "llvm/Support/support_macros.h"

// The following is a generic interface to a file/memory region
// that contains a generated bitcode file, wrapped bitcode file,
// or a data file.
class WrapperOutput {
 public:
  WrapperOutput() {}
  virtual ~WrapperOutput() {}
  // Writes a single byte, returning false if unable to write.
  virtual bool Write(uint8_t byte) = 0;
  // Writes the specified number of bytes in the buffer to
  // output. Returns false if unable to write.
  virtual bool Write(const uint8_t* buffer, size_t buffer_size);
 private:
  DISALLOW_CLASS_COPY_AND_ASSIGN(WrapperOutput);
};

#endif  // LLVM_WRAP_WRAPPER_OUTPUT_H__
