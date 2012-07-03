//===-- X86JITInfo.cpp - Implement the JIT interfaces for the X86 target --===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the JIT interfaces for the X86 target on Native Client
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "jit"
#include "X86NaClJITInfo.h"
#include "X86Relocations.h"
#include "X86Subtarget.h"
#include "X86TargetMachine.h"
#include <cstdlib>
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Disassembler.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Valgrind.h"
#ifdef __native_client__
#include <nacl/nacl_dyncode.h>
#endif

using namespace llvm;

extern cl::opt<int> FlagSfiX86JmpMask;

// Determine the platform we're running on
#if defined (__x86_64__) || defined (_M_AMD64) || defined (_M_X64)
# define X86_64_JIT
#elif defined(__i386__) || defined(i386) || defined(_M_IX86)
# define X86_32_JIT
#elif defined(__pnacl__)
#warning "PNaCl does not yet have JIT support"
#else
#error "Should not be building X86NaClJITInfo on non-x86"
// TODO(dschuff): make this work under pnacl self-build?
#endif

// Get the ASMPREFIX for the current host.  This is often '_'.
#ifndef __USER_LABEL_PREFIX__
#define __USER_LABEL_PREFIX__
#endif
#define GETASMPREFIX2(X) #X
#define GETASMPREFIX(X) GETASMPREFIX2(X)
#define ASMPREFIX GETASMPREFIX(__USER_LABEL_PREFIX__)

# define SIZE(sym) ".size " #sym ", . - " #sym "\n"
# define TYPE_FUNCTION(sym) ".type " #sym ", @function\n"

void X86NaClJITInfo::replaceMachineCodeForFunction(void *Old, void *New) {
  // We don't know the original instruction boundaries, so we replace the
  // whole bundle.
  uint8_t buf[kBundleSize];
  buf[0] = 0xE9;                // Emit JMP opcode.
  intptr_t OldAddr = ((uintptr_t)Old + 1);
  uint32_t NewOffset = (intptr_t)New - OldAddr - 4;// PC-relative offset of new
  *((uint32_t*)(buf + 1)) = NewOffset;
  memcpy(buf + 5, getNopSequence(kBundleSize - 5), kBundleSize - 5);

#ifdef __native_client__
  if(nacl_dyncode_create(Old, buf, kBundleSize)) {
    report_fatal_error("machine code replacement failed");
  }
#endif

  // X86 doesn't need to invalidate the processor cache, so just invalidate
  // Valgrind's cache directly.
  sys::ValgrindDiscardTranslations(Old, 5);
}

/// JITCompilerFunction - This contains the address of the JIT function used to
/// compile a function lazily.
static TargetJITInfo::JITCompilerFn JITCompilerFunction;

