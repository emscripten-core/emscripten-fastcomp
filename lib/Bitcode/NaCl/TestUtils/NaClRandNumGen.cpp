//===- NaClRandNumGen.cpp - Random number generator -----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file provides a default implementation of a random number generator.
//
//===----------------------------------------------------------------------===//

#include "llvm/Bitcode/NaCl/NaClRandNumGen.h"

#include <vector>
#include <algorithm>

namespace naclfuzz {

// Put destructor in .cpp file guarantee vtable construction.
RandomNumberGenerator::~RandomNumberGenerator() {}

DefaultRandomNumberGenerator::DefaultRandomNumberGenerator(llvm::StringRef Seed)
    : Seed(Seed) {
  saltSeed(0);
}

uint64_t DefaultRandomNumberGenerator::operator()() {
  return Generator();
}

void DefaultRandomNumberGenerator::saltSeed(uint64_t Salt) {
  // Combine seed and salt and pass to generator.
  std::vector<uint32_t> Data;
  Data.reserve(Seed.size() + 2);
  Data.push_back(static_cast<uint32_t>(Salt));
  Data.push_back(static_cast<uint32_t>(Salt >> 32));
  std::copy(Seed.begin(), Seed.end(), Data.end());
  std::seed_seq SeedSeq(Data.begin(), Data.end());
  Generator.seed(SeedSeq);
}

} // end of namespace naclfuzz
