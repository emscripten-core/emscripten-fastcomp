//===- PNaClAllowedIntrinsics.cpp - Set of allowed intrinsics -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Defines implementation of  class that holds set of allowed intrinsics.
//
// Keep 3 categories of intrinsics for now.
// (1) Allowed always, provided the exact name and type match.
// (2) Never allowed.
// (3) Debug info intrinsics.
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/NaCl/PNaClAllowedIntrinsics.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/Analysis/NaCl.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Type.h"

using namespace llvm;

/*
  The constructor sets up the whitelist of allowed intrinsics and their expected
  types.  The comments in that code have some details on the allowed intrinsics.
  Additionally, the following intrinsics are disallowed for the stated reasons:

  * Trampolines depend on a target-specific-sized/aligned buffer.
    Intrinsic::adjust_trampoline:
    Intrinsic::init_trampoline:
  * CXX exception handling is not stable.
    Intrinsic::eh_dwarf_cfa:
    Intrinsic::eh_return_i32:
    Intrinsic::eh_return_i64:
    Intrinsic::eh_sjlj_callsite:
    Intrinsic::eh_sjlj_functioncontext:
    Intrinsic::eh_sjlj_longjmp:
    Intrinsic::eh_sjlj_lsda:
    Intrinsic::eh_sjlj_setjmp:
    Intrinsic::eh_typeid_for:
    Intrinsic::eh_unwind_init:
  * We do not want to expose addresses to the user.
    Intrinsic::frameaddress:
    Intrinsic::returnaddress:
  * We do not support stack protectors.
    Intrinsic::stackprotector:
  * Var-args handling is done w/out intrinsics.
    Intrinsic::vacopy:
    Intrinsic::vaend:
    Intrinsic::vastart:
  * Disallow the *_with_overflow intrinsics because they return
    struct types.  All of them can be introduced by passing -ftrapv
    to Clang, which we do not support for now.  umul_with_overflow
    and uadd_with_overflow are introduced by Clang for C++'s new[],
    but ExpandArithWithOverflow expands out this use.
    Intrinsic::sadd_with_overflow:
    Intrinsic::ssub_with_overflow:
    Intrinsic::uadd_with_overflow:
    Intrinsic::usub_with_overflow:
    Intrinsic::smul_with_overflow:
    Intrinsic::umul_with_overflow:
  * Disallow lifetime.start/end because the semantics of what
    arguments they accept are not very well defined, and because it
    would be better to do merging of stack slots in the user
    toolchain than in the PNaCl translator.
    See https://code.google.com/p/nativeclient/issues/detail?id=3443
    Intrinsic::lifetime_end:
    Intrinsic::lifetime_start:
    Intrinsic::invariant_end:
    Intrinsic::invariant_start:
  * Some transcendental functions not needed yet.
    Intrinsic::cos:
    Intrinsic::exp:
    Intrinsic::exp2:
    Intrinsic::log:
    Intrinsic::log2:
    Intrinsic::log10:
    Intrinsic::pow:
    Intrinsic::powi:
    Intrinsic::sin:
  * We run -lower-expect to convert Intrinsic::expect into branch weights
    and consume in the middle-end. The backend just ignores llvm.expect.
    Intrinsic::expect:
  * For FLT_ROUNDS macro from float.h. It works for ARM and X86
    (but not MIPS). Also, wait until we add a set_flt_rounds intrinsic
    before we bless this.
    case Intrinsic::flt_rounds:
*/
PNaClAllowedIntrinsics::
PNaClAllowedIntrinsics(LLVMContext *Context) : Context(Context) {
  Type *I8Ptr = Type::getInt8PtrTy(*Context);
  Type *I8 = Type::getInt8Ty(*Context);
  Type *I16 = Type::getInt16Ty(*Context);
  Type *I32 = Type::getInt32Ty(*Context);
  Type *I64 = Type::getInt64Ty(*Context);
  Type *Float = Type::getFloatTy(*Context);
  Type *Double = Type::getDoubleTy(*Context);
  Type *Vec4Float = VectorType::get(Float, 4);

  // We accept bswap for a limited set of types (i16, i32, i64).  The
  // various backends are able to generate instructions to implement
  // the intrinsic.  Also, i16 and i64 are easy to implement as along
  // as there is a way to do i32.
  addIntrinsic(Intrinsic::bswap, I16);
  addIntrinsic(Intrinsic::bswap, I32);
  addIntrinsic(Intrinsic::bswap, I64);

  // We accept cttz, ctlz, and ctpop for a limited set of types (i32, i64).
  addIntrinsic(Intrinsic::ctlz, I32);
  addIntrinsic(Intrinsic::ctlz, I64);
  addIntrinsic(Intrinsic::cttz, I32);
  addIntrinsic(Intrinsic::cttz, I64);
  addIntrinsic(Intrinsic::ctpop, I32);
  addIntrinsic(Intrinsic::ctpop, I64);

  addIntrinsic(Intrinsic::nacl_read_tp);
  addIntrinsic(Intrinsic::nacl_longjmp);
  addIntrinsic(Intrinsic::nacl_setjmp);

  addIntrinsic(Intrinsic::fabs, Float);
  addIntrinsic(Intrinsic::fabs, Double);
  addIntrinsic(Intrinsic::fabs, Vec4Float);

  // For native sqrt instructions. Must guarantee when x < -0.0, sqrt(x) = NaN.
  addIntrinsic(Intrinsic::sqrt, Float);
  addIntrinsic(Intrinsic::sqrt, Double);

  Type *AtomicTypes[] = { I8, I16, I32, I64 };
  for (size_t T = 0, E = array_lengthof(AtomicTypes); T != E; ++T) {
    addIntrinsic(Intrinsic::nacl_atomic_load, AtomicTypes[T]);
    addIntrinsic(Intrinsic::nacl_atomic_store, AtomicTypes[T]);
    addIntrinsic(Intrinsic::nacl_atomic_rmw, AtomicTypes[T]);
    addIntrinsic(Intrinsic::nacl_atomic_cmpxchg, AtomicTypes[T]);
  }
  addIntrinsic(Intrinsic::nacl_atomic_fence);
  addIntrinsic(Intrinsic::nacl_atomic_fence_all);

  addIntrinsic(Intrinsic::nacl_atomic_is_lock_free);

  // Stack save and restore are used to support C99 VLAs.
  addIntrinsic(Intrinsic::stacksave);
  addIntrinsic(Intrinsic::stackrestore);

  addIntrinsic(Intrinsic::trap);

  // We only allow the variants of memcpy/memmove/memset with an i32
  // "len" argument, not an i64 argument.
  Type *MemcpyTypes[] = { I8Ptr, I8Ptr, I32 };
  addIntrinsic(Intrinsic::memcpy, MemcpyTypes);
  addIntrinsic(Intrinsic::memmove, MemcpyTypes);
  Type *MemsetTypes[] = { I8Ptr, I32 };
  addIntrinsic(Intrinsic::memset, MemsetTypes);
}

