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
#include "llvm/Support/raw_ostream.h"
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

unsigned JSTTIImpl::getNumberOfRegisters(bool Vector) {
  if (Vector) return 16; // like NEON, x86_64, etc.

  return 8; // like x86, thumb, etc.
}

unsigned JSTTIImpl::getRegisterBitWidth(bool Vector) {
  if (Vector) {
    return 128;
  }

  return 32;
}

static const unsigned Nope = 65536;

// Certain types are fine, but some vector types must be avoided at all Costs.
static bool isOkType(Type *Ty) {
  if (VectorType *VTy = dyn_cast<VectorType>(Ty)) {
    if (VTy->getNumElements() != 4 || !(VTy->getElementType()->isIntegerTy(1) ||
                                        VTy->getElementType()->isIntegerTy(32) ||
                                        VTy->getElementType()->isFloatTy())) {
      return false;
    }
  }
  return true;
}

unsigned JSTTIImpl::getArithmeticInstrCost(
    unsigned Opcode, Type *Ty, TTI::OperandValueKind Opd1Info,
    TTI::OperandValueKind Opd2Info, TTI::OperandValueProperties Opd1PropInfo,
    TTI::OperandValueProperties Opd2PropInfo) {

  unsigned Cost = BasicTTIImplBase<JSTTIImpl>::getArithmeticInstrCost(Opcode, Ty, Opd1Info, Opd2Info);

  if (!isOkType(Ty))
    return Nope;

  if (VectorType *VTy = dyn_cast<VectorType>(Ty)) {
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
  if (!isOkType(Val))
    return Nope;

  unsigned Cost = BasicTTIImplBase::getVectorInstrCost(Opcode, Val, Index);

  // SIMD.js' insert/extract currently only take constant indices.
  if (Index == -1u)
    return Cost + 100;

  return Cost;
}


unsigned JSTTIImpl::getMemoryOpCost(unsigned Opcode, Type *Src, unsigned Alignment,
                                    unsigned AddressSpace) {
  if (!isOkType(Src))
    return Nope;

  return BasicTTIImplBase::getMemoryOpCost(Opcode, Src, Alignment, AddressSpace);
}

unsigned JSTTIImpl::getCastInstrCost(unsigned Opcode, Type *Dst, Type *Src) {
  if (!isOkType(Src) || !isOkType(Dst))
    return Nope;

  return BasicTTIImplBase::getCastInstrCost(Opcode, Dst, Src);
}

