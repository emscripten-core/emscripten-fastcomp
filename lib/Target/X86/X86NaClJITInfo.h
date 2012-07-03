//=- X86NaClJITInfo.h - X86 implementation of the JIT interface  --*- C++ -*-=//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the X86 implementation of the TargetJITInfo class for
// Native Client
//
//===----------------------------------------------------------------------===//

#ifndef X86NACLJITINFO_H
#define X86NACLJITINFO_H

#include "X86JITInfo.h"
#include "llvm/Function.h"
#include "llvm/CodeGen/JITCodeEmitter.h"
#include "llvm/Target/TargetJITInfo.h"

namespace llvm {
  class X86NaClJITInfo : public X86JITInfo {
    void emitNopPadding(JITCodeEmitter &JCE, size_t len);
    const X86Subtarget *Subtarget;
    uintptr_t PICBase;
    uint8_t *NopString;
    HaltInstruction X86Hlt;
    uint8_t *RelocationBuffer;
   public:
    static const int kBundleSize = 32;
    explicit X86NaClJITInfo(X86TargetMachine &tm);
    virtual ~X86NaClJITInfo();

    virtual void replaceMachineCodeForFunction(void *Old, void *New);

    // getStubLayout - Returns the size and alignment of the largest call stub
    // on X86 NaCl.
    virtual StubLayout getStubLayout();

    // Note: the emission and functions MUST NOT touch the target memory
    virtual void *emitFunctionStub(const Function* F, void *Target,
                                   JITCodeEmitter &JCE);
    /// getLazyResolverFunction - Expose the lazy resolver to the JIT.
    virtual LazyResolverFn getLazyResolverFunction(JITCompilerFn);
    /// relocate - Before the JIT can run a block of code that has been emitted,
    /// it must rewrite the code to contain the actual addresses of any
    /// referenced global symbols.
    virtual void relocate(void *Function, MachineRelocation *MR,
                        unsigned NumRelocs, unsigned char* GOTBase);

    virtual char* allocateThreadLocalMemory(size_t size) {
      //TODO(dschuff) Implement TLS or decide whether X86 TLS works
      assert(0 && "This target does not implement thread local storage!");
      return 0;
    }
    /// Return a string containing a sequence of NOPs which is valid for
    /// the given length
    virtual const uint8_t *getNopSequence(size_t len) const;
    virtual const HaltInstruction *getHalt() const;
    virtual int getBundleSize() const;
    virtual int getJumpMask() const;
    /// Relocations cannot happen in-place in NaCl because we can't write to
    /// code. This function takes a pointer to where the code has been emitted,
    /// before it is copied to the code region. The subsequent call to
    /// relocate takes pointers to the target code location, but rewrites the
    /// code in the relocation buffer rather than at the target
    virtual void setRelocationBuffer(unsigned char * BufferBegin) {
      RelocationBuffer = BufferBegin;
    }
  };
}

#endif
