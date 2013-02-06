//===-- NaCl.h - NaCl Analysis ---------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_ANALYSIS_NACL_H
#define LLVM_ANALYSIS_NACL_H

#include "llvm/ADT/Twine.h"
#include <string>
#include <vector>

namespace llvm {

class FunctionPass;
class ModulePass;

FunctionPass *createPNaClABIVerifyFunctionsPass();
ModulePass *createPNaClABIVerifyModulePass();

}

// A simple class to store verification errors. This allows them to be saved
// and printed by the analysis passes' print() methods, while still allowing
// the messages to be easily constructed with Twine.
class ABIVerifyErrors {
 public:
  void addError(const llvm::Twine &Error) {
    Messages.push_back(Error.str());
  }
  typedef std::vector<std::string>::const_iterator const_iterator;
  const_iterator begin() const { return Messages.begin(); }
  const_iterator end() const   { return Messages.end(); }
  bool empty() const           { return Messages.empty(); }
  void clear()                 { Messages.clear(); }
 private:
  std::vector<std::string> Messages;
};

#endif
