//=== X86NaClRewritePAss.cpp - Rewrite instructions for NaCl SFI --*- C++ -*-=//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains a pass that ensures stores and loads and stack/frame
// pointer addresses are within the NaCl sandbox (for x86-64).
// It also ensures that indirect control flow follows NaCl requirments.
//
// The other major portion of rewriting for NaCl is done in X86InstrNaCl.cpp,
// which is responsible for expanding the NaCl-specific operations introduced
// here and also the intrinsic functions to support setjmp, etc.
//===----------------------------------------------------------------------===//
#define DEBUG_TYPE "x86-sandboxing"

#include "X86.h"
#include "X86InstrInfo.h"
#include "X86NaClDecls.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

cl::opt<bool> FlagRestrictR15("sfi-restrict-r15",
                              cl::desc("Restrict use of %r15.  This flag can"
                                       " be turned off for the zero-based"
                                       " sandbox model."),
                              cl::init(true));

namespace {
  class X86NaClRewritePass : public MachineFunctionPass {
  public:
    static char ID;
    X86NaClRewritePass() : MachineFunctionPass(ID) {}

    virtual bool runOnMachineFunction(MachineFunction &Fn);

    virtual const char *getPassName() const {
      return "NaCl Rewrites";
    }

  private:

    const TargetMachine *TM;
    const TargetInstrInfo *TII;
    const TargetRegisterInfo *TRI;
    const X86Subtarget *Subtarget;
    bool Is64Bit;

    bool runOnMachineBasicBlock(MachineBasicBlock &MBB);

    void TraceLog(const char *func,
                  const MachineBasicBlock &MBB,
                  const MachineBasicBlock::iterator MBBI) const;

    bool ApplyRewrites(MachineBasicBlock &MBB,
                      MachineBasicBlock::iterator MBBI);
    bool ApplyStackSFI(MachineBasicBlock &MBB,
                       MachineBasicBlock::iterator MBBI);

    bool ApplyMemorySFI(MachineBasicBlock &MBB,
                        MachineBasicBlock::iterator MBBI);

    bool ApplyFrameSFI(MachineBasicBlock &MBB,
                       MachineBasicBlock::iterator MBBI);

    bool ApplyControlSFI(MachineBasicBlock &MBB,
                         MachineBasicBlock::iterator MBBI);

    bool AlignJumpTableTargets(MachineFunction &MF);
  };

  char X86NaClRewritePass::ID = 0;

}

static void DumpInstructionVerbose(const MachineInstr &MI) {
  DEBUG({
      dbgs() << MI;
      dbgs() << MI.getNumOperands() << " operands:" << "\n";
      for (unsigned i = 0; i < MI.getNumOperands(); ++i) {
        const MachineOperand& op = MI.getOperand(i);
        dbgs() << "  " << i << "(" << op.getType() << "):" << op << "\n";
      }
      dbgs() << "\n";
    });
}

static bool IsPushPop(MachineInstr &MI) {
  const unsigned Opcode = MI.getOpcode();
  switch (Opcode) {
   default:
    return false;
   case X86::PUSH64r:
   case X86::POP64r:
    return true;
  }
}

static bool IsStore(MachineInstr &MI) {
  return MI.getDesc().mayStore();
}

static bool IsLoad(MachineInstr &MI) {
  return MI.getDesc().mayLoad();
}

static bool IsFrameChange(MachineInstr &MI) {
  return MI.modifiesRegister(X86::EBP, NULL) ||
         MI.modifiesRegister(X86::RBP, NULL);
}

static bool IsStackChange(MachineInstr &MI) {
  return MI.modifiesRegister(X86::ESP, NULL) ||
         MI.modifiesRegister(X86::RSP, NULL);
}


static bool HasControlFlow(const MachineInstr &MI) {
 return MI.getDesc().isBranch() ||
        MI.getDesc().isCall() ||
        MI.getDesc().isReturn() ||
        MI.getDesc().isTerminator() ||
        MI.getDesc().isBarrier();
}

static bool IsDirectBranch(const MachineInstr &MI) {
  return  MI.getDesc().isBranch() &&
         !MI.getDesc().isIndirectBranch();
}

static bool IsRegAbsolute(unsigned Reg) {
  const bool RestrictR15 = FlagRestrictR15;
  assert(FlagUseZeroBasedSandbox || RestrictR15);
  return (Reg == X86::RSP || Reg == X86::RBP ||
          (Reg == X86::R15 && RestrictR15));
}

