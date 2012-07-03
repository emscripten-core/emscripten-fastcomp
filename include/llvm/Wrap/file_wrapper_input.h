/* Copyright 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// Defines utility allowing files for bitcode input wrapping.

#ifndef FILE_WRAPPER_INPUT_H__
#define FILE_WRAPPER_INPUT_H__

#include "llvm/Support/support_macros.h"
#include "llvm/Wrap/wrapper_input.h"

#include <stdio.h>
#include <string>

// Define a class to wrap named files.
class FileWrapperInput : public WrapperInput {
 public:
  FileWrapperInput(const std::string& name);
  ~FileWrapperInput();
  // Tries to read the requested number of bytes into the buffer. Returns the
  // actual number of bytes read.
  virtual size_t Read(uint8_t* buffer, size_t wanted);
  // Returns true if at end of file. Note: May return false
  // until Read is called, and returns 0.
  virtual bool AtEof();
  // Returns the size of the file (in bytes).
  virtual off_t Size();
  // Moves to the given offset within the file. Returns
  // false if unable to move to that position.
  virtual bool Seek(uint32_t pos);
 private:
  // The name of the file.
  std::string _name;
  // True once eof has been encountered.
  bool _at_eof;
  // True if size has been computed.
  bool _size_found;
  // The size of the file.
  off_t _size;
  // The corresponding (opened) file.
  FILE* _file;
 private:
  DISALLOW_CLASS_COPY_AND_ASSIGN(FileWrapperInput);
};

#endif // FILE_WRAPPER_INPUT_H__
