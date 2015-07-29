//=== X86MCNaCl.cpp - Expansion of NaCl pseudo-instructions      --*- C++ -*-=//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//
#define DEBUG_TYPE "x86-sandboxing"

#include "MCTargetDesc/X86MCTargetDesc.h"
#include "MCTargetDesc/X86BaseInfo.h"
#include "MCTargetDesc/X86MCNaCl.h"
#include "X86NaClDecls.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/CodeGen/ValueTypes.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

cl::opt<bool> FlagUseZeroBasedSandbox("sfi-zero-based-sandbox",
                                      cl::desc("Use a zero-based sandbox model"
                                               " for the NaCl SFI."),
                                      cl::init(false));
// This flag can be set to false to test the performance impact of
// hiding the sandbox base.
cl::opt<bool> FlagHideSandboxBase("sfi-hide-sandbox-base",
                                  cl::desc("Prevent 64-bit NaCl sandbox"
                                           " pointers from being written to"
                                           " the stack. [default=true]"),
                                  cl::init(true));

const int kNaClX86InstructionBundleSize = 32;

// See the notes below where these functions are defined.
namespace {
unsigned getX86SubSuperRegister_(unsigned Reg, EVT VT, bool High=false);
unsigned DemoteRegTo32_(unsigned RegIn);
} // namespace

static void PushReturnAddress(const llvm::MCSubtargetInfo &STI,
                              MCContext &Context, MCStreamer &Out,
                              MCSymbol *RetTarget) {
  const MCExpr *RetTargetExpr = MCSymbolRefExpr::Create(RetTarget, Context);
  if (Context.getObjectFileInfo()->getRelocM() == Reloc::PIC_) {
    // Calculate return_addr
    // The return address should not be calculated into R11 because if the push
    // instruction ends up at the start of a bundle, an attacker could arrange
    // an indirect jump to it, which would push the full jump target
    // (which itself was calculated into r11) onto the stack.
    MCInst LEAInst;
    LEAInst.setOpcode(X86::LEA64_32r);
    LEAInst.addOperand(MCOperand::CreateReg(X86::R10D)); // DestReg
    LEAInst.addOperand(MCOperand::CreateReg(X86::RIP)); // BaseReg
    LEAInst.addOperand(MCOperand::CreateImm(1)); // Scale
    LEAInst.addOperand(MCOperand::CreateReg(0)); // IndexReg
    LEAInst.addOperand(MCOperand::CreateExpr(RetTargetExpr)); // Offset
    LEAInst.addOperand(MCOperand::CreateReg(0)); // SegmentReg
    Out.EmitInstruction(LEAInst, STI);
    // push return_addr
    MCInst PUSHInst;
    PUSHInst.setOpcode(X86::PUSH64r);
    PUSHInst.addOperand(MCOperand::CreateReg(X86::R10));
    Out.EmitInstruction(PUSHInst, STI);
  } else {
    // push return_addr
    MCInst PUSHInst;
    PUSHInst.setOpcode(X86::PUSH64i32);
    PUSHInst.addOperand(MCOperand::CreateExpr(RetTargetExpr));
    Out.EmitInstruction(PUSHInst, STI);
  }
}

static void EmitDirectCall(const llvm::MCSubtargetInfo &STI,
                           const MCOperand &Op, bool Is64Bit, MCStreamer &Out) {
  const bool HideSandboxBase =
      (FlagHideSandboxBase && Is64Bit && !FlagUseZeroBasedSandbox);
  if (HideSandboxBase) {
    // For NaCl64, the sequence
    //   call target
    //   return_addr:
    // is changed to
    //   push return_addr
    //   jmp target
    //   .align 32
    //   return_addr:
    // This avoids exposing the sandbox base address via the return
    // address on the stack.

    // When generating PIC code, calculate the return address manually:
    //  leal return_addr(%rip), %r10d
    //  push %r10
    //  jmp target
    //  .align 32
    //  return_addr:

    MCContext &Context = Out.getContext();

    // Generate a label for the return address.
    MCSymbol *RetTarget = Context.createTempSymbol("DirectCallRetAddr", true);

    PushReturnAddress(STI, Context, Out, RetTarget);

    // jmp target
    MCInst JMPInst;
    JMPInst.setOpcode(X86::JMP_4);
    JMPInst.addOperand(Op);
    Out.EmitInstruction(JMPInst, STI);

    Out.EmitCodeAlignment(kNaClX86InstructionBundleSize);
    Out.EmitLabel(RetTarget);
  } else {
    Out.EmitBundleLock(true);

    MCInst CALLInst;
    CALLInst.setOpcode(Is64Bit ? X86::CALL64pcrel32 : X86::CALLpcrel32);
    CALLInst.addOperand(Op);
    Out.EmitInstruction(CALLInst, STI);
    Out.EmitBundleUnlock();
  }
}