void PNaClAllowedIntrinsics::addIntrinsic(Intrinsic::ID ID,
                                          ArrayRef<Type *> Tys) {
  std::string Name = Intrinsic::getName(ID, Tys);
  FunctionType *FcnType = Intrinsic::getType(*Context, ID, Tys);
  if (TypeMap.count(Name) >= 1) {
    std::string Buffer;
    raw_string_ostream StrBuf(Buffer);
    StrBuf << "Instrinsic " << Name << " defined with multiple types: "
           << *TypeMap[Name] << " and " << *FcnType << "\n";
    report_fatal_error(StrBuf.str());
  }
  TypeMap[Name] = FcnType;
}

bool PNaClAllowedIntrinsics::isAllowed(const Function *Func) {
  if (isIntrinsicName(Func->getName()))
    return Func->getFunctionType() == TypeMap[Func->getName()];
  // Check to see if debugging intrinsic, which can be allowed if
  // command-line flag set.
  return isAllowedDebugInfoIntrinsic(Func->getIntrinsicID());
}

bool PNaClAllowedIntrinsics::isAllowedDebugInfoIntrinsic(unsigned IntrinsicID) {
  /* These intrinsics are allowed when debug info metadata is also allowed,
     and we just assume that they are called correctly by the frontend. */
  switch (IntrinsicID) {
    default: return false;
    case Intrinsic::dbg_declare:
    case Intrinsic::dbg_value:
      return PNaClABIAllowDebugMetadata;
  }
}
