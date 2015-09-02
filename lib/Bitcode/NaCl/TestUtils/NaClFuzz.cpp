//===- NaClFuzz.cpp - Fuzz PNaCl bitcode records --------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements a basic fuzzer for a list of PNaCl bitcode records.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/STLExtras.h"
#include "llvm/Bitcode/NaCl/NaClFuzz.h"

using namespace llvm;

namespace {

// Names for edit actions.
const char *ActionNameArray[] = {
  "Insert",
  "Mutate",
  "Remove",
  "Replace",
  "Swap"
};

} // end of anonymous namespace

namespace naclfuzz {

const char *RecordFuzzer::actionName(EditAction Action) {
  return Action < array_lengthof(ActionNameArray)
                  ? ActionNameArray[Action] : "???";
}

RecordFuzzer::RecordFuzzer(NaClMungedBitcode &Bitcode,
                           RandomNumberGenerator &Generator)
    : Bitcode(Bitcode), Generator(Generator) {
  if (Bitcode.getBaseRecords().empty())
    report_fatal_error(
        "Sorry, the fuzzer doesn't know how to fuzz an empty record list");
}

RecordFuzzer::~RecordFuzzer() {}

void RecordFuzzer::clear() {
  Bitcode.removeEdits();
}

} // end of namespace naclfuzz