static void EmitIndirectBranch(const llvm::MCSubtargetInfo &STI,
                               const MCOperand &Op, bool Is64Bit, bool IsCall,
                               MCStreamer &Out) {
  const bool HideSandboxBase =
      (FlagHideSandboxBase && Is64Bit && !FlagUseZeroBasedSandbox);
  const int JmpMask = -kNaClX86InstructionBundleSize;
  unsigned Reg32 = Op.getReg();

  // For NaCl64, the sequence
  //   jmp *%rXX
  // is changed to
  //   mov %rXX,%r11d
  //   and $0xffffffe0,%r11d
  //   add %r15,%r11
  //   jmpq *%r11
  //
  // And the sequence
  //   call *%rXX
  //   return_addr:
  // is changed to
  //   mov %rXX,%r11d
  //   push return_addr
  //   and $0xffffffe0,%r11d
  //   add %r15,%r11
  //   jmpq *%r11
  //   .align 32
  //   return_addr:
  //
  // This avoids exposing the sandbox base address via the return
  // address on the stack.

  // When generating PIC code for calls, calculate the return address manually:
  //   mov %rXX,%r11d
  //   leal return_addr(%rip), %r10d
  //   pushq %r10
  //   and $0xffffffe0,%r11d
  //   add %r15,%r11
  //   jmpq *%r11
  //   .align 32
  //   return_addr:

  MCSymbol *RetTarget = NULL;

  // For NaCl64, force an assignment of the branch target into r11,
  // and subsequently use r11 as the ultimate branch target, so that
  // only r11 (which will never be written to memory) exposes the
  // sandbox base address.  But avoid a redundant assignment if the
  // original branch target is already r11 or r11d.
  const unsigned SafeReg32 = X86::R11D;
  const unsigned SafeReg64 = X86::R11;
  if (HideSandboxBase) {
    // In some cases, EmitIndirectBranch() is called with a 32-bit
    // register Op (e.g. r11d), and in other cases a 64-bit register
    // (e.g. r11), so we need to test both variants to avoid a
    // redundant assignment.  TODO(stichnot): Make callers consistent
    // on 32 vs 64 bit register.
    if ((Reg32 != SafeReg32) && (Reg32 != SafeReg64)) {
      MCInst MOVInst;
      MOVInst.setOpcode(X86::MOV32rr);
      MOVInst.addOperand(MCOperand::CreateReg(SafeReg32));
      MOVInst.addOperand(MCOperand::CreateReg(Reg32));
      Out.EmitInstruction(MOVInst, STI);
      Reg32 = SafeReg32;
    }
    if (IsCall) {
      MCContext &Context = Out.getContext();
      // Generate a label for the return address.
      RetTarget = Context.createTempSymbol("IndirectCallRetAddr", true);
      // Explicitly push the (32-bit) return address for a NaCl64 call
      // instruction.
      PushReturnAddress(STI, Context, Out, RetTarget);
    }
  }
  const unsigned Reg64 = getX86SubSuperRegister_(Reg32, MVT::i64);

  const bool WillEmitCallInst = IsCall && !HideSandboxBase;
  Out.EmitBundleLock(WillEmitCallInst);

  MCInst ANDInst;
  ANDInst.setOpcode(X86::AND32ri8);
  ANDInst.addOperand(MCOperand::CreateReg(Reg32));
  ANDInst.addOperand(MCOperand::CreateReg(Reg32));
  ANDInst.addOperand(MCOperand::CreateImm(JmpMask));
  Out.EmitInstruction(ANDInst, STI);

  if (Is64Bit && !FlagUseZeroBasedSandbox) {
    MCInst InstADD;
    InstADD.setOpcode(X86::ADD64rr);
    InstADD.addOperand(MCOperand::CreateReg(Reg64));
    InstADD.addOperand(MCOperand::CreateReg(Reg64));
    InstADD.addOperand(MCOperand::CreateReg(X86::R15));
    Out.EmitInstruction(InstADD, STI);
  }

  if (WillEmitCallInst) {
    // callq *%rXX
    MCInst CALLInst;
    CALLInst.setOpcode(Is64Bit ? X86::CALL64r : X86::CALL32r);
    CALLInst.addOperand(MCOperand::CreateReg(Is64Bit ? Reg64 : Reg32));
    Out.EmitInstruction(CALLInst, STI);
  } else {
    // jmpq *%rXX   -or-   jmpq *%r11
    MCInst JMPInst;
    JMPInst.setOpcode(Is64Bit ? X86::JMP64r : X86::JMP32r);
    JMPInst.addOperand(MCOperand::CreateReg(Is64Bit ? Reg64 : Reg32));
    Out.EmitInstruction(JMPInst, STI);
  }
  Out.EmitBundleUnlock();
  if (RetTarget) {
    Out.EmitCodeAlignment(kNaClX86InstructionBundleSize);
    Out.EmitLabel(RetTarget);
  }
}

