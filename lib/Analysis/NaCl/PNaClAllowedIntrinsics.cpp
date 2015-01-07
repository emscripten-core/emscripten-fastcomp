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

PNaClAllowedIntrinsics::
PNaClAllowedIntrinsics(LLVMContext *Context) : Context(Context) {
  Type *I8Ptr = Type::getInt8PtrTy(*Context);
  Type *I8 = Type::getInt8Ty(*Context);
  Type *I16 = Type::getInt16Ty(*Context);
  Type *I32 = Type::getInt32Ty(*Context);
  Type *I64 = Type::getInt64Ty(*Context);
  Type *Float = Type::getFloatTy(*Context);
  Type *Double = Type::getDoubleTy(*Context);

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
  return isAllowedIntrinsicID(Func->getIntrinsicID());
}

bool PNaClAllowedIntrinsics::isAllowedIntrinsicID(unsigned ID) {
  // (1) Allowed always, provided the exact name and type match.
  // (2) Never allowed.
  // (3) Debug info intrinsics.
  //
  // Please keep these sorted or grouped in a sensible way, within
  // each category.
  switch (ID) {
    // Disallow by default.
    default: return false;

    /* The following is intentionally commented out, since the default
       will return false.
    // (2) Known to be never allowed.
    case Intrinsic::not_intrinsic:
    // Trampolines depend on a target-specific-sized/aligned buffer.
    case Intrinsic::adjust_trampoline:
    case Intrinsic::init_trampoline:
    // CXX exception handling is not stable.
    case Intrinsic::eh_dwarf_cfa:
    case Intrinsic::eh_return_i32:
    case Intrinsic::eh_return_i64:
    case Intrinsic::eh_sjlj_callsite:
    case Intrinsic::eh_sjlj_functioncontext:
    case Intrinsic::eh_sjlj_longjmp:
    case Intrinsic::eh_sjlj_lsda:
    case Intrinsic::eh_sjlj_setjmp:
    case Intrinsic::eh_typeid_for:
    case Intrinsic::eh_unwind_init:
    // We do not want to expose addresses to the user.
    case Intrinsic::frameaddress:
    case Intrinsic::returnaddress:
    // Not supporting stack protectors.
    case Intrinsic::stackprotector:
    // Var-args handling is done w/out intrinsics.
    case Intrinsic::vacopy:
    case Intrinsic::vaend:
    case Intrinsic::vastart:
    // Disallow the *_with_overflow intrinsics because they return
    // struct types.  All of them can be introduced by passing -ftrapv
    // to Clang, which we do not support for now.  umul_with_overflow
    // and uadd_with_overflow are introduced by Clang for C++'s new[],
    // but ExpandArithWithOverflow expands out this use.
    case Intrinsic::sadd_with_overflow:
    case Intrinsic::ssub_with_overflow:
    case Intrinsic::uadd_with_overflow:
    case Intrinsic::usub_with_overflow:
    case Intrinsic::smul_with_overflow:
    case Intrinsic::umul_with_overflow:
    // Disallow lifetime.start/end because the semantics of what
    // arguments they accept are not very well defined, and because it
    // would be better to do merging of stack slots in the user
    // toolchain than in the PNaCl translator.
    // See https://code.google.com/p/nativeclient/issues/detail?id=3443
    case Intrinsic::lifetime_end:
    case Intrinsic::lifetime_start:
    case Intrinsic::invariant_end:
    case Intrinsic::invariant_start:
    // Some transcendental functions not needed yet.
    case Intrinsic::cos:
    case Intrinsic::exp:
    case Intrinsic::exp2:
    case Intrinsic::log:
    case Intrinsic::log2:
    case Intrinsic::log10:
    case Intrinsic::pow:
    case Intrinsic::powi:
    case Intrinsic::sin:
    // We run -lower-expect to convert Intrinsic::expect into branch weights
    // and consume in the middle-end. The backend just ignores llvm.expect.
    case Intrinsic::expect:
    // For FLT_ROUNDS macro from float.h. It works for ARM and X86
    // (but not MIPS). Also, wait until we add a set_flt_rounds intrinsic
    // before we bless this.
    case Intrinsic::flt_rounds:
      return false;
    */

    // (3) Debug info intrinsics.
    case Intrinsic::dbg_declare:
    case Intrinsic::dbg_value:
      return PNaClABIAllowDebugMetadata;
  }
}
