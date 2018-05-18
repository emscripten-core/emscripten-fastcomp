//==- JSTargetTransformInfo.h - JS-specific TTI -*- C++ -*-=//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief This file a TargetTransformInfo::Concept conforming object specific
/// to the JS target machine.
///
/// It uses the target's detailed information to provide more precise answers to
/// certain TTI queries, while letting the target independent and default TTI
/// implementations handle the rest.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_JS_JSTARGETTRANSFORMINFO_H
#define LLVM_LIB_TARGET_JS_JSTARGETTRANSFORMINFO_H

#include "JSTargetMachine.h"
#include "llvm/CodeGen/BasicTTIImpl.h"
#include <algorithm>

namespace llvm {

class JSTTIImpl final : public BasicTTIImplBase<JSTTIImpl> {
  typedef BasicTTIImplBase<JSTTIImpl> BaseT;
  typedef TargetTransformInfo TTI;
  friend BaseT;

  const JSSubtarget *ST;
  const JSTargetLowering *TLI;

  const JSSubtarget *getST() const { return ST; }
  const JSTargetLowering *getTLI() const { return TLI; }

public:
  JSTTIImpl(const JSTargetMachine *TM, const Function &F)
      : BaseT(TM, F.getParent()->getDataLayout()), ST(TM->getSubtargetImpl(F)),
        TLI(ST->getTargetLowering()) {}

  /// \name Scalar TTI Implementations
  /// @{

  // TODO: Implement more Scalar TTI for JS

  TTI::PopcntSupportKind getPopcntSupport(unsigned TyWidth) const;

  /// @}

  /// \name Vector TTI Implementations
  /// @{

  unsigned getNumberOfRegisters(bool Vector);
  unsigned getRegisterBitWidth(bool Vector) const;
  unsigned getArithmeticInstrCost(
      unsigned Opcode, Type *Ty,
      TTI::OperandValueKind Opd1Info = TTI::OK_AnyValue,
      TTI::OperandValueKind Opd2Info = TTI::OK_AnyValue,
      TTI::OperandValueProperties Opd1PropInfo = TTI::OP_None,
      TTI::OperandValueProperties Opd2PropInfo = TTI::OP_None,
      ArrayRef<const Value *> Args = ArrayRef<const Value *>());
  unsigned getVectorInstrCost(unsigned Opcode, Type *Val, unsigned Index);

  /// @}
};

} // end namespace llvm

#endif