static bool FindMemoryOperand(const MachineInstr &MI, unsigned* index) {
  int NumFound = 0;
  unsigned MemOp = 0;
  for (unsigned i = 0; i < MI.getNumOperands(); ) {
    if (isMem(&MI, i)) {
      NumFound++;
      MemOp = i;
      i += X86::AddrNumOperands;
    } else {
      i++;
    }
  }

  // Intrinsics and other functions can have mayLoad and mayStore to reflect
  // the side effects of those functions.  This function is used to find
  // explicit memory references in the instruction, of which there are none.
  if (NumFound == 0)
    return false;

  if (NumFound > 1)
    llvm_unreachable("Too many memory operands in instruction!");

  *index = MemOp;
  return true;
}

static unsigned PromoteRegTo64(unsigned RegIn) {
  if (RegIn == 0)
    return 0;
  unsigned RegOut = getX86SubSuperRegister(RegIn, MVT::i64, false);
  assert(RegOut != 0);
  return RegOut;
}

static unsigned DemoteRegTo32(unsigned RegIn) {
  if (RegIn == 0)
    return 0;
  unsigned RegOut = getX86SubSuperRegister(RegIn, MVT::i32, false);
  assert(RegOut != 0);
  return RegOut;
}


//
// True if this MI restores RSP from RBP with a slight adjustment offset.
//
static bool MatchesSPAdj(const MachineInstr &MI) {
  assert (MI.getOpcode() == X86::LEA64r && "Call to MatchesSPAdj w/ non LEA");
  const MachineOperand &DestReg = MI.getOperand(0);
  const MachineOperand &BaseReg = MI.getOperand(1);
  const MachineOperand &Scale = MI.getOperand(2);
  const MachineOperand &IndexReg = MI.getOperand(3);
  const MachineOperand &Offset = MI.getOperand(4);
  return (DestReg.isReg() && DestReg.getReg() == X86::RSP &&
          BaseReg.isReg() && BaseReg.getReg() == X86::RBP &&
          Scale.getImm() == 1 &&
          IndexReg.isReg() && IndexReg.getReg() == 0 &&
          Offset.isImm());
}

void
X86NaClRewritePass::TraceLog(const char *func,
                             const MachineBasicBlock &MBB,
                             const MachineBasicBlock::iterator MBBI) const {
  DEBUG(dbgs() << "@" << func
        << "(" << MBB.getName() << ", " << (*MBBI) << ")\n");
}

