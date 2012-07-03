//=== X86NaClRewriteFinalPass.cpp - Expand NaCl pseudo-instructions  --*- C++ -*-=//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
// This pass expands NaCl pseudo-instructions into real instructions.
// This duplicates much of the functionality found in X86MCNaCl.cpp but is
// needed for non-MC JIT, which doesn't use MC. It expands pseudo instructions
// into bundle-locked groups by emitting a BUNDLE_LOCK marker,
// followed by the instructions, followed by a BUNDLE_UNLOCK marker.
// The Code Emitter needs to ensure the alignment as it emits. Additionallly,
// this pass needs to be run last, or the user at least needs to ensure that
// subsequent passes do not reorder or remove any bundled groups.
//===----------------------------------------------------------------------===//
#define DEBUG_TYPE "x86-jit-sandboxing"
#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Function.h"

using namespace llvm;

extern cl::opt<int> FlagSfiX86JmpMask;

namespace {
  class X86NaClRewriteFinalPass : public MachineFunctionPass {
  public:
    static char ID;
    X86NaClRewriteFinalPass() : MachineFunctionPass(ID),
        kJumpMask(FlagSfiX86JmpMask) {}

    virtual bool runOnMachineFunction(MachineFunction &Fn);

    virtual const char *getPassName() const {
      return "NaCl Pseudo-instruction expansion";
    }

  private:
    const int kJumpMask;
    const TargetMachine *TM;
    const TargetInstrInfo *TII;
    const TargetRegisterInfo *TRI;
    bool Is64Bit;

    bool runOnMachineBasicBlock(MachineBasicBlock &MBB);

    void TraceLog(const char *fun,
		  const MachineBasicBlock &MBB,
		  const MachineBasicBlock::iterator MBBI) const;

    void RewriteIndirectJump(MachineBasicBlock &MBB,
                             MachineBasicBlock::iterator MBBI,
                             bool Is64Bit,
                             bool IsCall);
    void RewriteDirectCall(MachineBasicBlock &MBB,
                           MachineBasicBlock::iterator MBBI,
                           bool Is64Bit);
    bool ApplyCommonRewrites(MachineBasicBlock &MBB,
                             MachineBasicBlock::iterator MBBI);

  };

  char X86NaClRewriteFinalPass::ID = 0;
}

void X86NaClRewriteFinalPass::RewriteIndirectJump(MachineBasicBlock &MBB,
    MachineBasicBlock::iterator MBBI,
    bool Is64Bit,
    bool IsCall) {
  MachineInstr &MI = *MBBI;
  DebugLoc DL = MI.getDebugLoc();

  DEBUG(dbgs() << "rewrite indirect jump " << MBB);

  unsigned reg32 = MI.getOperand(0).getReg();
  unsigned reg64 = getX86SubSuperRegister(reg32, MVT::i64);

  if (IsCall)
    BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::BUNDLE_ALIGN_END));

  BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::BUNDLE_LOCK));

  BuildMI(MBB, MBBI, DL, TII->get(X86::AND32ri8))
    .addReg(reg32)
    .addReg(reg32)
    //.addOperand(MI.getOperand(0))//correct flags, but might be 64bit reg
    .addImm(kJumpMask);

  if (Is64Bit) {
    BuildMI(MBB, MBBI, DL, TII->get(X86::ADD64rr))
      .addReg(reg64)
      .addReg(reg64)
      .addReg(X86::R15);
  }

  if (IsCall) {
    BuildMI(MBB, MBBI, DL, TII->get(Is64Bit ? X86::CALL64r : X86::CALL32r))
        .addReg(Is64Bit ? reg64 : reg32);
  } else {
    BuildMI(MBB, MBBI, DL, TII->get(Is64Bit ? X86::JMP64r : X86::JMP32r))
        .addReg(Is64Bit ? reg64 : reg32);
  }

  BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::BUNDLE_UNLOCK));
  MI.eraseFromParent();

  DEBUG(dbgs() << "done rewrite indirect jump " << MBB);
}

