//====- X86InstrNaCl.td - Describe NaCl Instructions ----*- tablegen -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file describes the modifications to the X86 instruction set needed for
// Native Client code generation.
//
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// NaCl specific DAG Nodes.
//

//===----------------------------------------------------------------------===//
//
//                       Native Client Pseudo-Instructions
//
// These instructions implement the Native Client pseudo-instructions, such
// as nacljmp and naclasp.
//
// TableGen and MC consider these to be "real" instructions. They can be
// parsed by the AsmParser and emitted by the AsmStreamer as if they
// were just regular instructions. They are not marked "Pseudo" because
// this would imply isCodeGenOnly=1, which would stop them from being
// parsed by the assembler.
//
// These instructions cannot be encoded (written into an object file) by the
// MCCodeEmitter. Instead, during direct object emission, they get lowered to
// a sequence of streamer emits. (see X86InstrNaCl.cpp)
//
// These instructions should not be used in CodeGen. They have no pattern
// and lack CodeGen metadata. Instead, the X86NaClRewritePass should
// generate these instructions after CodeGen is finished.
//
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
// 32-bit Native Client Pseudo Instructions
//===----------------------------------------------------------------------===//

class NaClPI32<dag outs, dag ins, string asm>
  : I<0, CustomFrm, outs, ins, asm, []>, Requires<[IsNaCl, In32BitMode]>;

let isTerminator = 1, isBarrier = 1, hasCtrlDep = 1, isAsmParserOnly = 1 in {
  def NACL_TRAP32  : NaClPI32<(outs), (ins), "nacltrap">;
}

let isTerminator = 1, isReturn = 1, isBarrier = 1,
    hasCtrlDep = 1, FPForm = SpecialFP, isAsmParserOnly = 1 in {
  def NACL_RET32  : NaClPI32<(outs), (ins), "naclret">;
  def NACL_RETI32 : NaClPI32<(outs), (ins i16imm:$amt), "naclreti\t$amt">;
}

let isBranch = 1, isTerminator = 1, isBarrier = 1, isIndirectBranch = 1,
    isAsmParserOnly = 1 in {
  def NACL_JMP32r : NaClPI32<(outs), (ins GR32:$dst), "nacljmp\t$dst">;
}

let isCall = 1, isAsmParserOnly = 1 in {
  def NACL_CALL32d : NaClPI32<(outs), (ins i32imm_pcrel:$dst),
                     "naclcall\t$dst">;
  def NACL_CALL32r : NaClPI32<(outs), (ins GR32:$dst),
                     "naclcall\t$dst">;
}

// nacltlsaddr32 gets rewritten to:
//     .bundle_align_end
//     .bundle_lock
//     leal\t$sym@TLSGD, %eax
//     call\t___tls_get_addr@PLT
//     .bundle_unlock
// (The linker expects the leal+call sequence to be directly adjacent)
let Defs = [EAX, ECX, EDX, FP0, FP1, FP2, FP3, FP4, FP5, FP6, ST0,
            MM0, MM1, MM2, MM3, MM4, MM5, MM6, MM7,
            XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7,
            XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15, EFLAGS],
    Uses = [ESP],
    isAsmParserOnly = 1 in
def NACL_TLS_addr32 : NaClPI32<(outs), (ins i32mem:$sym),
                      "nacltlsaddr32\t$sym">;

//===----------------------------------------------------------------------===//
// 64-bit Native Client Pseudo Instructions
//===----------------------------------------------------------------------===//

class NaClPI64<dag outs, dag ins, string asm>
  : I<0, CustomFrm, outs, ins, asm, []>, Requires<[IsNaCl, In64BitMode]>;

let isTerminator = 1, isBarrier = 1, hasCtrlDep = 1, isAsmParserOnly = 1 in {
  def NACL_TRAP64  : NaClPI64<(outs), (ins), "nacltrap">;
}

let isTerminator = 1, isReturn = 1, isBarrier = 1,
    hasCtrlDep = 1, FPForm = SpecialFP, isAsmParserOnly = 1 in {
  def NACL_RET64  : NaClPI64<(outs), (ins), "naclret">;
}

let isBranch = 1, isTerminator = 1, isBarrier = 1, isIndirectBranch = 1,
    isAsmParserOnly = 1 in {
  def NACL_JMP64r : NaClPI64<(outs), (ins GR32:$dst, GR64:$rZP),
                    "nacljmp\t{$dst, $rZP|$rZP, $dst}">;
  def NACL_JMP64z : NaClPI64<(outs), (ins GR32:$dst),
                    "nacljmp\t$dst">;
}


