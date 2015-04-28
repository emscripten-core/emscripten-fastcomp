//===-- JSTargetTransformInfo.h - JS specific TTI ---------------*- C++ -*-===//
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

#ifndef LLVM_LIB_TARGET_JSBACKEND_JSTARGETTRANSFORMINFO_H
#define LLVM_LIB_TARGET_JSBACKEND_JSTARGETTRANSFORMINFO_H

#include "JS.h"
#include "JSTargetMachine.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/BasicTTIImpl.h"

namespace llvm {

class JSTTI : public BasicTTIImplBase<JSTTI> {
  typedef BasicTTIImplBase<JSTTI> BaseT;
  typedef TargetTransformInfo TTI;
  friend BaseT;

  const TargetSubtargetInfo *ST;
  const TargetLoweringBase *TLI;

  const TargetSubtargetInfo *getST() const { return ST; }
  const TargetLoweringBase *getTLI() const { return TLI; }

public:
explicit JSTTI(const JSTargetMachine *TM, Function &F)
    : BaseT(TM), ST(TM->getSubtargetImpl(F)), TLI(ST->getTargetLowering()) {}
  // Provide value semantics. MSVC requires that we spell all of these out.
  JSTTI(const JSTTI &Arg)
      : BaseT(static_cast<const BaseT &>(Arg)), ST(Arg.ST), TLI(Arg.TLI) {}
  JSTTI(JSTTI &&Arg)
      : BaseT(std::move(static_cast<BaseT &>(Arg))), ST(std::move(Arg.ST)),
        TLI(std::move(Arg.TLI)) {}
  JSTTI &operator=(const JSTTI &RHS) {
    BaseT::operator=(static_cast<const BaseT &>(RHS));
    ST = RHS.ST;
    TLI = RHS.TLI;
    return *this;
  }
  JSTTI &operator=(JSTTI &&RHS) {
    BaseT::operator=(std::move(static_cast<BaseT &>(RHS)));
    ST = std::move(RHS.ST);
    TLI = std::move(RHS.TLI);
    return *this;
  }


  TTI::PopcntSupportKind getPopcntSupport(unsigned IntTyWidthInBit);

  unsigned getRegisterBitWidth(bool Vector) const;

  unsigned getArithmeticInstrCost(
      unsigned Opcode, Type *Ty,
      TTI::OperandValueKind Opd1Info = TTI::OK_AnyValue,
      TTI::OperandValueKind Opd2Info = TTI::OK_AnyValue,
      TTI::OperandValueProperties Opd1PropInfo = TTI::OP_None,
      TTI::OperandValueProperties Opd2PropInfo = TTI::OP_None);

  unsigned getVectorInstrCost(unsigned Opcode, Type *Val,
                              unsigned Index = -1);

  void getUnrollingPreferences(Loop *L,
                               TTI::UnrollingPreferences &UP) const;
};

} // end namespace llvm
#endif
