//===- NaClRandNumGen.h - random number generator ---------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines a random number generator API for 64-bit unsigned
// values, and a corresponding default implementation.
//
// *** WARNING *** One should assume that random number generators are not
// thread safe.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_BITCODE_NACL_NACLRANDNUMGEN_H
#define LLVM_BITCODE_NACL_NACLRANDNUMGEN_H

#include "llvm/ADT/StringRef.h"
#include <random>
#include <stdint.h>

namespace naclfuzz {

/// Defines API for a random number generator to use with fuzzing.
class RandomNumberGenerator {
  RandomNumberGenerator(const RandomNumberGenerator&) = delete;
  void operator=(const RandomNumberGenerator&) = delete;
public:
  virtual ~RandomNumberGenerator();
  /// Returns a random number.
  virtual uint64_t operator()() = 0;
  // Returns a random value in [0..Limit)
  uint64_t chooseInRange(uint64_t Limit) {
    return (*this)() % Limit;
  }
protected:
  RandomNumberGenerator() {}
};

/// Defines a random number generator based on C++ generator std::mt19937_64.
class DefaultRandomNumberGenerator : public RandomNumberGenerator {
  DefaultRandomNumberGenerator(const DefaultRandomNumberGenerator&) = delete;
  void operator=(const DefaultRandomNumberGenerator&) = delete;
public:
  DefaultRandomNumberGenerator(llvm::StringRef Seed);
  uint64_t operator()() final;
  ~DefaultRandomNumberGenerator() final {}
  // Resets random number seed by salting the seed of constructor with Salt.
  void saltSeed(uint64_t Salt);
private:
  // 64-bit Mersenne Twister by Matsumoto and Nishimura, 2000
  // http://en.cppreference.com/w/cpp/numeric/random/mersenne_twister_engine
  // This RNG is deterministically portable across C++11
  // implementations.
  std::mt19937_64 Generator;
  // Seed for the random number generator.
  std::string Seed;
};

} // end of namespace naclfuzz

#endif // LLVM_BITCODE_NACL_NACLRANDNUMGEN_H