extern "C" {
#if defined(X86_64_JIT) || defined(__pnacl__) || !defined(__native_client__)
void X86NaClCompilationCallback(void) {
//TODO(dschuff): implement for X86-64
}
void X86NaClCompilationCallback_fastcc(void) {
//TODO(dschuff): implement for X86-64
}
#else
// Chrome system requirements include PIII, So SSE is present.
// For now this is the same as X86CompilationCallback_SSE
// In the future we could emit this rather than defining it with asm, for
// compatibility with pnacl self-build
// Also omit CFI junk (which is #defined away)

// The difference between the 2 wrapper variants is that the first returns
// through ecx and the 2nd returns through eax. The fastcc calling convention
// uses ecx to pass arguments, and the C calling convention uses eax to pass
// arguments with the 'inreg' attribute, so we make sure not to clobber it.
// Returning through eax for fastcc and ecx for C clobbers the 'nest' parameter
// breaking nested functions (which are not supported by clang in any case).

void X86NaClCompilationCallback(void);
asm(
    ".text\n"
    ".align 32\n"
    ".globl " ASMPREFIX "X86NaClCompilationCallback\n"
    TYPE_FUNCTION(X86NaClCompilationCallback)
    ASMPREFIX "X86NaClCompilationCallback:\n"
    "pushl %ebp\n"
    "movl    %esp, %ebp\n"    // Standard prologue
    "pushl   %eax\n"
    "pushl   %edx\n"          // Save EAX/EDX/ECX
    "pushl   %ecx\n"
    "andl    $-16, %esp\n"    // Align ESP on 16-byte boundary
    // Save all XMM arg registers
    "subl    $64, %esp\n"
    // FIXME: provide frame move information for xmm registers.
    // This can be tricky, because CFA register is ebp (unaligned)
    // and we need to produce offsets relative to it.
    "movaps  %xmm0, (%esp)\n"
    "movaps  %xmm1, 16(%esp)\n"
    "movaps  %xmm2, 32(%esp)\n"
    "movaps  %xmm3, 48(%esp)\n"
    "subl    $16, %esp\n"
    "movl    4(%ebp), %eax\n" // Pass prev frame and return address
    "movl    %eax, 4(%esp)\n"
    "movl    %ebp, (%esp)\n"
    "call    " ASMPREFIX "X86NaClCompilationCallback2\n"
    "addl    $16, %esp\n"
    "movaps  48(%esp), %xmm3\n"
    "movaps  32(%esp), %xmm2\n"
    "movaps  16(%esp), %xmm1\n"
    "movaps  (%esp), %xmm0\n"
    "movl    %ebp, %esp\n"    // Restore ESP
    "subl    $12, %esp\n"
    "popl    %ecx\n"
    "popl    %edx\n"
    "popl    %eax\n"
    "popl    %ebp\n"
    "popl %ecx\n"
    "nacljmp %ecx\n"
    SIZE(X86NaClCompilationCallback)
);



void X86NaClCompilationCallback_fastcc(void);
asm(
    ".text\n"
    ".align 32\n"
    ".globl " ASMPREFIX "X86NaClCompilationCallback_fastcc\n"
    TYPE_FUNCTION(X86NaClCompilationCallback_fastcc)
    ASMPREFIX "X86NaClCompilationCallback_fastcc:\n"
    "pushl %ebp\n"
    "movl    %esp, %ebp\n"    // Standard prologue
    "pushl   %eax\n"
    "pushl   %edx\n"          // Save EAX/EDX/ECX
    "pushl   %ecx\n"
    "andl    $-16, %esp\n"    // Align ESP on 16-byte boundary
    // Save all XMM arg registers
    "subl    $64, %esp\n"
    // FIXME: provide frame move information for xmm registers.
    // This can be tricky, because CFA register is ebp (unaligned)
    // and we need to produce offsets relative to it.
    "movaps  %xmm0, (%esp)\n"
    "movaps  %xmm1, 16(%esp)\n"
    "movaps  %xmm2, 32(%esp)\n"
    "movaps  %xmm3, 48(%esp)\n"
    "subl    $16, %esp\n"
    "movl    4(%ebp), %eax\n" // Pass prev frame and return address
    "movl    %eax, 4(%esp)\n"
    "movl    %ebp, (%esp)\n"
    "call    " ASMPREFIX "X86NaClCompilationCallback2\n"
    "addl    $16, %esp\n"
    "movaps  48(%esp), %xmm3\n"
    "movaps  32(%esp), %xmm2\n"
    "movaps  16(%esp), %xmm1\n"
    "movaps  (%esp), %xmm0\n"
    "movl    %ebp, %esp\n"    // Restore ESP
    "subl    $12, %esp\n"
    "popl    %ecx\n"
    "popl    %edx\n"
    "popl    %eax\n"
    "popl    %ebp\n"
    "popl %eax\n"
    "nacljmp %eax\n"
    SIZE(X86NaClCompilationCallback_fastcc)
);
#endif

/// X86CompilationCallback2 - This is the target-specific function invoked by the
/// function stub when we did not know the real target of a call.  This function
/// must locate the start of the stub or call site and pass it into the JIT
/// compiler function.

// A stub has the following format:
// | Jump opcode (1 byte) | Jump target +22 bytes | 3 bytes of NOPs
//   | 18 bytes of NOPs | 1 halt | Call opcode (1 byte) | call target
// The jump targets the call at the end of the bundle, which targets the
// compilation callback. Once the compilation callback JITed the target
// function it replaces the first 8 bytes of the stub in a single atomic
// operation, retargeting the jump at the JITed function.

static uint8_t *BundleRewriteBuffer;

static void LLVM_ATTRIBUTE_USED
X86NaClCompilationCallback2(intptr_t *StackPtr, intptr_t RetAddr) {
  // Get the return address from where the call instruction left it
  intptr_t *RetAddrLoc = &StackPtr[1];
  assert(*RetAddrLoc == RetAddr &&
         "Could not find return address on the stack!");

  // TODO: take a lock here. figure out whether it has to be the JIT lock or
  // can be our own lock (or however we handle thread safety)
#if 0
  DEBUG(dbgs() << "In callback! Addr=" << (void*)RetAddr
               << " ESP=" << (void*)StackPtr << "\n");
#endif

  intptr_t StubStart = RetAddr - 32;
  // This probably isn't necessary. I believe the corresponding code in
  // X86JITInfo is vestigial, and AFAICT no non-stub calls to the compilation
  // callback are generated anywhere. Still it doesn't hurt as a sanity check
  bool isStub = *((unsigned char*)StubStart) == 0xE9 &&
      *((int32_t*)(StubStart + 1)) == 22 &&
      *((unsigned char*)(StubStart + 26)) == 0xF4;

  assert(isStub && "NaCl doesn't support rewriting non-stub callsites yet");

  // Backtrack so RetAddr points inside the stub (so JITResolver can find
  // which function to compile)
  RetAddr -= 4;

  intptr_t NewVal = (intptr_t)JITCompilerFunction((void*)RetAddr);

  // Rewrite the stub's call target, so that we don't end up here every time we
  // execute the call.

  // Get the first 8 bytes of the stub
  memcpy(BundleRewriteBuffer, (void *)(StubStart), 8);
  // Point the jump at the newly-JITed code
  *((intptr_t *)(BundleRewriteBuffer + 1)) = NewVal - (StubStart + 5);

  // Copy the new code
#ifdef __native_client__
  if(nacl_dyncode_modify((void *)StubStart, BundleRewriteBuffer, 8)) {
    report_fatal_error("dyncode_modify failed");
  }
#endif
  // TODO: release the lock

  // Change our return address to execute the new jump
  *RetAddrLoc = StubStart;
}

}