let isCall = 1, isAsmParserOnly = 1 in {
  def NACL_CALL64d : NaClPI64<(outs), (ins i32imm_pcrel:$dst),
                     "naclcall\t$dst">;
  def NACL_CALL64r : NaClPI64<(outs), (ins GR32:$dst, GR64:$rZP),
                     "naclcall\t$dst,$rZP">;
}

let Defs = [RSP, EFLAGS], Uses = [RSP], isAsmParserOnly = 1 in {
  def NACL_ASPi8 : NaClPI64<(outs), (ins i64i8imm:$off, GR64:$rZP),
                   "naclasp{q}\t{$off, $rZP|$rZP, $off}">;

  def NACL_ASPi32: NaClPI64<(outs), (ins i64i32imm:$off, GR64:$rZP),
                   "naclasp{q}\t{$off, $rZP|$rZP, $off}">;

  def NACL_SSPi8 : NaClPI64<(outs), (ins i64i8imm:$off, GR64:$rZP),
                   "naclssp{q}\t{$off, $rZP|$rZP, $off}">;

  def NACL_SSPi32: NaClPI64<(outs), (ins i64i32imm:$off, GR64:$rZP),
                   "naclssp{q}\t{$off, $rZP|$rZP, $off}">;

  def NACL_ANDSPi32: NaClPI64<(outs), (ins i64i32imm:$off, GR64:$rZP),
                   "naclandsp{q}\t{$off, $rZP|$rZP, $off}">;
}

let Defs = [RSP], Uses = [RBP], isAsmParserOnly = 1 in {
  def NACL_SPADJi32  : NaClPI64<(outs), (ins i64i32imm:$off, GR64:$rZP),
                       "naclspadj\t{$off, $rZP|$rZP, $off}">;
}

let Defs = [RSP], isAsmParserOnly = 1 in {
  def NACL_RESTSPr   : NaClPI64<(outs), (ins GR32:$src, GR64:$rZP),
                       "naclrestsp_noflags\t{$src, $rZP|$rZP, $src}">;
  def NACL_RESTSPm   : NaClPI64<(outs), (ins i32mem:$src, GR64:$rZP),
                       "naclrestsp_noflags\t{$src, $rZP|$rZP, $src}">;
  def NACL_RESTSPrz   : NaClPI64<(outs), (ins GR32:$src),
                       "naclrestsp_noflags\t$src">;
}

def : MnemonicAlias<"naclrestsp", "naclrestsp_noflags">;

let Defs = [RBP], isAsmParserOnly = 1 in {
  def NACL_RESTBPr   : NaClPI64<(outs), (ins GR32:$src, GR64:$rZP),
                       "naclrestbp\t{$src, $rZP|$rZP, $src}">;
  def NACL_RESTBPm   : NaClPI64<(outs), (ins i32mem:$src, GR64:$rZP),
                       "naclrestbp\t{$src, $rZP|$rZP, $src}">;
  def NACL_RESTBPrz   : NaClPI64<(outs), (ins GR32:$src),
                       "naclrestbp\t$src">;
}

//===----------------------------------------------------------------------===//
//
// Code Generator Instructions (isCodeGenOnly == 1)
//
// These instructions exists to make CodeGen work with Native Client's
// modifications.
//
// Many of these instructions exist because of limitations in CodeGen
// or TableGen, and may become unnecessary in the future.
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
//
// CodeGen 32-bit
//
//===----------------------------------------------------------------------===//


// To avoid a naming conflict between call/naclcall, we have to
// disable the real CALLpcrel32 and CALL32r instructions when targeting
// for NaCl. Thus, they need to be produced here.

let isCall = 1 in
  // All calls clobber the non-callee saved registers. ESP is marked as
  // a use to prevent stack-pointer assignments that appear immediately
  // before calls from potentially appearing dead. Uses for argument
  // registers are added manually.
  let Defs = [EAX, ECX, EDX, FP0, FP1, FP2, FP3, FP4, FP5, FP6, ST0,
              MM0, MM1, MM2, MM3, MM4, MM5, MM6, MM7,
              XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7,
              XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15, EFLAGS],
      Uses = [ESP] in {

    def NACL_CG_CALLpcrel32 : I<0, Pseudo,
                              (outs), (ins i32imm_pcrel:$dst),
                              "naclcall\t$dst", []>,
                              Requires<[IsNaCl, In32BitMode]>;
    def NACL_CG_CALL32r     : I<0, Pseudo,
                              (outs), (ins GR32:$dst),
                              "naclcall\t$dst", [(X86call GR32:$dst)]>,
                              Requires<[IsNaCl, In32BitMode]>;
}