bool X86NaClRewritePass::ApplyStackSFI(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator MBBI) {
  TraceLog("ApplyStackSFI", MBB, MBBI);
  assert(Is64Bit);
  MachineInstr &MI = *MBBI;

  if (!IsStackChange(MI))
    return false;

  if (IsPushPop(MI))
    return false;

  if (MI.getDesc().isCall())
    return false;

  unsigned Opc = MI.getOpcode();
  DebugLoc DL = MI.getDebugLoc();
  unsigned DestReg = MI.getOperand(0).getReg();
  assert(DestReg == X86::ESP || DestReg == X86::RSP);

  unsigned NewOpc = 0;
  switch (Opc) {
  case X86::ADD64ri8 : NewOpc = X86::NACL_ASPi8; break;
  case X86::ADD64ri32: NewOpc = X86::NACL_ASPi32; break;
  case X86::SUB64ri8 : NewOpc = X86::NACL_SSPi8; break;
  case X86::SUB64ri32: NewOpc = X86::NACL_SSPi32; break;
  case X86::AND64ri32: NewOpc = X86::NACL_ANDSPi32; break;
  }
  if (NewOpc) {
    BuildMI(MBB, MBBI, DL, TII->get(NewOpc))
      .addImm(MI.getOperand(2).getImm())
      .addReg(FlagUseZeroBasedSandbox ? 0 : X86::R15);
    MI.eraseFromParent();
    return true;
  }

  // Promote "MOV ESP, EBP" to a 64-bit move
  if (Opc == X86::MOV32rr && MI.getOperand(1).getReg() == X86::EBP) {
    MI.getOperand(0).setReg(X86::RSP);
    MI.getOperand(1).setReg(X86::RBP);
    MI.setDesc(TII->get(X86::MOV64rr));
    Opc = X86::MOV64rr;
  }

  // "MOV RBP, RSP" is already safe
  if (Opc == X86::MOV64rr && MI.getOperand(1).getReg() == X86::RBP) {
    return true;
  }

  //  Promote 32-bit lea to 64-bit lea (does this ever happen?)
  assert(Opc != X86::LEA32r && "Invalid opcode in 64-bit mode!");
  if (Opc == X86::LEA64_32r) {
    unsigned DestReg = MI.getOperand(0).getReg();
    unsigned BaseReg = MI.getOperand(1).getReg();
    unsigned Scale   = MI.getOperand(2).getImm();
    unsigned IndexReg = MI.getOperand(3).getReg();
    assert(DestReg == X86::ESP);
    assert(Scale == 1);
    assert(BaseReg == X86::EBP);
    assert(IndexReg == 0);
    MI.getOperand(0).setReg(X86::RSP);
    MI.getOperand(1).setReg(X86::RBP);
    MI.setDesc(TII->get(X86::LEA64r));
    Opc = X86::LEA64r;
  }

  if (Opc == X86::LEA64r && MatchesSPAdj(MI)) {
    const MachineOperand &Offset = MI.getOperand(4);
    BuildMI(MBB, MBBI, DL, TII->get(X86::NACL_SPADJi32))
      .addImm(Offset.getImm())
      .addReg(FlagUseZeroBasedSandbox ? 0 : X86::R15);
    MI.eraseFromParent();
    return true;
  }

  if (Opc == X86::MOV32rr || Opc == X86::MOV64rr) {
    BuildMI(MBB, MBBI, DL, TII->get(X86::NACL_RESTSPr))
      .addReg(DemoteRegTo32(MI.getOperand(1).getReg()))
      .addReg(FlagUseZeroBasedSandbox ? 0 : X86::R15);
    MI.eraseFromParent();
    return true;
  }

  if (Opc == X86::MOV32rm) {
    BuildMI(MBB, MBBI, DL, TII->get(X86::NACL_RESTSPm))
      .addOperand(MI.getOperand(1)) // Base
      .addOperand(MI.getOperand(2)) // Scale
      .addOperand(MI.getOperand(3)) // Index
      .addOperand(MI.getOperand(4)) // Offset
      .addOperand(MI.getOperand(5)) // Segment
      .addReg(FlagUseZeroBasedSandbox ? 0 : X86::R15);
    MI.eraseFromParent();
    return true;
  }

  DEBUG(DumpInstructionVerbose(MI));
  llvm_unreachable("Unhandled Stack SFI");
}

bool X86NaClRewritePass::ApplyFrameSFI(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator MBBI) {
  TraceLog("ApplyFrameSFI", MBB, MBBI);
  assert(Is64Bit);
  MachineInstr &MI = *MBBI;

  if (!IsFrameChange(MI))
    return false;

  unsigned Opc = MI.getOpcode();
  DebugLoc DL = MI.getDebugLoc();

  // Handle moves to RBP
  if (Opc == X86::MOV64rr) {
    assert(MI.getOperand(0).getReg() == X86::RBP);
    unsigned SrcReg = MI.getOperand(1).getReg();

    // MOV RBP, RSP is already safe
    if (SrcReg == X86::RSP)
      return false;

    // Rewrite: mov %rbp, %rX
    // To:      naclrestbp %eX, %rZP
    BuildMI(MBB, MBBI, DL, TII->get(X86::NACL_RESTBPr))
      .addReg(DemoteRegTo32(SrcReg))
      .addReg(FlagUseZeroBasedSandbox ? 0 : X86::R15); // rZP
    MI.eraseFromParent();
    return true;
  }

  // Handle memory moves to RBP
  if (Opc == X86::MOV64rm) {
    assert(MI.getOperand(0).getReg() == X86::RBP);

    // Zero-based sandbox model uses address clipping
    if (FlagUseZeroBasedSandbox)
      return false;

    // Rewrite: mov %rbp, (...)
    // To:      naclrestbp (...), %rZP
    BuildMI(MBB, MBBI, DL, TII->get(X86::NACL_RESTBPm))
      .addOperand(MI.getOperand(1))  // Base
      .addOperand(MI.getOperand(2))  // Scale
      .addOperand(MI.getOperand(3))  // Index
      .addOperand(MI.getOperand(4))  // Offset
      .addOperand(MI.getOperand(5))  // Segment
      .addReg(FlagUseZeroBasedSandbox ? 0 : X86::R15); // rZP
    MI.eraseFromParent();
    return true;
  }

  // Popping onto RBP
  // Rewrite to:
  //   naclrestbp (%rsp), %rZP
  //   naclasp $8, %rZP
  //
  // TODO(pdox): Consider rewriting to this instead:
  //   .bundle_lock
  //   pop %rbp
  //   mov %ebp,%ebp
  //   add %rZP, %rbp
  //   .bundle_unlock
  if (Opc == X86::POP64r) {
    assert(MI.getOperand(0).getReg() == X86::RBP);

    BuildMI(MBB, MBBI, DL, TII->get(X86::NACL_RESTBPm))
      .addReg(X86::RSP)  // Base
      .addImm(1)  // Scale
      .addReg(0)  // Index
      .addImm(0)  // Offset
      .addReg(0)  // Segment
      .addReg(FlagUseZeroBasedSandbox ? 0 : X86::R15); // rZP

    BuildMI(MBB, MBBI, DL, TII->get(X86::NACL_ASPi8))
      .addImm(8)
      .addReg(FlagUseZeroBasedSandbox ? 0 : X86::R15);

    MI.eraseFromParent();
    return true;
  }

  DEBUG(DumpInstructionVerbose(MI));
  llvm_unreachable("Unhandled Frame SFI");
}