const int X86NaClJITInfo::kBundleSize;

TargetJITInfo::LazyResolverFn
X86NaClJITInfo::getLazyResolverFunction(JITCompilerFn F) {
  JITCompilerFunction = F;
  return X86NaClCompilationCallback;
}

X86NaClJITInfo::X86NaClJITInfo(X86TargetMachine &tm) : X86JITInfo(tm) {
  // FIXME: does LLVM have some way of doing static initialization?
#ifndef __pnacl__
  if(posix_memalign((void **)&BundleRewriteBuffer, kBundleSize, kBundleSize))
    report_fatal_error("Could not allocate aligned memory");
#else
  BundleRewriteBuffer = NULL;
#endif

  NopString = new uint8_t[kBundleSize];
  for (int i = 0; i < kBundleSize; i++) NopString[i] = 0x90;
  X86Hlt.ins = new uint8_t[1];
  X86Hlt.ins[0] = 0xf4;
  X86Hlt.len = 1;
}

X86NaClJITInfo::~X86NaClJITInfo() {
  delete [] NopString;
  delete [] X86Hlt.ins;
}

TargetJITInfo::StubLayout X86NaClJITInfo::getStubLayout() {
  // NaCl stubs must be full bundles because calls still have to be aligned
  // even if they don't return
  StubLayout Result = {kBundleSize, kBundleSize};
  return Result;
}