// Normal calls, with various flavors of addresses.
def : Pat<(X86call (i32 tglobaladdr:$dst)),
          (NACL_CG_CALLpcrel32 tglobaladdr:$dst)>,
          Requires<[IsNaCl, In32BitMode]>;
def : Pat<(X86call (i32 texternalsym:$dst)),
          (NACL_CG_CALLpcrel32 texternalsym:$dst)>,
          Requires<[IsNaCl, In32BitMode]>;
def : Pat<(X86call (i32 imm:$dst)),
          (NACL_CG_CALLpcrel32 imm:$dst)>,
          Requires<[IsNaCl, In32BitMode, CallImmAddr]>;

//===----------------------------------------------------------------------===//
//
// CodeGen 64-bit
//
//===----------------------------------------------------------------------===//


// Because pointers are 32-bit on X86-64 Native Client, we need to
// produce new versions of the JMP64/CALL64 instructions which can accept
// addresses which are i32 instead of i64.

let isBranch = 1, isTerminator = 1, isBarrier = 1, isIndirectBranch = 1 in {
  def NACL_CG_JMP64r     : I<0, Pseudo, (outs), (ins GR32:$dst),
                           "nacljmp\t$dst",
                           [(brind GR32:$dst)]>,
                           Requires<[IsNaCl, In64BitMode]>;
}

let isCall = 1 in
  // All calls clobber the non-callee saved registers. RSP is marked as
  // a use to prevent stack-pointer assignments that appear immediately
  // before calls from potentially appearing dead. Uses for argument
  // registers are added manually.
  let Defs = [RAX, RCX, RDX, RSI, RDI, R8, R9, R10, R11,
              FP0, FP1, FP2, FP3, FP4, FP5, FP6, ST0, ST1,
              MM0, MM1, MM2, MM3, MM4, MM5, MM6, MM7,
              XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7,
              XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15, EFLAGS],
      Uses = [RSP] in {

    def NACL_CG_CALL64pcrel32 : I<0, Pseudo, (outs),
                                (ins i32imm_pcrel:$dst),
                                "naclcall\t$dst", []>,
                                Requires<[IsNaCl, In64BitMode]>;

    def NACL_CG_CALL64r       : I<0, Pseudo, (outs), (ins GR32:$dst),
                                "naclcall\t$dst,%r15",
                                [(X86call GR32:$dst)]>,
                                Requires<[IsNaCl, In64BitMode]>;
}

def : Pat<(X86call (i32 tglobaladdr:$dst)),
          (NACL_CG_CALL64pcrel32 tglobaladdr:$dst)>,
      Requires<[IsNaCl, In64BitMode]>;
def : Pat<(X86call (i32 texternalsym:$dst)),
          (NACL_CG_CALL64pcrel32 texternalsym:$dst)>,
      Requires<[IsNaCl, In64BitMode]>;

// Tail calls
// Also needed due to the i64 / i32 pointer problem.
let isCall = 1, isTerminator = 1, isReturn = 1, isBarrier = 1,
    isCodeGenOnly = 1 in
  let Defs = [RAX, RCX, RDX, RSI, RDI, R8, R9, R10, R11,
              FP0, FP1, FP2, FP3, FP4, FP5, FP6, ST0, ST1,
              MM0, MM1, MM2, MM3, MM4, MM5, MM6, MM7,
              XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7,
              XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15, EFLAGS],
      Uses = [RSP] in {

  def NACL_CG_TCRETURNdi64 : I<0, Pseudo, (outs),
                             (ins i32imm_pcrel:$dst, i32imm:$offset),
                             "#TC_RETURN $dst $offset", []>,
                          Requires<[IsNaCl, In64BitMode]>;
  def NACL_CG_TCRETURNri64 : I<0, Pseudo, (outs),
                            (ins GR32_TC_64:$dst, i32imm:$offset),
                            "#TC_RETURN $dst $offset", []>,
                            Requires<[IsNaCl, In64BitMode]>;

  def NACL_CG_TAILJMPd64 : I<0, Pseudo, (outs),
                           (ins i32imm_pcrel:$dst),
                           "jmp\t$dst  # TAILCALL", []>,
                           Requires<[IsNaCl, In64BitMode]>;
  def NACL_CG_TAILJMPr64 : I<0, Pseudo, (outs),
                           (ins GR32_TC_64:$dst),
                           "nacljmp\t$dst,%r15  # TAILCALL", []>,
                           Requires<[IsNaCl, In64BitMode]>;
}

