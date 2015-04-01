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

  virtual unsigned getArithmeticInstrCost(unsigned Opcode, Type *Ty,
                                          OperandValueKind Opd1Info = OK_AnyValue,
                                          OperandValueKind Opd2Info = OK_AnyValue) const;

  virtual unsigned getVectorInstrCost(unsigned Opcode, Type *Val,
                                      unsigned Index = -1) const;

  virtual void getUnrollingPreferences(Loop *L, UnrollingPreferences &UP) const;
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

unsigned JSTTI::getArithmeticInstrCost(unsigned Opcode, Type *Ty,
                                       OperandValueKind Opd1Info,
                                       OperandValueKind Opd2Info) const {
  const unsigned Nope = 65536;

  unsigned Cost = TargetTransformInfo::getArithmeticInstrCost(Opcode, Ty, Opd1Info, Opd2Info);

  if (VectorType *VTy = dyn_cast<VectorType>(Ty)) {
    switch (VTy->getNumElements()) {
    case 4:
      // SIMD.js supports int32x4 and float32x4, and we can emulate <4 x i1>.
      if (!VTy->getElementType()->isIntegerTy(1) &&
          !VTy->getElementType()->isIntegerTy(32) &&
          !VTy->getElementType()->isFloatTy())
      {
          return Nope;
      }
      break;
    default:
      // Wait until the other types are optimized.
      return Nope;
    }

    switch (Opcode) {
      case Instruction::LShr:
      case Instruction::AShr:
      case Instruction::Shl:
        // SIMD.js' shifts are currently only ByScalar.
        if (Opd2Info != OK_UniformValue && Opd2Info != OK_UniformConstantValue)
          Cost = Cost * VTy->getNumElements() + 100;
        break;
    }
  }

  return Cost;
}

unsigned JSTTI::getVectorInstrCost(unsigned Opcode, Type *Val, unsigned Index) const {
  unsigned Cost = TargetTransformInfo::getVectorInstrCost(Opcode, Val, Index);

  // SIMD.js' insert/extract currently only take constant indices.
  if (Index == -1u)
      return Cost + 100;

  return Cost;
}

void JSTTI::getUnrollingPreferences(Loop *L, UnrollingPreferences &UP) const {
  // We generally don't want a lot of unrolling.
  UP.Partial = false;
  UP.Runtime = false;
}
