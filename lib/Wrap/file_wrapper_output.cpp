/* Copyright 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "llvm/Wrap/file_wrapper_output.h"
#include <stdlib.h>


FileWrapperOutput::FileWrapperOutput(const std::string& name)
    : _name(name) {
  _file = fopen(name.c_str(), "wb");
  if (NULL == _file) {
    fprintf(stderr, "Unable to open: %s\n", name.c_str());
    exit(1);
  }
}

FileWrapperOutput::~FileWrapperOutput() {
  fclose(_file);
}

bool FileWrapperOutput::Write(uint8_t byte) {
  return EOF != fputc(byte, _file);
}

bool FileWrapperOutput::Write(const uint8_t* buffer, size_t buffer_size) {
  if (!buffer) {
    return false;
  }

  if (buffer_size > 0) {
    return buffer_size == fwrite(buffer, 1, buffer_size, _file);
  } else {
    return true;
  }
}
