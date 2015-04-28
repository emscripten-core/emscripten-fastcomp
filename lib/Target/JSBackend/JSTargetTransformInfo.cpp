//===-- JSTargetTransformInfo.cpp - JS specific TTI  ------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file
/// This file implements a TargetTransformInfo::Concept conforming object
/// specific to the JS target machine. It uses the target's detailed information
/// to provide more precise answers to certain TTI queries, while letting the
/// target independent and default TTI implementations handle the rest.
///
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "jstti"
#include "JS.h"
#include "JSTargetMachine.h"
#include "JSTargetTransformInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Target/TargetLowering.h"
#include "llvm/Target/CostTable.h"
using namespace llvm;

//===----------------------------------------------------------------------===//
//
// JS cost model.
//
//===----------------------------------------------------------------------===//

TargetTransformInfo::PopcntSupportKind JSTTI::getPopcntSupport(
    unsigned TyWidth) {
  assert(isPowerOf2_32(TyWidth) && "Ty width must be power of 2");
  // Hopefully we'll get popcnt in ES7, but for now, we just have software.
  return TargetTransformInfo::PSK_Software;
}

unsigned JSTTI::getRegisterBitWidth(bool Vector) const {
  if (Vector) {
    return 128;
  }

  return 32;
}

unsigned JSTTI::getArithmeticInstrCost(
    unsigned Opcode, Type *Ty, TTI::OperandValueKind Opd1Info,
    TTI::OperandValueKind Opd2Info, TTI::OperandValueProperties Opd1PropInfo,
    TTI::OperandValueProperties Opd2PropInfo) {
  const unsigned Nope = 65536;

  unsigned Cost = BasicTTIImplBase<JSTTI>::getArithmeticInstrCost(Opcode, Ty, Opd1Info, Opd2Info);

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
        if (Opd2Info != TTI::OK_UniformValue && Opd2Info != TTI::OK_UniformConstantValue)
          Cost = Cost * VTy->getNumElements() + 100;
        break;
    }
  }

  return Cost;
}

unsigned JSTTI::getVectorInstrCost(unsigned Opcode, Type *Val, unsigned Index) {
  unsigned Cost = BasicTTIImplBase::getVectorInstrCost(Opcode, Val, Index);

  // SIMD.js' insert/extract currently only take constant indices.
  if (Index == -1u)
      return Cost + 100;

  return Cost;
}

void JSTTI::getUnrollingPreferences(Loop *L,
                                    TTI::UnrollingPreferences &UP) const {
  // We generally don't want a lot of unrolling.
  UP.Partial = false;
  UP.Runtime = false;
}
