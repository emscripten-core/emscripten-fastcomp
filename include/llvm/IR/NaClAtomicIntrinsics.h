//===-- llvm/IR/NaClAtomicIntrinsics.h - NaCl Atomic Intrinsics -*- C++ -*-===//
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

#ifndef LLVM_IR_NACL_ATOMIC_INTRINSICS_H
#define LLVM_IR_NACL_ATOMIC_INTRINSICS_H

#include "llvm/IR/Intrinsics.h"
#include "llvm/Support/Compiler.h"
#include <cstddef>

namespace llvm {

namespace NaCl {

static const size_t NumAtomicIntrinsics = 6;
static const size_t NumAtomicIntrinsicOverloadTypes = 4;
static const size_t MaxAtomicIntrinsicsParameters = 5;

/// Describe all the atomic intrinsics and their type signature. Most
/// can be overloaded on a type.
class AtomicIntrinsics {
public:
  enum ParamType {
    NoP, /// No parameter.
    Int, /// Overloaded.
    Ptr, /// Overloaded.
    RMW, /// Atomic RMW operation type.
    Mem  /// Memory order.
  };

  struct AtomicIntrinsic {
    Type *OverloadedType;
    Intrinsic::ID ID;
    uint8_t Overloaded : 1;
    uint8_t NumParams : 7;
    uint8_t ParamType[MaxAtomicIntrinsicsParameters];

    Function *getDeclaration(Module *M) const {
      // The atomic intrinsic can be overloaded on zero or one type,
      // which is needed to create the function's declaration.
      return Intrinsic::getDeclaration(
          M, ID, ArrayRef<Type *>(&OverloadedType, Overloaded ? 1 : 0));
    }
  };

  AtomicIntrinsics(LLVMContext &C);
  ~AtomicIntrinsics() {}

  typedef ArrayRef<AtomicIntrinsic> View;

  /// Access all atomic intrinsics, which can then be iterated over.
  View allIntrinsicsAndOverloads() const;
  /// Access a particular atomic intrinsic.
  /// \returns 0 if no intrinsic was found.
  const AtomicIntrinsic *find(Intrinsic::ID ID, Type *OverloadedType) const;

private:
  AtomicIntrinsic I[NumAtomicIntrinsics][NumAtomicIntrinsicOverloadTypes];

  AtomicIntrinsics() = delete;
  AtomicIntrinsics(const AtomicIntrinsics &) = delete;
  AtomicIntrinsics &operator=(const AtomicIntrinsics &) = delete;
};

/// Operations that can be represented by the @llvm.nacl.atomic.rmw
/// intrinsic.
///
/// Do not reorder these values: their order offers forward
/// compatibility of bitcode targeted to NaCl.
enum AtomicRMWOperation {
  AtomicInvalid = 0, // Invalid, keep first.
  AtomicAdd,
  AtomicSub,
  AtomicOr,
  AtomicAnd,
  AtomicXor,
  AtomicExchange,
  AtomicNum // Invalid, keep last.
};

/// Memory orderings supported by C11/C++11.
///
/// Do not reorder these values: their order offers forward
/// compatibility of bitcode targeted to NaCl.
enum MemoryOrder {
  MemoryOrderInvalid = 0, // Invalid, keep first.
  MemoryOrderRelaxed,
  MemoryOrderConsume,
  MemoryOrderAcquire,
  MemoryOrderRelease,
  MemoryOrderAcquireRelease,
  MemoryOrderSequentiallyConsistent,
  MemoryOrderNum // Invalid, keep last.
};

} // End NaCl namespace

} // End llvm namespace

#endif