static void EmitRet(const llvm::MCSubtargetInfo &STI, const MCOperand *AmtOp,
                    bool Is64Bit, MCStreamer &Out) {
  // For NaCl64 returns, follow the convention of using r11 to hold
  // the target of an indirect jump to avoid potentially leaking the
  // sandbox base address.
  const bool HideSandboxBase = (FlagHideSandboxBase &&
                                Is64Bit && !FlagUseZeroBasedSandbox);
  // For NaCl64 sandbox hiding, use r11 to hold the branch target.
  // Otherwise, use rcx/ecx for fewer instruction bytes (no REX
  // prefix).
  const unsigned RegTarget = HideSandboxBase ? X86::R11 :
    (Is64Bit ? X86::RCX : X86::ECX);
  MCInst POPInst;
  POPInst.setOpcode(Is64Bit ? X86::POP64r : X86::POP32r);
  POPInst.addOperand(MCOperand::CreateReg(RegTarget));
  Out.EmitInstruction(POPInst, STI);

  if (AmtOp) {
    assert(!Is64Bit);
    MCInst ADDInst;
    unsigned ADDReg = X86::ESP;
    ADDInst.setOpcode(X86::ADD32ri);
    ADDInst.addOperand(MCOperand::CreateReg(ADDReg));
    ADDInst.addOperand(MCOperand::CreateReg(ADDReg));
    ADDInst.addOperand(*AmtOp);
    Out.EmitInstruction(ADDInst, STI);
  }

  EmitIndirectBranch(STI, MCOperand::CreateReg(RegTarget), Is64Bit, false, Out);
}

// Fix a register after being truncated to 32-bits.
static void EmitRegFix(const llvm::MCSubtargetInfo &STI, unsigned Reg64,
                       MCStreamer &Out) {
  // lea (%rsp, %r15, 1), %rsp
  // We do not need to add the R15 base for the zero-based sandbox model
  if (!FlagUseZeroBasedSandbox) {
    MCInst Tmp;
    Tmp.setOpcode(X86::LEA64r);
    Tmp.addOperand(MCOperand::CreateReg(Reg64));    // DestReg
    Tmp.addOperand(MCOperand::CreateReg(Reg64));    // BaseReg
    Tmp.addOperand(MCOperand::CreateImm(1));        // Scale
    Tmp.addOperand(MCOperand::CreateReg(X86::R15)); // IndexReg
    Tmp.addOperand(MCOperand::CreateImm(0));        // Offset
    Tmp.addOperand(MCOperand::CreateReg(0));        // SegmentReg
    Out.EmitInstruction(Tmp, STI);
  }
}

static void EmitSPArith(const llvm::MCSubtargetInfo &STI, unsigned Opc,
                        const MCOperand &ImmOp, MCStreamer &Out) {
  Out.EmitBundleLock(false);

  MCInst Tmp;
  Tmp.setOpcode(Opc);
  Tmp.addOperand(MCOperand::CreateReg(X86::ESP));
  Tmp.addOperand(MCOperand::CreateReg(X86::ESP));
  Tmp.addOperand(ImmOp);
  Out.EmitInstruction(Tmp, STI);

  EmitRegFix(STI, X86::RSP, Out);
  Out.EmitBundleUnlock();
}

static void EmitSPAdj(const llvm::MCSubtargetInfo &STI, const MCOperand &ImmOp,
                      MCStreamer &Out) {
  Out.EmitBundleLock(false);

  MCInst Tmp;
  Tmp.setOpcode(X86::LEA64_32r);
  Tmp.addOperand(MCOperand::CreateReg(X86::RSP)); // DestReg
  Tmp.addOperand(MCOperand::CreateReg(X86::RBP)); // BaseReg
  Tmp.addOperand(MCOperand::CreateImm(1));        // Scale
  Tmp.addOperand(MCOperand::CreateReg(0));        // IndexReg
  Tmp.addOperand(ImmOp);                          // Offset
  Tmp.addOperand(MCOperand::CreateReg(0));        // SegmentReg
  Out.EmitInstruction(Tmp, STI);

  EmitRegFix(STI, X86::RSP, Out);
  Out.EmitBundleUnlock();
}

