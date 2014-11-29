//===-- JSTargetTransformInfo.cpp - JS specific TTI pass ------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file
/// This file implements a TargetTransformInfo analysis pass specific to the
/// JS target machine. It uses the target's detailed information to provide
/// more precise answers to certain TTI queries, while letting the target
/// independent and default TTI implementations handle the rest.
///
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "jstti"
#include "JS.h"
#include "JSTargetMachine.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Target/TargetLowering.h"
#include "llvm/Target/CostTable.h"
using namespace llvm;

// Declare the pass initialization routine locally as target-specific passes
// don't havve a target-wide initialization entry point, and so we rely on the
// pass constructor initialization.
namespace llvm {
void initializeJSTTIPass(PassRegistry &);
}

namespace {

class JSTTI : public ImmutablePass, public TargetTransformInfo {
public:
  JSTTI() : ImmutablePass(ID) {
    llvm_unreachable("This pass cannot be directly constructed");
  }

  JSTTI(const JSTargetMachine *TM)
      : ImmutablePass(ID) {
    initializeJSTTIPass(*PassRegistry::getPassRegistry());
  }

  virtual void initializePass() {
    pushTTIStack(this);
  }

  virtual void finalizePass() {
    popTTIStack();
  }

  virtual void getAnalysisUsage(AnalysisUsage &AU) const {
    TargetTransformInfo::getAnalysisUsage(AU);
  }

  /// Pass identification.
  static char ID;

  /// Provide necessary pointer adjustments for the two base classes.
  virtual void *getAdjustedAnalysisPointer(const void *ID) {
    if (ID == &TargetTransformInfo::ID)
      return (TargetTransformInfo*)this;
    return this;
  }

  virtual PopcntSupportKind getPopcntSupport(unsigned IntTyWidthInBit) const;

  virtual unsigned getRegisterBitWidth(bool Vector) const;
};

} // end anonymous namespace

INITIALIZE_AG_PASS(JSTTI, TargetTransformInfo, "jstti",
                   "JS Target Transform Info", true, true, false)
char JSTTI::ID = 0;

ImmutablePass *
llvm::createJSTargetTransformInfoPass(const JSTargetMachine *TM) {
  return new JSTTI(TM);
}


//===----------------------------------------------------------------------===//
//
// JS cost model.
//
//===----------------------------------------------------------------------===//

JSTTI::PopcntSupportKind JSTTI::getPopcntSupport(unsigned TyWidth) const {
  assert(isPowerOf2_32(TyWidth) && "Ty width must be power of 2");
  // Hopefully we'll get popcnt in ES7, but for now, we just have software.
  return PSK_Software;
}

unsigned JSTTI::getRegisterBitWidth(bool Vector) const {
  if (Vector) {
    return 128;
  }

  return 32;
}
