//===-- JSTargetTransformInfo.h - JS specific TTI -------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
/// \file
/// This file a TargetTransformInfo::Concept conforming object specific to the
/// JS target machine. It uses the target's detailed information to
/// provide more precise answers to certain TTI queries, while letting the
/// target independent and default TTI implementations handle the rest.
///
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_JS_JSTARGETTRANSFORMINFO_H
#define LLVM_LIB_TARGET_JS_JSTARGETTRANSFORMINFO_H

#include "JSTargetMachine.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/CodeGen/BasicTTIImpl.h"
#include "llvm/Target/TargetLowering.h"

namespace llvm {

class JSTTIImpl : public BasicTTIImplBase<JSTTIImpl> {
  typedef BasicTTIImplBase<JSTTIImpl> BaseT;
  typedef TargetTransformInfo TTI;
  friend BaseT;

  const TargetSubtargetInfo *ST;
  const TargetLoweringBase *TLI;

  const TargetSubtargetInfo *getST() const { return ST; }
  const TargetLoweringBase *getTLI() const { return TLI; }

public:
  explicit JSTTIImpl(const JSTargetMachine *TM, const Function &F)
      : BaseT(TM, F.getParent()->getDataLayout()), ST(TM->getSubtargetImpl(F)),
        TLI(ST->getTargetLowering()) {}

  // Provide value semantics. MSVC requires that we spell all of these out.
  JSTTIImpl(const JSTTIImpl &Arg)
      : BaseT(static_cast<const BaseT &>(Arg)), ST(Arg.ST), TLI(Arg.TLI) {}
  JSTTIImpl(JSTTIImpl &&Arg)
      : BaseT(std::move(static_cast<BaseT &>(Arg))), ST(std::move(Arg.ST)),
        TLI(std::move(Arg.TLI)) {}
/*
  JSTTIImpl &operator=(const JSTTIImpl &RHS) {
    BaseT::operator=(static_cast<const BaseT &>(RHS));
    ST = RHS.ST;
    TLI = RHS.TLI;
    return *this;
  }
  JSTTIImpl &operator=(JSTTIImpl &&RHS) {
    BaseT::operator=(std::move(static_cast<BaseT &>(RHS)));
    ST = std::move(RHS.ST);
    TLI = std::move(RHS.TLI);
    return *this;
  }
*/

  bool hasBranchDivergence() { return true; }

  void getUnrollingPreferences(Loop *L, TTI::UnrollingPreferences &UP);

  TTI::PopcntSupportKind getPopcntSupport(
      unsigned TyWidth) {
    assert(isPowerOf2_32(TyWidth) && "Ty width must be power of 2");
    // Hopefully we'll get popcnt in ES7, but for now, we just have software.
    return TargetTransformInfo::PSK_Software;
  }

  unsigned getNumberOfRegisters(bool Vector);

  unsigned getRegisterBitWidth(bool Vector);

  unsigned getArithmeticInstrCost(
      unsigned Opcode, Type *Ty,
      TTI::OperandValueKind Opd1Info = TTI::OK_AnyValue,
      TTI::OperandValueKind Opd2Info = TTI::OK_AnyValue,
      TTI::OperandValueProperties Opd1PropInfo = TTI::OP_None,
      TTI::OperandValueProperties Opd2PropInfo = TTI::OP_None);

  unsigned getVectorInstrCost(unsigned Opcode, Type *Val, unsigned Index);

  unsigned getMemoryOpCost(unsigned Opcode, Type *Src, unsigned Alignment,
                           unsigned AddressSpace);

  unsigned getCastInstrCost(unsigned Opcode, Type *Dst, Type *Src);
};

} // end namespace llvm

#endif
