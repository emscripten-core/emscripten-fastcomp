#include "llvm/Wrap/wrapper_output.h"

bool WrapperOutput::Write(const uint8_t* buffer, size_t buffer_size) {
  // Default implementation that uses the byte write routine.
  for (size_t i = 0; i < buffer_size; ++i) {
    if (!Write(buffer[i])) return false;
  }
  return true;
}
