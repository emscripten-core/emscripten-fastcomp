//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// \file
// This file implements a TargetTransformInfo analysis pass specific to the
// JS target machine. It uses the target's detailed information to provide
// more precise answers to certain TTI queries, while letting the target
// independent and default TTI implementations handle the rest.
//
//===----------------------------------------------------------------------===//

#include "JSTargetTransformInfo.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/CodeGen/BasicTTIImpl.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Debug.h"
#include "llvm/Target/CostTable.h"
#include "llvm/Target/TargetLowering.h"
using namespace llvm;

#define DEBUG_TYPE "JStti"

void JSTTIImpl::getUnrollingPreferences(Loop *L,
                                            TTI::UnrollingPreferences &UP) {
  // We generally don't want a lot of unrolling.
  UP.Partial = false;
  UP.Runtime = false;
}

unsigned JSTTIImpl::getRegisterBitWidth(bool Vector) {
  if (Vector) {
    return 128;
  }

  return 32;
}

unsigned JSTTIImpl::getArithmeticInstrCost(
    unsigned Opcode, Type *Ty, TTI::OperandValueKind Opd1Info,
    TTI::OperandValueKind Opd2Info, TTI::OperandValueProperties Opd1PropInfo,
    TTI::OperandValueProperties Opd2PropInfo) {
  const unsigned Nope = 65536;

  unsigned Cost = BasicTTIImplBase<JSTTIImpl>::getArithmeticInstrCost(Opcode, Ty, Opd1Info, Opd2Info);

  if (VectorType *VTy = dyn_cast<VectorType>(Ty)) {
    switch (VTy->getNumElements()) {
    case 4:
      // SIMD.js supports Int32x4 and Float32x4, and we can emulate <4 x i1>.
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

unsigned JSTTIImpl::getVectorInstrCost(unsigned Opcode, Type *Val, unsigned Index) {
  unsigned Cost = BasicTTIImplBase::getVectorInstrCost(Opcode, Val, Index);

  // SIMD.js' insert/extract currently only take constant indices.
  if (Index == -1u)
      return Cost + 100;

  return Cost;
}