bool X86NaClRewritePass::ApplyControlSFI(MachineBasicBlock &MBB,
                                         MachineBasicBlock::iterator MBBI) {
  const bool HideSandboxBase = (FlagHideSandboxBase &&
                                Is64Bit && !FlagUseZeroBasedSandbox);
  TraceLog("ApplyControlSFI", MBB, MBBI);
  MachineInstr &MI = *MBBI;

  if (!HasControlFlow(MI))
    return false;

  // Direct branches are OK
  if (IsDirectBranch(MI))
    return false;

  DebugLoc DL = MI.getDebugLoc();
  unsigned Opc = MI.getOpcode();

  // Rewrite indirect jump/call instructions
  unsigned NewOpc = 0;
  switch (Opc) {
  // 32-bit
  case X86::JMP32r               : NewOpc = X86::NACL_JMP32r; break;
  case X86::TAILJMPr             : NewOpc = X86::NACL_JMP32r; break;
  case X86::NACL_CG_CALL32r      : NewOpc = X86::NACL_CALL32r; break;
  // 64-bit
  case X86::NACL_CG_JMP64r       : NewOpc = X86::NACL_JMP64r; break;
  case X86::NACL_CG_CALL64r      : NewOpc = X86::NACL_CALL64r; break;
  case X86::NACL_CG_TAILJMPr64   : NewOpc = X86::NACL_JMP64r; break;
  }
  if (NewOpc) {
    MachineInstrBuilder NewMI =
     BuildMI(MBB, MBBI, DL, TII->get(NewOpc))
       .addOperand(MI.getOperand(0));
    if (Is64Bit) {
      NewMI.addReg(FlagUseZeroBasedSandbox ? 0 : X86::R15);
    }
    MI.eraseFromParent();
    return true;
  }

  // EH_RETURN has a single argment which is not actually used directly.
  // The argument gives the location where to reposition the stack pointer
  // before returning. EmitPrologue takes care of that repositioning.
  // So EH_RETURN just ultimately emits a plain "ret".
  // RETI returns and pops some number of bytes from the stack.
  if (Opc == X86::RET || Opc == X86::EH_RETURN || Opc == X86::EH_RETURN64 ||
      Opc == X86::RETI) {
    // To maintain compatibility with nacl-as, for now we don't emit naclret.
    // MI.setDesc(TII->get(Is64Bit ? X86::NACL_RET64 : X86::NACL_RET32));
    //
    // For NaCl64 returns, follow the convention of using r11 to hold
    // the target of an indirect jump to avoid potentially leaking the
    // sandbox base address.
    unsigned RegTarget;
    if (Is64Bit) {
      RegTarget = (HideSandboxBase ? X86::R11 : X86::RCX);
      BuildMI(MBB, MBBI, DL, TII->get(X86::POP64r), RegTarget);
      if (Opc == X86::RETI) {
        BuildMI(MBB, MBBI, DL, TII->get(X86::NACL_ASPi32))
          .addOperand(MI.getOperand(0))
          .addReg(FlagUseZeroBasedSandbox ? 0 : X86::R15);
      }
      BuildMI(MBB, MBBI, DL, TII->get(X86::NACL_JMP64r))
        .addReg(RegTarget)
        .addReg(FlagUseZeroBasedSandbox ? 0 : X86::R15);
    } else {
      RegTarget = X86::ECX;
      BuildMI(MBB, MBBI, DL, TII->get(X86::POP32r), RegTarget);
      if (Opc == X86::RETI) {
        BuildMI(MBB, MBBI, DL, TII->get(X86::ADD32ri), X86::ESP)
          .addReg(X86::ESP)
          .addOperand(MI.getOperand(0));
      }
      BuildMI(MBB, MBBI, DL, TII->get(X86::NACL_JMP32r))
        .addReg(RegTarget);
    }
    MI.eraseFromParent();
    return true;
  }

  // Rewrite trap
  if (Opc == X86::TRAP) {
    // To maintain compatibility with nacl-as, for now we don't emit nacltrap.
    // MI.setDesc(TII->get(Is64Bit ? X86::NACL_TRAP64 : X86::NACL_TRAP32));
    BuildMI(MBB, MBBI, DL, TII->get(X86::MOV32mi))
      .addReg(Is64Bit && !FlagUseZeroBasedSandbox ? X86::R15 : 0) // Base
      .addImm(1) // Scale
      .addReg(0) // Index
      .addImm(0) // Offset
      .addReg(0) // Segment
      .addImm(0); // Value
    MI.eraseFromParent();
    return true;
  }

  DEBUG(DumpInstructionVerbose(MI));
  llvm_unreachable("Unhandled Control SFI");
}

