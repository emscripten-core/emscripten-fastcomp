//===- NaClFuzz.h - Fuzz PNaCl bitcode records ------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines a basic fuzzer for a list of PNaCl bitcode records.
//
// *** WARNING *** The implementation of the fuzzer uses a random
// number generator.  As a result, this code is not thread safe.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_BITCODE_NACL_NACLFUZZ_H
#define LLVM_BITCODE_NACL_NACLFUZZ_H

#include "llvm/Bitcode/NaCl/NaClBitcodeMungeUtils.h"
#include "llvm/Bitcode/NaCl/NaClRandNumGen.h"

namespace naclfuzz {

using namespace llvm;

/// \brief Fuzzes a list of editable bitcode records.
class RecordFuzzer {
  RecordFuzzer(const RecordFuzzer&) = delete;
  void operator=(const RecordFuzzer&) = delete;
public:
  typedef NaClMungedBitcode::iterator iterator;

  /// \brief The set of possible fuzzing actions.
  enum EditAction {
    /// \brief Inserts a new record into the list of bitcode records.
    InsertRecord,
    /// \brief Mutate contents of an existing bitcode record.
    MutateRecord,
    /// \brief Removes an existing record from the list of bitcode
    /// records.
    RemoveRecord,
    /// \brief Replaces an existing record with a new bitcode record.
    ReplaceRecord,
    /// \brief Swaps two records in the bitcode record list.
    SwapRecord
  };

  virtual ~RecordFuzzer();

  /// \brief Generates a random mutation of the bitcode, using the
  /// provided random number generator. Percentage (a value between 0
  /// and 1 defined by Count/Base) is used to define the number of
  /// fuzzing actions applied to the bitcode.  Returns true if fuzzing
  /// succeeded.
  ///
  /// May be called an arbitrary number of times. Results are left in
  /// the munged bitcode records passed into static method
  /// createSimpleRecordFuzzer.
  virtual bool fuzz(unsigned Count, unsigned Base=100) = 0;

  /// \brief Shows how many times each record was edited in the
  /// corresponding (input) bitcode, over all calls to fuzz.
  virtual void showRecordDistribution(raw_ostream &Out) const = 0;

  /// \brief Shows how many times each type of edit action was applied
  /// to the corresponding bitcode, over all calls to fuzz.
  virtual void showEditDistribution(raw_ostream &Out) const = 0;

  // Creates an instance of a fuzzer for the given bitcode.
  static RecordFuzzer
  *createSimpleRecordFuzzer(NaClMungedBitcode &Bitcode,
                            RandomNumberGenerator &RandGenerator);

  /// Returns printable name for the edit action.
  static const char *actionName(EditAction Action);

protected:
  RecordFuzzer(NaClMungedBitcode &Bitcode, RandomNumberGenerator &Generator);

  // Holds the bitcode being munged.
  NaClMungedBitcode &Bitcode;

  // Hold the random number generator.
  RandomNumberGenerator &Generator;

  // Erases the last fuzzing result from the munged bitcode records
  // in Bitcode.
  virtual void clear();
};

} // end of namespace naclfuzz

#endif // LLVM_BITCODE_NACL_NACLFUZZ_H