void *X86NaClJITInfo::emitFunctionStub(const Function* F, void *Target,
                                       JITCodeEmitter &JCE) {
  bool TargetsCC = Target == (void *)(intptr_t)X86NaClCompilationCallback;

  // If we target the compilation callback, swap it for a different one for
  // functions using the fastcc calling convention
  if(TargetsCC && F->getCallingConv() == CallingConv::Fast) {
    Target = (void *)(intptr_t)X86NaClCompilationCallback_fastcc;
  }

  void *Result = (void *)JCE.getCurrentPCValue();
  assert(RoundUpToAlignment((uintptr_t)Result, kBundleSize) == (uintptr_t)Result
         && "Unaligned function stub");
  if (!TargetsCC) {
    // Jump to the target
    JCE.emitByte(0xE9);
    JCE.emitWordLE((intptr_t)Target - JCE.getCurrentPCValue() - 4);
    // Fill with Nops.
    emitNopPadding(JCE, 27);
  } else {
    // Jump over 22 bytes
    JCE.emitByte(0xE9);
    JCE.emitWordLE(22);
    // emit 3-bytes of nop to ensure an instruction boundary at 8 bytes
    emitNopPadding(JCE, 3);
    // emit 18 bytes of nop
    emitNopPadding(JCE, 18);
    // emit 1 byte of halt. This helps CompilationCallback tell whether
    // we came from a stub or not
    JCE.emitByte(X86Hlt.ins[0]);
    // emit a call to the compilation callback
    JCE.emitByte(0xE8);
    JCE.emitWordLE((intptr_t)Target - JCE.getCurrentPCValue() - 4);
  }
  return Result;
}

// Relocations are the same as in X86, but the address being written
// not the same as the address that the offset is relative to (see comment on
// setRelocationBuffer in X86NaClJITInfo.h
void X86NaClJITInfo::relocate(void *Function, MachineRelocation *MR,
                    unsigned NumRelocs, unsigned char* GOTBase) {
  for (unsigned i = 0; i != NumRelocs; ++i, ++MR) {
    void *RelocPos = RelocationBuffer + MR->getMachineCodeOffset();
    void *RelocTargetPos = (char*)Function + MR->getMachineCodeOffset();
    intptr_t ResultPtr = (intptr_t)MR->getResultPointer();
    switch ((X86::RelocationType)MR->getRelocationType()) {
    case X86::reloc_pcrel_word: {
      // PC relative relocation, add the relocated value to the value already in
      // memory, after we adjust it for where the PC is.
      ResultPtr = ResultPtr -(intptr_t)RelocTargetPos - 4 - MR->getConstantVal();
      *((unsigned*)RelocPos) += (unsigned)ResultPtr;
      break;
    }
    case X86::reloc_picrel_word: {
      // PIC base relative relocation, add the relocated value to the value
      // already in memory, after we adjust it for where the PIC base is.
      ResultPtr = ResultPtr - ((intptr_t)Function + MR->getConstantVal());
      *((unsigned*)RelocPos) += (unsigned)ResultPtr;
      break;
    }
    case X86::reloc_absolute_word:
    case X86::reloc_absolute_word_sext:
      // Absolute relocation, just add the relocated value to the value already
      // in memory.
      *((unsigned*)RelocPos) += (unsigned)ResultPtr;
      break;
    case X86::reloc_absolute_dword:
      *((intptr_t*)RelocPos) += ResultPtr;
      break;
    }
  }
}

const uint8_t *X86NaClJITInfo::getNopSequence(size_t len) const {
  // TODO(dschuff): use more efficient NOPs.
  // Update emitNopPadding when it happens
  assert((int)len <= kBundleSize &&
         "Nop sequence can't be more than bundle size");
  return NopString;
}

void X86NaClJITInfo::emitNopPadding(JITCodeEmitter &JCE, size_t len) {
  for (size_t i = 0; i < len; i++) JCE.emitByte(NopString[i]);
}

const TargetJITInfo::HaltInstruction *X86NaClJITInfo::getHalt() const {
  return &X86Hlt;
}

int X86NaClJITInfo::getBundleSize() const {
  return kBundleSize;
}

int32_t X86NaClJITInfo::getJumpMask() const {
  return FlagSfiX86JmpMask;
}