void X86NaClRewriteFinalPass::RewriteDirectCall(MachineBasicBlock &MBB,
    MachineBasicBlock::iterator MBBI,
    bool Is64Bit) {
  MachineInstr &MI = *MBBI;
  DebugLoc DL = MI.getDebugLoc();
  DEBUG(dbgs() << "rewrite direct call " << MBB);
  const MachineOperand &MO = MI.getOperand(0);
  // rewrite calls to immediates as indirect calls.
  if (MO.isImm()) {
    DEBUG(dbgs() << " is immediate " << MO);
    // First, rewrite as a move imm->reg + indirect call sequence,
    BuildMI(MBB, MBBI, DL, TII->get(X86::MOV32ri))
            .addReg(X86::ECX)
            .addOperand(MO);
    BuildMI(MBB, MBBI, DL, TII->get(Is64Bit ? X86::CALL64r : X86::CALL32r))
            .addReg(X86::ECX);
    // Then use RewriteIndirectJump to sandbox it
    MachineBasicBlock::iterator I = MBBI;
    --I; // I now points at the call instruction
    MI.eraseFromParent();
    return RewriteIndirectJump(MBB, I, Is64Bit, true);
  }

  BuildMI(MBB, MBBI, DL, TII->get(TargetOpcode::BUNDLE_ALIGN_END));

  BuildMI(MBB, MBBI, DL,
          TII->get(Is64Bit ? X86::CALL64pcrel32 : X86::CALLpcrel32))
          .addOperand(MI.getOperand(0));

  MI.eraseFromParent();
}

bool X86NaClRewriteFinalPass::ApplyCommonRewrites(MachineBasicBlock &MBB,
    MachineBasicBlock::iterator MBBI) {
  MachineInstr &MI = *MBBI;
  unsigned Opcode = MI.getOpcode();
  switch(Opcode) {
  case X86::NACL_CALL32d:
    RewriteDirectCall(MBB, MBBI, false);
    break;
  case X86::NACL_CALL64d:
    RewriteDirectCall(MBB, MBBI, true);
    break;
  case X86::NACL_CALL32r:
    RewriteIndirectJump(MBB, MBBI, false, true);
    return true;
  case X86::NACL_CALL64r:
    RewriteIndirectJump(MBB, MBBI, true, true);
    return true;
  case X86::NACL_JMP32r:
    RewriteIndirectJump(MBB, MBBI, false, false);
    return true;
  case X86::NACL_JMP64r:
    RewriteIndirectJump(MBB, MBBI, true, false);
    return true;
  case X86::NACL_TRAP32:
  case X86::NACL_TRAP64:
  case X86::NACL_ASPi8:
  case X86::NACL_ASPi32:
  case X86::NACL_SSPi8:
  case X86::NACL_SSPi32:
  case X86::NACL_SPADJi32:
  case X86::NACL_RESTBPm:
  case X86::NACL_RESTBPr:
  case X86::NACL_RESTSPm:
  case X86::NACL_RESTSPr:
  case X86::NACL_SETJ32:
  case X86::NACL_SETJ64:
  case X86::NACL_LONGJ32:
  case X86::NACL_LONGJ64:
    dbgs() << "inst, opcode not handled: " << MI << Opcode;
    assert(false && "NaCl Pseudo-inst not handled");
  case X86::NACL_RET32:
  case X86::NACL_RET64:
  case X86::NACL_RETI32:
    assert(false && "Should not get RETs here");
  }
  return false;
}

bool X86NaClRewriteFinalPass::runOnMachineFunction(MachineFunction &MF) {
  bool modified = false;
  TM = &MF.getTarget();
  TII = TM->getInstrInfo();
  TRI = TM->getRegisterInfo();
  const X86Subtarget *subtarget = &TM->getSubtarget<X86Subtarget>();
  assert(subtarget->isTargetNaCl() && "Target in NaClRewriteFinal is not NaCl");

  DEBUG(dbgs() << "*************** NaCl Rewrite Final ***************\n");
  DEBUG(dbgs() << " funcnum " << MF.getFunctionNumber() << " "
               << MF.getFunction()->getName() << "\n");

  for (MachineFunction::iterator MFI = MF.begin(), E = MF.end(); 
       MFI != E; ++MFI) {
    modified |= runOnMachineBasicBlock(*MFI);
  }

  DEBUG(dbgs() << "************* NaCl Rewrite Final Done *************\n");
  return modified;
}

bool X86NaClRewriteFinalPass::runOnMachineBasicBlock(MachineBasicBlock &MBB) {
  bool modified = false;
  for (MachineBasicBlock::iterator MBBI = MBB.begin(), NextMBBI = MBBI;
       MBBI != MBB.end(); MBBI = NextMBBI) {
    ++NextMBBI;
    if (ApplyCommonRewrites(MBB, MBBI)) {
      modified = true;
    }
  }
  return modified;
}

// return an instance of the pass
namespace llvm {
  FunctionPass *createX86NaClRewriteFinalPass() {
    return new X86NaClRewriteFinalPass();
  }
}