static void EmitPrefix(const llvm::MCSubtargetInfo &STI, unsigned Opc,
                       MCStreamer &Out, X86MCNaClSFIState &State) {
  MCInst PrefixInst;
  PrefixInst.setOpcode(Opc);
  State.EmitRaw = true;
  Out.EmitInstruction(PrefixInst, STI);
  State.EmitRaw = false;
}

static void EmitMoveRegReg(const llvm::MCSubtargetInfo &STI, bool Is64Bit,
                           unsigned ToReg, unsigned FromReg, MCStreamer &Out) {
  MCInst Move;
  Move.setOpcode(Is64Bit ? X86::MOV64rr : X86::MOV32rr);
  Move.addOperand(MCOperand::CreateReg(ToReg));
  Move.addOperand(MCOperand::CreateReg(FromReg));
  Out.EmitInstruction(Move, STI);
}

static void EmitRegTruncate(const llvm::MCSubtargetInfo &STI, unsigned Reg64,
                            MCStreamer &Out) {
  unsigned Reg32 = getX86SubSuperRegister_(Reg64, MVT::i32);
  EmitMoveRegReg(STI, false, Reg32, Reg32, Out);
}

static void HandleMemoryRefTruncation(const llvm::MCSubtargetInfo &STI,
                                      MCInst *Inst, unsigned IndexOpPosition,
                                      MCStreamer &Out) {
  unsigned IndexReg = Inst->getOperand(IndexOpPosition).getReg();
  if (FlagUseZeroBasedSandbox) {
    // With the zero-based sandbox, we use a 32-bit register on the index
    Inst->getOperand(IndexOpPosition).setReg(DemoteRegTo32_(IndexReg));
  } else {
    EmitRegTruncate(STI, IndexReg, Out);
  }
}

static void ShortenMemoryRef(MCInst *Inst, unsigned IndexOpPosition) {
  unsigned ImmOpPosition = IndexOpPosition - 1;
  unsigned BaseOpPosition = IndexOpPosition - 2;
  unsigned IndexReg = Inst->getOperand(IndexOpPosition).getReg();
  // For the SIB byte, if the scale is 1 and the base is 0, then
  // an equivalent setup moves index to base, and index to 0.  The
  // equivalent setup is optimized to remove the SIB byte in
  // X86MCCodeEmitter.cpp.
  if (Inst->getOperand(ImmOpPosition).getImm() == 1 &&
      Inst->getOperand(BaseOpPosition).getReg() == 0) {
    Inst->getOperand(BaseOpPosition).setReg(IndexReg);
    Inst->getOperand(IndexOpPosition).setReg(0);
  }
}

static void EmitLoad(const llvm::MCSubtargetInfo &STI, bool Is64Bit,
                     unsigned DestReg, unsigned BaseReg, unsigned Scale,
                     unsigned IndexReg, unsigned Offset, unsigned SegmentReg,
                     MCStreamer &Out) {
  // Load DestReg from address BaseReg + Scale * IndexReg + Offset
  MCInst Load;
  Load.setOpcode(Is64Bit ? X86::MOV64rm : X86::MOV32rm);
  Load.addOperand(MCOperand::CreateReg(DestReg));
  Load.addOperand(MCOperand::CreateReg(BaseReg));
  Load.addOperand(MCOperand::CreateImm(Scale));
  Load.addOperand(MCOperand::CreateReg(IndexReg));
  Load.addOperand(MCOperand::CreateImm(Offset));
  Load.addOperand(MCOperand::CreateReg(SegmentReg));
  Out.EmitInstruction(Load, STI);
}

static bool SandboxMemoryRef(MCInst *Inst,
                             unsigned *IndexOpPosition) {
  for (unsigned i = 0, last = Inst->getNumOperands(); i < last; i++) {
    if (!Inst->getOperand(i).isReg() ||
        Inst->getOperand(i).getReg() != X86::PSEUDO_NACL_SEG) {
      continue;
    }
    // Return the index register that will need to be truncated.
    // The order of operands on a memory reference is always:
    // (BaseReg, ScaleImm, IndexReg, DisplacementImm, SegmentReg),
    // So if we found a match for a segment register value, we know that
    // the index register is exactly two operands prior.
    *IndexOpPosition = i - 2;

    // Remove the PSEUDO_NACL_SEG annotation.
    Inst->getOperand(i).setReg(0);
    return true;
  }
  return false;
}

