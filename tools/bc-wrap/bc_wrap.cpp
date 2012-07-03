/* Copyright 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */
/*
 * Utility to wrap a .bc file, using LLVM standard+ custom headers.
 */

#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Wrap/bitcode_wrapperer.h"
#include "llvm/Wrap/file_wrapper_input.h"
#include "llvm/Wrap/file_wrapper_output.h"

#include <ctype.h>
#include <string.h>

using namespace llvm;

static cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<input file>"), cl::Required);

static cl::opt<std::string>
OutputFilename("o", cl::desc("<output file>"));

static cl::opt<bool> UnwrapFlag("u",
                                cl::desc("unwrap rather than wrap the file"),
                                cl::init(false));

static cl::opt<bool> VerboseFlag("v",
                                 cl::desc("print verbose header information"),
                                 cl::init(false));

static cl::opt<bool> DryRunFlag("n",
                                cl::desc("Dry run (implies -v)"),
                                cl::init(false));

// Accept the hash on the command line to avoid having to include sha1
// library with the LLVM code
static cl::opt<std::string> BitcodeHash("hash",
  cl::desc("Hash of bitcode (ignored if -u is given)"));

const int kMaxBinaryHashLen = 32;

// Convert ASCII hex hash to binary hash. return buffer and length.
// The caller must free the returned buffer.
static uint8_t* ParseBitcodeHash(int* len) {
  if (BitcodeHash.size() > kMaxBinaryHashLen * 2 ||
      BitcodeHash.size() % 2) return NULL;
  *len = BitcodeHash.size() / 2;
  uint8_t* buf = new uint8_t[*len];
  const char* arg = BitcodeHash.data();
  for (size_t i = 0; i < BitcodeHash.size() / 2; i++) {
    unsigned int r; // glibc has %hhx but it's nonstandard
    if (!isxdigit(*(arg + 2 * i + 1)) || // sscanf ignores trailing junk
        !sscanf(arg + 2 * i, "%2x", &r) ||
        r > std::numeric_limits<uint8_t>::max()) {
      delete [] buf;
      return NULL;
    }
    buf[i] = static_cast<uint8_t>(r);
  }
  return buf;
}

int main(const int argc, const char* argv[]) {
  bool success = true;
  cl::ParseCommandLineOptions(argc, argv, "bitcode wrapper/unwrapper\n");
  if (OutputFilename == "") {
    // Default to input file = output file. The cl lib doesn't seem to
    // directly support initializing one opt from another.
    OutputFilename = InputFilename;
  }
  if (DryRunFlag) VerboseFlag = true;
  sys::fs::file_status outfile_status;
  std::string outfile_temp;
  outfile_temp = std::string(OutputFilename) + ".temp";
  if (UnwrapFlag) {
    FileWrapperInput inbc(InputFilename);
    FileWrapperOutput outbc(outfile_temp);
    BitcodeWrapperer wrapperer(&inbc, &outbc);
    if (wrapperer.IsInputBitcodeWrapper()) {
      if (VerboseFlag) {
        fprintf(stderr, "Headers read from infile:\n");
        wrapperer.PrintWrapperHeader();
      }
      if (DryRunFlag)
        return 0;
      success = wrapperer.GenerateRawBitcodeFile();
    }
  } else {
    FileWrapperInput inbc(InputFilename);
    FileWrapperOutput outbc(outfile_temp);
    BitcodeWrapperer wrapperer(&inbc, &outbc);
    if (BitcodeHash.size()) {
      // SHA-2 hash is 256 bit
      int hash_len;
      uint8_t* buf = ParseBitcodeHash(&hash_len);
      if (!buf) {
        fprintf(stderr, "Bitcode hash must be a hex string <= 64 chars.\n");
        exit(1);
      }
      BCHeaderField hash(BCHeaderField::kBitcodeHash, hash_len, buf);
      wrapperer.AddHeaderField(&hash);
    }
    if (VerboseFlag) {
      fprintf(stderr, "Headers generated:\n");
      wrapperer.PrintWrapperHeader();
    }
    if (DryRunFlag)
      return 0;
    success = wrapperer.GenerateWrappedBitcodeFile();
  }
  error_code ec;
  if ((ec = sys::fs::rename(outfile_temp, OutputFilename))) {
    fprintf(stderr, "Could not rename temporary: %s\n", ec.message().c_str());
    success = false;
  }
  if (success) return 0;
  fprintf(stderr, "error: Unable to generate a proper %s bitcode file!\n",
          (UnwrapFlag ? "unwrapped" : "wrapped"));
  return 1;
}