//
// Sandboxes loads and stores (64-bit only)
//
bool X86NaClRewritePass::ApplyMemorySFI(MachineBasicBlock &MBB,
                                        MachineBasicBlock::iterator MBBI) {
  TraceLog("ApplyMemorySFI", MBB, MBBI);
  assert(Is64Bit);
  MachineInstr &MI = *MBBI;

  if (!IsLoad(MI) && !IsStore(MI))
    return false;

  if (IsPushPop(MI))
    return false;

  unsigned MemOp;
  if (!FindMemoryOperand(MI, &MemOp))
    return false;
  assert(isMem(&MI, MemOp));
  MachineOperand &BaseReg  = MI.getOperand(MemOp + 0);
  MachineOperand &Scale = MI.getOperand(MemOp + 1);
  MachineOperand &IndexReg  = MI.getOperand(MemOp + 2);
  //MachineOperand &Disp = MI.getOperand(MemOp + 3);
  MachineOperand &SegmentReg = MI.getOperand(MemOp + 4);

  // RIP-relative addressing is safe.
  if (BaseReg.getReg() == X86::RIP)
    return false;

  // Make sure the base and index are 64-bit registers.
  IndexReg.setReg(PromoteRegTo64(IndexReg.getReg()));
  BaseReg.setReg(PromoteRegTo64(BaseReg.getReg()));
  assert(IndexReg.getSubReg() == 0);
  assert(BaseReg.getSubReg() == 0);

  bool AbsoluteBase = IsRegAbsolute(BaseReg.getReg());
  bool AbsoluteIndex = IsRegAbsolute(IndexReg.getReg());
  unsigned AddrReg = 0;

  if (AbsoluteBase && AbsoluteIndex) {
    llvm_unreachable("Unexpected absolute register pair");
  } else if (AbsoluteBase) {
    AddrReg = IndexReg.getReg();
  } else if (AbsoluteIndex) {
    assert(!BaseReg.getReg() && "Unexpected base register");
    assert(Scale.getImm() == 1);
    AddrReg = 0;
  } else {
    if (!BaseReg.getReg()) {
      // No base, fill in relative.
      BaseReg.setReg(FlagUseZeroBasedSandbox ? 0 : X86::R15);
      AddrReg = IndexReg.getReg();
    } else if (!FlagUseZeroBasedSandbox) {
      // Switch base and index registers if index register is undefined.
      // That is do conversions like "mov d(%r,0,0) -> mov d(%r15, %r, 1)".
      assert (!IndexReg.getReg()
              && "Unexpected index and base register");
      IndexReg.setReg(BaseReg.getReg());
      Scale.setImm(1);
      BaseReg.setReg(X86::R15);
      AddrReg = IndexReg.getReg();
    } else {
      llvm_unreachable(
          "Unexpected index and base register");
    }
  }

  if (AddrReg) {
    assert(!SegmentReg.getReg() && "Unexpected segment register");
    SegmentReg.setReg(X86::PSEUDO_NACL_SEG);
    return true;
  }

  return false;
}