static void EmitREST(const llvm::MCSubtargetInfo &STI, const MCInst &Inst,
                     unsigned Reg32, bool IsMem, MCStreamer &Out) {
  unsigned Reg64 = getX86SubSuperRegister_(Reg32, MVT::i64);
  Out.EmitBundleLock(false);
  if (!IsMem) {
    EmitMoveRegReg(STI, false, Reg32, Inst.getOperand(0).getReg(), Out);
  } else {
    unsigned IndexOpPosition;
    MCInst SandboxedInst = Inst;
    if (SandboxMemoryRef(&SandboxedInst, &IndexOpPosition)) {
      HandleMemoryRefTruncation(STI, &SandboxedInst, IndexOpPosition, Out);
      ShortenMemoryRef(&SandboxedInst, IndexOpPosition);
    }
    EmitLoad(STI, false, Reg32,
             SandboxedInst.getOperand(0).getReg(), // BaseReg
             SandboxedInst.getOperand(1).getImm(), // Scale
             SandboxedInst.getOperand(2).getReg(), // IndexReg
             SandboxedInst.getOperand(3).getImm(), // Offset
             SandboxedInst.getOperand(4).getReg(), // SegmentReg
             Out);
  }

  EmitRegFix(STI, Reg64, Out);
  Out.EmitBundleUnlock();
}


namespace {
// RAII holder for the recursion guard.
class EmitRawState {
 public:
  EmitRawState(X86MCNaClSFIState &S) : State(S) {
    State.EmitRaw = true;
  }
  ~EmitRawState() {
    State.EmitRaw = false;
  }
 private:
  X86MCNaClSFIState &State;
};
}