def : Pat<(X86tcret (i32 tglobaladdr:$dst), imm:$off),
          (NACL_CG_TCRETURNdi64 tglobaladdr:$dst, imm:$off)>,
	  Requires<[IsNaCl, In64BitMode]>;

def : Pat<(X86tcret (i32 texternalsym:$dst), imm:$off),
          (NACL_CG_TCRETURNdi64 texternalsym:$dst, imm:$off)>,
	  Requires<[IsNaCl, In64BitMode]>;

def : Pat<(X86tcret GR32_TC_64:$dst, imm:$off),
          (NACL_CG_TCRETURNri64 GR32_TC_64:$dst, imm:$off)>,
	  Requires<[IsNaCl, In64BitMode]>;

// ELF TLS Support

let Defs = [EAX, ECX, EDX, FP0, FP1, FP2, FP3, FP4, FP5, FP6, ST0,
            MM0, MM1, MM2, MM3, MM4, MM5, MM6, MM7,
            XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7,
            XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15, EFLAGS],
    Uses = [ESP] in
def NACL_CG_TLS_addr32 : I<0, Pseudo, (outs), (ins i32mem:$sym),
                         ".bundle_align_end"
                         ".bundle_lock"
                         "leal\t$sym, %eax; "
                         "call\t___tls_get_addr@PLT"
                         ".bundle_unlock",
                         [(X86tlsaddr tls32addr:$sym)]>,
                         Requires<[In32BitMode, IsNaCl]>;

// These are lowered in X86NaClRewritePass.
let Defs = [RAX, RCX, RDX, RSI, RDI, R8, R9, R10, R11,
            FP0, FP1, FP2, FP3, FP4, FP5, FP6, ST0, ST1,
            MM0, MM1, MM2, MM3, MM4, MM5, MM6, MM7,
            XMM0, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7,
            XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15, EFLAGS],
    Uses = [RSP] in {
def NACL_CG_GD_TLS_addr64 : I<0, Pseudo, (outs), (ins i32mem:$sym), "",
                            [(X86tlsaddr tls32addr:$sym)]>,
                            Requires<[IsNaCl, In64BitMode]>;
def NACL_CG_LE_TLS_addr64 : I<0, Pseudo, (outs), (ins i32mem:$sym), "",
                            [(X86tlsaddr_le tls32addr:$sym)]>,
                            Requires<[IsNaCl, In64BitMode]>;
def NACL_CG_IE_TLS_addr64 : I<0, Pseudo, (outs), (ins i32mem:$sym), "",
                            [(X86tlsaddr_ie tls32addr:$sym)]>,
                            Requires<[IsNaCl, In64BitMode]>;
// For mtls-use-call.
def NACL_CG_LE_TLS_addr32 : I<0, Pseudo, (outs), (ins i32mem:$sym), "",
                            [(X86tlsaddr_le tls32addr:$sym)]>,
                            Requires<[IsNaCl, In32BitMode]>;
def NACL_CG_IE_TLS_addr32 : I<0, Pseudo, (outs), (ins i32mem:$sym), "",
                            [(X86tlsaddr_ie tls32addr:$sym)]>,
                            Requires<[IsNaCl, In32BitMode]>;
}

let usesCustomInserter = 1, Defs = [EFLAGS] in
def NACL_CG_VAARG_64 : I<0, Pseudo,
                     (outs GR32:$dst),
                     (ins i8mem:$ap, i32imm:$size, i8imm:$mode, i32imm:$align),
                     "#NACL_VAARG_64 $dst, $ap, $size, $mode, $align",
                     [(set GR32:$dst,
                     (X86vaarg64 addr:$ap, imm:$size, imm:$mode, imm:$align)),
                     (implicit EFLAGS)]>,
                     Requires<[IsNaCl, In64BitMode]>;