bool X86NaClRewritePass::ApplyRewrites(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator MBBI) {
  MachineInstr &MI = *MBBI;
  DebugLoc DL = MI.getDebugLoc();
  unsigned Opc = MI.getOpcode();

  // These direct jumps need their opcode rewritten
  // and variable operands removed.
  unsigned NewOpc = 0;
  switch (Opc) {
  case X86::NACL_CG_CALLpcrel32  : NewOpc = X86::NACL_CALL32d; break;
  case X86::TAILJMPd             : NewOpc = X86::JMP_4; break;
  case X86::NACL_CG_TAILJMPd64   : NewOpc = X86::JMP_4; break;
  case X86::NACL_CG_CALL64pcrel32: NewOpc = X86::NACL_CALL64d; break;
  }
  if (NewOpc) {
    BuildMI(MBB, MBBI, DL, TII->get(NewOpc))
      .addOperand(MI.getOperand(0));
    MI.eraseFromParent();
    return true;
  }

  if (Opc == X86::NACL_CG_TLS_addr32) {
    // Rewrite to nacltlsaddr32
    BuildMI(MBB, MBBI, DL, TII->get(X86::NACL_TLS_addr32))
      .addOperand(MI.getOperand(0))  // Base
      .addOperand(MI.getOperand(1))  // Scale
      .addOperand(MI.getOperand(2))  // Index
      .addGlobalAddress(MI.getOperand(3).getGlobal(), 0, X86II::MO_TLSGD)
      .addOperand(MI.getOperand(4)); // Segment
    MI.eraseFromParent();
    return true;
  }

  // General Dynamic NaCl TLS model
  // http://code.google.com/p/nativeclient/issues/detail?id=1685
  if (Opc == X86::NACL_CG_GD_TLS_addr64) {

    // Rewrite to:
    //   leaq $sym@TLSGD(%rip), %rdi
    //   naclcall __tls_get_addr@PLT
    BuildMI(MBB, MBBI, DL, TII->get(X86::LEA64r), X86::RDI)
        .addReg(X86::RIP) // Base
        .addImm(1) // Scale
        .addReg(0) // Index
        .addGlobalAddress(MI.getOperand(3).getGlobal(), 0,
                          MI.getOperand(3).getTargetFlags())
        .addReg(0); // Segment
    BuildMI(MBB, MBBI, DL, TII->get(X86::NACL_CALL64d))
        .addExternalSymbol("__tls_get_addr", X86II::MO_PLT);
    MI.eraseFromParent();
    return true;
  }

  // Local Exec NaCl TLS Model
  if (Opc == X86::NACL_CG_LE_TLS_addr64 ||
      Opc == X86::NACL_CG_LE_TLS_addr32) {
    unsigned CallOpc, LeaOpc, Reg;
    // Rewrite to:
    //   naclcall __nacl_read_tp@PLT
    //   lea $sym@flag(,%reg), %reg
    if (Opc == X86::NACL_CG_LE_TLS_addr64) {
      CallOpc = X86::NACL_CALL64d;
      LeaOpc = X86::LEA64r;
      Reg = X86::RAX;
    } else {
      CallOpc = X86::NACL_CALL32d;
      LeaOpc = X86::LEA32r;
      Reg = X86::EAX;
    }
    BuildMI(MBB, MBBI, DL, TII->get(CallOpc))
        .addExternalSymbol("__nacl_read_tp", X86II::MO_PLT);
    BuildMI(MBB, MBBI, DL, TII->get(LeaOpc), Reg)
        .addReg(0) // Base
        .addImm(1) // Scale
        .addReg(Reg) // Index
        .addGlobalAddress(MI.getOperand(3).getGlobal(), 0,
                          MI.getOperand(3).getTargetFlags())
        .addReg(0); // Segment
    MI.eraseFromParent();
    return true;
  }

  // Initial Exec NaCl TLS Model
  if (Opc == X86::NACL_CG_IE_TLS_addr64 ||
      Opc == X86::NACL_CG_IE_TLS_addr32) {
    unsigned CallOpc, AddOpc, Base, Reg;
    // Rewrite to:
    //   naclcall __nacl_read_tp@PLT
    //   addq sym@flag(%base), %reg
    if (Opc == X86::NACL_CG_IE_TLS_addr64) {
      CallOpc = X86::NACL_CALL64d;
      AddOpc = X86::ADD64rm;
      Base = X86::RIP;
      Reg = X86::RAX;
    } else {
      CallOpc = X86::NACL_CALL32d;
      AddOpc = X86::ADD32rm;
      Base = MI.getOperand(3).getTargetFlags() == X86II::MO_INDNTPOFF ?
          0 : X86::EBX; // EBX for GOTNTPOFF.
      Reg = X86::EAX;
    }
    BuildMI(MBB, MBBI, DL, TII->get(CallOpc))
        .addExternalSymbol("__nacl_read_tp", X86II::MO_PLT);
    BuildMI(MBB, MBBI, DL, TII->get(AddOpc), Reg)
        .addReg(Reg)
        .addReg(Base)
        .addImm(1) // Scale
        .addReg(0) // Index
        .addGlobalAddress(MI.getOperand(3).getGlobal(), 0,
                          MI.getOperand(3).getTargetFlags())
        .addReg(0); // Segment
    MI.eraseFromParent();
    return true;
  }

  return false;
}