namespace llvm {
// CustomExpandInstNaClX86 -
//   If Inst is a NaCl pseudo instruction, emits the substitute
//   expansion to the MCStreamer and returns true.
//   Otherwise, returns false.
//
//   NOTE: Each time this function calls Out.EmitInstruction(), it will be
//   called again recursively to rewrite the new instruction being emitted.
//   Care must be taken to ensure that this does not result in an infinite
//   loop. Also, global state must be managed carefully so that it is
//   consistent during recursive calls.
//
//   We need global state to keep track of the explicit prefix (PREFIX_*)
//   instructions. Unfortunately, the assembly parser prefers to generate
//   these instead of combined instructions. At this time, having only
//   one explicit prefix is supported.
bool CustomExpandInstNaClX86(const llvm::MCSubtargetInfo &STI,
                             const MCInst &Inst, MCStreamer &Out,
                             X86MCNaClSFIState &State) {
  // If we are emitting to .s, only sandbox pseudos not supported by gas.
  if (Out.hasRawTextSupport()) {
    if (!(Inst.getOpcode() == X86::NACL_ANDSPi8 ||
          Inst.getOpcode() == X86::NACL_ANDSPi32))
      return false;
  }
  // If we make a call to EmitInstruction, we will be called recursively. In
  // this case we just want the raw instruction to be emitted instead of
  // handling the instruction here.
  if (State.EmitRaw == true) {
    return false;
  }
  EmitRawState E(State);
  unsigned Opc = Inst.getOpcode();
  DEBUG(dbgs() << "CustomExpandInstNaClX86("; Inst.dump(); dbgs() << ")\n");
  switch (Opc) {
  case X86::LOCK_PREFIX:
  case X86::REP_PREFIX:
  case X86::REPNE_PREFIX:
  case X86::REX64_PREFIX:
    // Ugly hack because LLVM AsmParser is not smart enough to combine
    // prefixes back into the instruction they modify.
    assert(State.PrefixSaved == 0);
    State.PrefixSaved = Opc;
    return true;
  case X86::CALLpcrel32:
    assert(State.PrefixSaved == 0);
    EmitDirectCall(STI, Inst.getOperand(0), false, Out);
    return true;
  case X86::CALL64pcrel32:
  case X86::NACL_CALL64d:
    assert(State.PrefixSaved == 0);
    EmitDirectCall(STI, Inst.getOperand(0), true, Out);
    return true;
  case X86::NACL_CALL32r:
    assert(State.PrefixSaved == 0);
    EmitIndirectBranch(STI, Inst.getOperand(0), false, true, Out);
    return true;
  case X86::NACL_CALL64r:
    assert(State.PrefixSaved == 0);
    EmitIndirectBranch(STI, Inst.getOperand(0), true, true, Out);
    return true;
  case X86::NACL_JMP32r:
    assert(State.PrefixSaved == 0);
    EmitIndirectBranch(STI, Inst.getOperand(0), false, false, Out);
    return true;
  case X86::NACL_JMP64r:
  case X86::NACL_JMP64z:
    assert(State.PrefixSaved == 0);
    EmitIndirectBranch(STI, Inst.getOperand(0), true, false, Out);
    return true;
  case X86::NACL_RET32:
    assert(State.PrefixSaved == 0);
    EmitRet(STI, NULL, false, Out);
    return true;
  case X86::NACL_RET64:
    assert(State.PrefixSaved == 0);
    EmitRet(STI, NULL, true, Out);
    return true;
  case X86::NACL_RETI32:
    assert(State.PrefixSaved == 0);
    EmitRet(STI, &Inst.getOperand(0), false, Out);
    return true;
  case X86::NACL_ASPi8:
    assert(State.PrefixSaved == 0);
    EmitSPArith(STI, X86::ADD32ri8, Inst.getOperand(0), Out);
    return true;
  case X86::NACL_ASPi32:
    assert(State.PrefixSaved == 0);
    EmitSPArith(STI, X86::ADD32ri, Inst.getOperand(0), Out);
    return true;
  case X86::NACL_SSPi8:
    assert(State.PrefixSaved == 0);
    EmitSPArith(STI, X86::SUB32ri8, Inst.getOperand(0), Out);
    return true;
  case X86::NACL_SSPi32:
    assert(State.PrefixSaved == 0);
    EmitSPArith(STI, X86::SUB32ri, Inst.getOperand(0), Out);
    return true;
  case X86::NACL_ANDSPi8:
    assert(State.PrefixSaved == 0);
    EmitSPArith(STI, X86::AND32ri8, Inst.getOperand(0), Out);
    return true;
  case X86::NACL_ANDSPi32:
    assert(State.PrefixSaved == 0);
    EmitSPArith(STI, X86::AND32ri, Inst.getOperand(0), Out);
    return true;
  case X86::NACL_SPADJi32:
    assert(State.PrefixSaved == 0);
    EmitSPAdj(STI, Inst.getOperand(0), Out);
    return true;
  case X86::NACL_RESTBPm:
    assert(State.PrefixSaved == 0);
    EmitREST(STI, Inst, X86::EBP, true, Out);
    return true;
  case X86::NACL_RESTBPr:
  case X86::NACL_RESTBPrz:
    assert(State.PrefixSaved == 0);
    EmitREST(STI, Inst, X86::EBP, false, Out);
    return true;
  case X86::NACL_RESTSPm:
    assert(State.PrefixSaved == 0);
    EmitREST(STI, Inst, X86::ESP, true, Out);
    return true;
  case X86::NACL_RESTSPr:
  case X86::NACL_RESTSPrz:
    assert(State.PrefixSaved == 0);
    EmitREST(STI, Inst, X86::ESP, false, Out);
    return true;
  }

  unsigned IndexOpPosition;
  MCInst SandboxedInst = Inst;
  // If we need to sandbox a memory reference and we have a saved prefix,
  // use a single bundle-lock/unlock for the whole sequence of
  // added_truncating_inst + prefix + mem_ref_inst.
  if (SandboxMemoryRef(&SandboxedInst, &IndexOpPosition)) {
    unsigned PrefixLocal = State.PrefixSaved;
    State.PrefixSaved = 0;

    if (PrefixLocal || !FlagUseZeroBasedSandbox)
      Out.EmitBundleLock(false);

    HandleMemoryRefTruncation(STI, &SandboxedInst, IndexOpPosition, Out);
    ShortenMemoryRef(&SandboxedInst, IndexOpPosition);

    if (PrefixLocal)
      EmitPrefix(STI, PrefixLocal, Out, State);
    Out.EmitInstruction(SandboxedInst, STI);

    if (PrefixLocal || !FlagUseZeroBasedSandbox)
      Out.EmitBundleUnlock();
    return true;
  }

  // If the special case above doesn't apply, but there is still a saved prefix,
  // then the saved prefix should be bundled-locked with Inst, so that it cannot
  // be separated by bundle padding.
  if (State.PrefixSaved) {
    unsigned PrefixLocal = State.PrefixSaved;
    State.PrefixSaved = 0;
    Out.EmitBundleLock(false);
    EmitPrefix(STI, PrefixLocal, Out, State);
    Out.EmitInstruction(Inst, STI);
    Out.EmitBundleUnlock();
    return true;
  }
  return false;
}

} // namespace llvm




// @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//
// This is an exact copy of getX86SubSuperRegister from X86RegisterInfo.h
// We cannot use the original because it is part of libLLVMX86CodeGen,
// which cannot be a dependency of this module (libLLVMX86Desc).
//
// However, in all likelyhood, the real getX86SubSuperRegister will
// eventually be moved to MCTargetDesc, and then this copy can be
// removed.