//===----------------------------------------------------------------------===//
// NativeClient intrinsics
// These provide the ability to implement several low-level features without
// having to link native ASM code on the client.
// These need to be kept in sync with in lib/Target/ARM/ARMInstrInfo.td and
// lib/Target/X86/X86InstrNaCl.cpp.
// TODO(sehr): Separate this code to allow NaCl and non-NaCl versions.

// Saves all the callee-saves registers, [er]sp, and [er]ip to the JMP_BUF
// structure pointed to by 4(%esp) or rdi.  The JMP_BUF structure is the
// maximum size over all supported architectures.  The MC expansions happen
// in X86InstrNaCl.cpp.
let Uses = [ECX, RDX], Defs = [EAX, EFLAGS] in {
  def NACL_SETJ32 : I<0, Pseudo, (outs), (ins),
                     "movl %ebx, 0(%ecx); "
                     "movl %ebp, 4(%ecx); "
                     "movl %esp, 8(%ecx); "
                     "movl %esi, 12(%ecx); "
                     "movl %edi, 16(%ecx); "
                     "movl %edx, 20(%ecx); "
                     "xorl %eax, %eax; ",
                     [(set EAX, (int_nacl_setjmp ECX, EDX))]>,
                     Requires<[IsNaCl, In32BitMode]>;
}
let Uses = [EDI, RDX], Defs = [EAX, EFLAGS] in {
  def NACL_SETJ64 : I<0, Pseudo, (outs), (ins),
                      "movq %rbx, %nacl:0(%r15, %rdi); "
                      "movq %rbp, %nacl:8(%r15, %rdi); "
                      "movq %rsp, %nacl:16(%r15, %rdi); "
                      "movq %r12, %nacl:24(%r15, %rdi); "
                      "movq %r13, %nacl:32(%r15, %rdi); "
                      "movq %r14, %nacl:40(%r15, %rdi); "
                      "movq %rdx, %nacl:48(%r15, %rdi); "
                      "xorl %eax, %eax; ",
                      [(set EAX, (int_nacl_setjmp EDI, EDX))]>,
                      Requires<[IsNaCl, In64BitMode]>;
}

// Restores all the callee-saves registers, [er]sp, and [er]ip from the JMP_BUF
// structure pointed to by 4(%esp) or %rdi.  Returns the value in 8(%esp) or
// %rsi at entry.  This implements the tail of longjmp, with the normalization
// of the return value (if the caller passes zero to longjmp, it should return
// 1) done in the caller. The MC expansions happen in X86InstrNaCl.cpp.
let isBranch = 1, isTerminator = 1, isBarrier = 1, isIndirectBranch = 1,
    Uses = [EAX, ECX] in {
  def NACL_LONGJ32 : I<0, Pseudo, (outs), (ins),
                       "movl $$1, %ebx; "
                       "andl %eax, %eax; "
                       "cmovzl %ebx, %eax; "
                       "movl 0(%ecx), %ebx; "
                       "movl 4(%ecx), %ebp; "
                       "movl 8(%ecx), %esp; "
                       "movl 12(%ecx), %esi; "
                       "movl 16(%ecx), %edi; "
                       "movl 20(%ecx), %ecx; "
                       "nacljmp %ecx; ",
                       [(int_nacl_longjmp ECX, EAX)]>,
                       Requires<[IsNaCl, In32BitMode]>, TB;
}

let isBranch = 1, isTerminator = 1, isBarrier = 1, isIndirectBranch = 1,
    Uses = [EAX, EDI, R15] in {
  def NACL_LONGJ64 : I<0, Pseudo, (outs), (ins),
                       "movl $$1, %ebx; "
                       "andl %eax, %eax; "
                       "cmovzl %ebx, %eax; "
                       "movq %nacl:0(%r15, %edi), %rbx; "
                       "movq %nacl:8(%r15, %edi), %rdx; "
                       "naclrestbp %edx, %r15; "
                       "movq %nacl:16(%r15, %edi), %rdx; "
                       "naclrestsp %edx, %r15; "
                       "movq %nacl:24(%r15, %edi), %r12; "
                       "movq %nacl:32(%r15, %edi), %r13; "
                       "movq %nacl:40(%r15, %edi), %r14; "
                       "movq %nacl:48(%r15, %edi), %rcx; "
                       "nacljmp %ecx, %r15; ",
                       [(int_nacl_longjmp EDI, EAX)]>,
                       Requires<[IsNaCl, In64BitMode]>, TB;
}
