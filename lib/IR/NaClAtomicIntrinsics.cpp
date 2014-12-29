//=== llvm/IR/NaClAtomicIntrinsics.cpp - NaCl Atomic Intrinsics -*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file describes atomic intrinsic functions that are specific to NaCl.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/NaClAtomicIntrinsics.h"
#include "llvm/IR/Type.h"

namespace llvm {

namespace NaCl {

AtomicIntrinsics::AtomicIntrinsics(LLVMContext &C) {
  Type *IT[NumAtomicIntrinsicOverloadTypes] = { Type::getInt8Ty(C),
                                                Type::getInt16Ty(C),
                                                Type::getInt32Ty(C),
                                                Type::getInt64Ty(C) };
  size_t CurIntrin = 0;

  // Initialize each of the atomic intrinsics and their overloads. They
  // have up to 5 parameters, the following macro will take care of
  // overloading.
#define INIT(P0, P1, P2, P3, P4, INTRIN)                                       \
  do {                                                                         \
    for (size_t CurType = 0; CurType != NumAtomicIntrinsicOverloadTypes;       \
         ++CurType) {                                                          \
      size_t Param = 0;                                                        \
      I[CurIntrin][CurType].OverloadedType = IT[CurType];                      \
      I[CurIntrin][CurType].ID = Intrinsic::nacl_atomic_##INTRIN;              \
      I[CurIntrin][CurType].Overloaded =                                       \
          P0 == Int || P0 == Ptr || P1 == Int || P1 == Ptr || P2 == Int ||     \
          P2 == Ptr || P3 == Int || P3 == Ptr || P4 == Int || P4 == Ptr;       \
      I[CurIntrin][CurType].NumParams =                                        \
          (P0 != NoP) + (P1 != NoP) + (P2 != NoP) + (P3 != NoP) + (P4 != NoP); \
      I[CurIntrin][CurType].ParamType[Param++] = P0;                           \
      I[CurIntrin][CurType].ParamType[Param++] = P1;                           \
      I[CurIntrin][CurType].ParamType[Param++] = P2;                           \
      I[CurIntrin][CurType].ParamType[Param++] = P3;                           \
      I[CurIntrin][CurType].ParamType[Param++] = P4;                           \
    }                                                                          \
    ++CurIntrin;                                                               \
  } while (0)

  INIT(Ptr, Mem, NoP, NoP, NoP, load);
  INIT(Ptr, Int, Mem, NoP, NoP, store);
  INIT(RMW, Ptr, Int, Mem, NoP, rmw);
  INIT(Ptr, Int, Int, Mem, Mem, cmpxchg);
  INIT(Mem, NoP, NoP, NoP, NoP, fence);
  INIT(NoP, NoP, NoP, NoP, NoP, fence_all);
}

AtomicIntrinsics::View AtomicIntrinsics::allIntrinsicsAndOverloads() const {
  return View(&I[0][0], NumAtomicIntrinsics * NumAtomicIntrinsicOverloadTypes);
}

const AtomicIntrinsics::AtomicIntrinsic *
AtomicIntrinsics::find(Intrinsic::ID ID, Type *OverloadedType) const {
  View R = allIntrinsicsAndOverloads();
  for (const AtomicIntrinsic *AI = R.begin(), *E = R.end(); AI != E; ++AI)
    if (AI->ID == ID && AI->OverloadedType == OverloadedType)
      return AI;
  return 0;
}

} // End NaCl namespace

} // End llvm namespace