namespace {
unsigned getX86SubSuperRegister_(unsigned Reg, EVT VT, bool High) {
  switch (VT.getSimpleVT().SimpleTy) {
  default: return Reg;
  case MVT::i8:
    if (High) {
      switch (Reg) {
      default: return 0;
      case X86::AH: case X86::AL: case X86::AX: case X86::EAX: case X86::RAX:
        return X86::AH;
      case X86::DH: case X86::DL: case X86::DX: case X86::EDX: case X86::RDX:
        return X86::DH;
      case X86::CH: case X86::CL: case X86::CX: case X86::ECX: case X86::RCX:
        return X86::CH;
      case X86::BH: case X86::BL: case X86::BX: case X86::EBX: case X86::RBX:
        return X86::BH;
      }
    } else {
      switch (Reg) {
      default: return 0;
      case X86::AH: case X86::AL: case X86::AX: case X86::EAX: case X86::RAX:
        return X86::AL;
      case X86::DH: case X86::DL: case X86::DX: case X86::EDX: case X86::RDX:
        return X86::DL;
      case X86::CH: case X86::CL: case X86::CX: case X86::ECX: case X86::RCX:
        return X86::CL;
      case X86::BH: case X86::BL: case X86::BX: case X86::EBX: case X86::RBX:
        return X86::BL;
      case X86::SIL: case X86::SI: case X86::ESI: case X86::RSI:
        return X86::SIL;
      case X86::DIL: case X86::DI: case X86::EDI: case X86::RDI:
        return X86::DIL;
      case X86::BPL: case X86::BP: case X86::EBP: case X86::RBP:
        return X86::BPL;
      case X86::SPL: case X86::SP: case X86::ESP: case X86::RSP:
        return X86::SPL;
      case X86::R8B: case X86::R8W: case X86::R8D: case X86::R8:
        return X86::R8B;
      case X86::R9B: case X86::R9W: case X86::R9D: case X86::R9:
        return X86::R9B;
      case X86::R10B: case X86::R10W: case X86::R10D: case X86::R10:
        return X86::R10B;
      case X86::R11B: case X86::R11W: case X86::R11D: case X86::R11:
        return X86::R11B;
      case X86::R12B: case X86::R12W: case X86::R12D: case X86::R12:
        return X86::R12B;
      case X86::R13B: case X86::R13W: case X86::R13D: case X86::R13:
        return X86::R13B;
      case X86::R14B: case X86::R14W: case X86::R14D: case X86::R14:
        return X86::R14B;
      case X86::R15B: case X86::R15W: case X86::R15D: case X86::R15:
        return X86::R15B;
      }
    }
  case MVT::i16:
    switch (Reg) {
    default: return Reg;
    case X86::AH: case X86::AL: case X86::AX: case X86::EAX: case X86::RAX:
      return X86::AX;
    case X86::DH: case X86::DL: case X86::DX: case X86::EDX: case X86::RDX:
      return X86::DX;
    case X86::CH: case X86::CL: case X86::CX: case X86::ECX: case X86::RCX:
      return X86::CX;
    case X86::BH: case X86::BL: case X86::BX: case X86::EBX: case X86::RBX:
      return X86::BX;
    case X86::SIL: case X86::SI: case X86::ESI: case X86::RSI:
      return X86::SI;
    case X86::DIL: case X86::DI: case X86::EDI: case X86::RDI:
      return X86::DI;
    case X86::BPL: case X86::BP: case X86::EBP: case X86::RBP:
      return X86::BP;
    case X86::SPL: case X86::SP: case X86::ESP: case X86::RSP:
      return X86::SP;
    case X86::R8B: case X86::R8W: case X86::R8D: case X86::R8:
      return X86::R8W;
    case X86::R9B: case X86::R9W: case X86::R9D: case X86::R9:
      return X86::R9W;
    case X86::R10B: case X86::R10W: case X86::R10D: case X86::R10:
      return X86::R10W;
    case X86::R11B: case X86::R11W: case X86::R11D: case X86::R11:
      return X86::R11W;
    case X86::R12B: case X86::R12W: case X86::R12D: case X86::R12:
      return X86::R12W;
    case X86::R13B: case X86::R13W: case X86::R13D: case X86::R13:
      return X86::R13W;
    case X86::R14B: case X86::R14W: case X86::R14D: case X86::R14:
      return X86::R14W;
    case X86::R15B: case X86::R15W: case X86::R15D: case X86::R15:
      return X86::R15W;
    }
  case MVT::i32:
    switch (Reg) {
    default: return Reg;
    case X86::AH: case X86::AL: case X86::AX: case X86::EAX: case X86::RAX:
      return X86::EAX;
    case X86::DH: case X86::DL: case X86::DX: case X86::EDX: case X86::RDX:
      return X86::EDX;
    case X86::CH: case X86::CL: case X86::CX: case X86::ECX: case X86::RCX:
      return X86::ECX;
    case X86::BH: case X86::BL: case X86::BX: case X86::EBX: case X86::RBX:
      return X86::EBX;
    case X86::SIL: case X86::SI: case X86::ESI: case X86::RSI:
      return X86::ESI;
    case X86::DIL: case X86::DI: case X86::EDI: case X86::RDI:
      return X86::EDI;
    case X86::BPL: case X86::BP: case X86::EBP: case X86::RBP:
      return X86::EBP;
    case X86::SPL: case X86::SP: case X86::ESP: case X86::RSP:
      return X86::ESP;
    case X86::R8B: case X86::R8W: case X86::R8D: case X86::R8:
      return X86::R8D;
    case X86::R9B: case X86::R9W: case X86::R9D: case X86::R9:
      return X86::R9D;
    case X86::R10B: case X86::R10W: case X86::R10D: case X86::R10:
      return X86::R10D;
    case X86::R11B: case X86::R11W: case X86::R11D: case X86::R11:
      return X86::R11D;
    case X86::R12B: case X86::R12W: case X86::R12D: case X86::R12:
      return X86::R12D;
    case X86::R13B: case X86::R13W: case X86::R13D: case X86::R13:
      return X86::R13D;
    case X86::R14B: case X86::R14W: case X86::R14D: case X86::R14:
      return X86::R14D;
    case X86::R15B: case X86::R15W: case X86::R15D: case X86::R15:
      return X86::R15D;
    }
  case MVT::i64:
    switch (Reg) {
    default: return Reg;
    case X86::AH: case X86::AL: case X86::AX: case X86::EAX: case X86::RAX:
      return X86::RAX;
    case X86::DH: case X86::DL: case X86::DX: case X86::EDX: case X86::RDX:
      return X86::RDX;
    case X86::CH: case X86::CL: case X86::CX: case X86::ECX: case X86::RCX:
      return X86::RCX;
    case X86::BH: case X86::BL: case X86::BX: case X86::EBX: case X86::RBX:
      return X86::RBX;
    case X86::SIL: case X86::SI: case X86::ESI: case X86::RSI:
      return X86::RSI;
    case X86::DIL: case X86::DI: case X86::EDI: case X86::RDI:
      return X86::RDI;
    case X86::BPL: case X86::BP: case X86::EBP: case X86::RBP:
      return X86::RBP;
    case X86::SPL: case X86::SP: case X86::ESP: case X86::RSP:
      return X86::RSP;
    case X86::R8B: case X86::R8W: case X86::R8D: case X86::R8:
      return X86::R8;
    case X86::R9B: case X86::R9W: case X86::R9D: case X86::R9:
      return X86::R9;
    case X86::R10B: case X86::R10W: case X86::R10D: case X86::R10:
      return X86::R10;
    case X86::R11B: case X86::R11W: case X86::R11D: case X86::R11:
      return X86::R11;
    case X86::R12B: case X86::R12W: case X86::R12D: case X86::R12:
      return X86::R12;
    case X86::R13B: case X86::R13W: case X86::R13D: case X86::R13:
      return X86::R13;
    case X86::R14B: case X86::R14W: case X86::R14D: case X86::R14:
      return X86::R14;
    case X86::R15B: case X86::R15W: case X86::R15D: case X86::R15:
      return X86::R15;
    }
  }

  return Reg;
}

// This is a copy of DemoteRegTo32 from X86NaClRewritePass.cpp.
// We cannot use the original because it uses part of libLLVMX86CodeGen,
// which cannot be a dependency of this module (libLLVMX86Desc).
// Note that this function calls getX86SubSuperRegister_, which is
// also a copied function for the same reason.

unsigned DemoteRegTo32_(unsigned RegIn) {
  if (RegIn == 0)
    return 0;
  unsigned RegOut = getX86SubSuperRegister_(RegIn, MVT::i32, false);
  assert(RegOut != 0);
  return RegOut;
}
} //namespace
// @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

unsigned getReg32(unsigned Reg) {
  return getX86SubSuperRegister_(Reg, MVT::i32, false);
}

unsigned getReg64(unsigned Reg) {
  return getX86SubSuperRegister_(Reg, MVT::i64, false);
}