bool X86NaClRewritePass::AlignJumpTableTargets(MachineFunction &MF) {
  bool Modified = true;

  MF.setAlignment(5); // log2, 32 = 2^5

  MachineJumpTableInfo *JTI = MF.getJumpTableInfo();
  if (JTI != NULL) {
    const std::vector<MachineJumpTableEntry> &JT = JTI->getJumpTables();
    for (unsigned i = 0; i < JT.size(); ++i) {
      const std::vector<MachineBasicBlock*> &MBBs = JT[i].MBBs;
      for (unsigned j = 0; j < MBBs.size(); ++j) {
        MBBs[j]->setAlignment(5);
        Modified |= true;
      }
    }
  }
  return Modified;
}

bool X86NaClRewritePass::runOnMachineFunction(MachineFunction &MF) {
  bool Modified = false;

  TM = &MF.getTarget();
  TII = TM->getInstrInfo();
  TRI = TM->getRegisterInfo();
  Subtarget = &TM->getSubtarget<X86Subtarget>();
  Is64Bit = Subtarget->is64Bit();

  assert(Subtarget->isTargetNaCl() && "Unexpected target in NaClRewritePass!");

  DEBUG(dbgs() << "*************** NaCl Rewrite Pass ***************\n");
  for (MachineFunction::iterator MFI = MF.begin(), E = MF.end();
       MFI != E;
       ++MFI) {
    Modified |= runOnMachineBasicBlock(*MFI);
  }
  Modified |= AlignJumpTableTargets(MF);
  DEBUG(dbgs() << "*************** NaCl Rewrite DONE  ***************\n");
  return Modified;
}

bool X86NaClRewritePass::runOnMachineBasicBlock(MachineBasicBlock &MBB) {
  bool Modified = false;
  if (MBB.hasAddressTaken()) {
    //FIXME: use a symbolic constant or get this value from some configuration
    MBB.setAlignment(5);
    Modified = true;
  }
  for (MachineBasicBlock::iterator MBBI = MBB.begin(), NextMBBI = MBBI;
       MBBI != MBB.end(); MBBI = NextMBBI) {
    ++NextMBBI;
    // When one of these methods makes a change,
    // it returns true, skipping the others.
    if (ApplyRewrites(MBB, MBBI) ||
        (Is64Bit && ApplyStackSFI(MBB, MBBI)) ||
        (Is64Bit && ApplyMemorySFI(MBB, MBBI)) ||
        (Is64Bit && ApplyFrameSFI(MBB, MBBI)) ||
        ApplyControlSFI(MBB, MBBI)) {
      Modified = true;
    }
  }
  return Modified;
}

/// createX86NaClRewritePassPass - returns an instance of the pass.
namespace llvm {
  FunctionPass* createX86NaClRewritePass() {
    return new X86NaClRewritePass();
  }
}
