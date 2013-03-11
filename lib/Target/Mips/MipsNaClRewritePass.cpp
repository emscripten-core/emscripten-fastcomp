//===-- MipsNaClRewritePass.cpp - Native Client Rewrite Pass  -----*- C++ -*-=//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Native Client Rewrite Pass
// This final pass inserts the sandboxing instructions needed to run inside
// the Native Client sandbox. Native Client requires certain software fault
// isolation (SFI) constructions to be put in place, to prevent escape from
// the sandbox. Native Client refuses to execute binaries without the correct
// SFI sequences.
//
// Potentially dangerous operations which are protected include:
// * Stores
// * Branches
// * Changes to SP
//
//===----------------------------------------------------------------------===//

#define DEBUG_TYPE "mips-sfi"
#include "Mips.h"
#include "MipsInstrInfo.h"
#include "MipsNaClRewritePass.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineJumpTableInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"

using namespace llvm;

unsigned Mips::IndirectBranchMaskReg = Mips::T6;
unsigned Mips::LoadStoreStackMaskReg = Mips::T7;

namespace {
  class MipsNaClRewritePass : public MachineFunctionPass {
  public:
    static char ID;
    MipsNaClRewritePass() : MachineFunctionPass(ID) {}

    const MipsInstrInfo *TII;
    const TargetRegisterInfo *TRI;
    virtual bool runOnMachineFunction(MachineFunction &Fn);

    virtual const char *getPassName() const {
      return "Mips Native Client Rewrite Pass";
    }

  private:

    bool SandboxLoadsInBlock(MachineBasicBlock &MBB);
    bool SandboxStoresInBlock(MachineBasicBlock &MBB);
    void SandboxLoadStore(MachineBasicBlock &MBB,
                      MachineBasicBlock::iterator MBBI,
                      MachineInstr &MI,
                      int AddrIdx);

    bool SandboxBranchesInBlock(MachineBasicBlock &MBB);
    bool SandboxStackChangesInBlock(MachineBasicBlock &MBB);

    void SandboxStackChange(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MBBI);
    void AlignAllJumpTargets(MachineFunction &MF);
  };
  char MipsNaClRewritePass::ID = 0;
}

static bool IsReturn(const MachineInstr &MI) {
  return (MI.getOpcode() == Mips::RET);
}

static bool IsIndirectJump(const MachineInstr &MI) {
  return (MI.getOpcode() == Mips::JR);
}

static bool IsIndirectCall(const MachineInstr &MI) {
  return (MI.getOpcode() == Mips::JALR);
}

static bool IsDirectCall(const MachineInstr &MI) {
  return ((MI.getOpcode() == Mips::JAL) || (MI.getOpcode() == Mips::BGEZAL)
       || (MI.getOpcode() == Mips::BLTZAL));
;
}

static bool IsStackMask(const MachineInstr &MI) {
  return (MI.getOpcode() == Mips::SFI_DATA_MASK);
}

static bool NeedSandboxStackChange(const MachineInstr &MI,
                                   const TargetRegisterInfo *TRI) {
  if (IsDirectCall(MI) || IsIndirectCall(MI)) {
    // We check this first because method modifiesRegister
    // returns true for calls.
    return false;
  }
  return (MI.modifiesRegister(Mips::SP, TRI) && !IsStackMask(MI));
}

void MipsNaClRewritePass::SandboxStackChange(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator MBBI) {
  MachineInstr &MI = *MBBI;

  BuildMI(MBB, MBBI, MI.getDebugLoc(),
          TII->get(Mips::SFI_NOP_IF_AT_BUNDLE_END));

  // Get to next instr (one + to get the original, and one more + to get past).
  MachineBasicBlock::iterator MBBINext = (MBBI++);
  (void) MBBINext;
  MachineBasicBlock::iterator MBBINext2 = (MBBI++);

  BuildMI(MBB, MBBINext2, MI.getDebugLoc(),
          TII->get(Mips::SFI_DATA_MASK), Mips::SP)
          .addReg(Mips::SP)
          .addReg(Mips::LoadStoreStackMaskReg);
  return;
}

bool MipsNaClRewritePass::SandboxStackChangesInBlock(MachineBasicBlock &MBB) {
  bool Modified = false;
  for (MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
       MBBI != E; ++MBBI) {
    MachineInstr &MI = *MBBI;
    if (NeedSandboxStackChange(MI, TRI)) {
      SandboxStackChange(MBB, MBBI);
      Modified = true;
    }
  }
  return Modified;
}

bool MipsNaClRewritePass::SandboxBranchesInBlock(MachineBasicBlock &MBB) {
  bool Modified = false;

  for (MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
      MBBI != E; ++MBBI) {
    MachineInstr &MI = *MBBI;

    if (IsReturn(MI)) {
      unsigned AddrReg = MI.getOperand(0).getReg();
      BuildMI(MBB, MBBI, MI.getDebugLoc(),
              TII->get(Mips::SFI_GUARD_RETURN), AddrReg)
          .addReg(AddrReg)
          .addReg(Mips::IndirectBranchMaskReg);
      Modified = true;
    } else if (IsIndirectJump(MI)) {
      unsigned AddrReg = MI.getOperand(0).getReg();
      BuildMI(MBB, MBBI, MI.getDebugLoc(),
              TII->get(Mips::SFI_GUARD_INDIRECT_JMP), AddrReg)
          .addReg(AddrReg)
          .addReg(Mips::IndirectBranchMaskReg);
      Modified = true;
    } else if (IsDirectCall(MI)) {
      BuildMI(MBB, MBBI, MI.getDebugLoc(),
              TII->get(Mips::SFI_GUARD_CALL));
      Modified = true;
    } else if (IsIndirectCall(MI)) {
      unsigned AddrReg = MI.getOperand(0).getReg();
      BuildMI(MBB, MBBI, MI.getDebugLoc(),
              TII->get(Mips::SFI_GUARD_INDIRECT_CALL), AddrReg)
          .addReg(AddrReg)
          .addReg(Mips::IndirectBranchMaskReg);
      Modified = true;
    }
  }

  return Modified;
}

/*
 * Sandboxes a load or store instruction by inserting an appropriate mask
 * operation before it.
 */
void MipsNaClRewritePass::SandboxLoadStore(MachineBasicBlock &MBB,
                                      MachineBasicBlock::iterator MBBI,
                                      MachineInstr &MI,
                                      int AddrIdx) {
  unsigned BaseReg = MI.getOperand(AddrIdx).getReg();

  BuildMI(MBB, MBBI, MI.getDebugLoc(),
          TII->get(Mips::SFI_GUARD_LOADSTORE), BaseReg)
      .addReg(BaseReg)
      .addReg(Mips::LoadStoreStackMaskReg);
  return;
}

bool IsDangerousLoad(const MachineInstr &MI, int *AddrIdx) {
  unsigned Opcode = MI.getOpcode();
  switch (Opcode) {
  default: return false;

  // Instructions with base address register in position 1
  case Mips::LB:
  case Mips::LBu:
  case Mips::LH:
  case Mips::LHu:
  case Mips::LW:
  case Mips::LWC1:
  case Mips::LDC1:
  case Mips::LL:
  case Mips::LWL:
  case Mips::LWR:
    *AddrIdx = 1;
    break;
  }

  switch (MI.getOperand(*AddrIdx).getReg()) {
    default: break;
    // The contents of SP and thread pointer register do not require masking.
    case Mips::SP:
    case Mips::T8:
      return false;
  }

  return true;
}

bool IsDangerousStore(const MachineInstr &MI, int *AddrIdx) {
  unsigned Opcode = MI.getOpcode();
  switch (Opcode) {
  default: return false;

  // Instructions with base address register in position 1
  case Mips::SB:
  case Mips::SH:
  case Mips::SW:
  case Mips::SWC1:
  case Mips::SDC1:
  case Mips::SWL:
  case Mips::SWR:
    *AddrIdx = 1;
    break;

  case Mips::SC:
    *AddrIdx = 2;
    break;
  }

  switch (MI.getOperand(*AddrIdx).getReg()) {
    default: break;
    // The contents of SP and thread pointer register do not require masking.
    case Mips::SP:
    case Mips::T8:
      return false;
  }

  return true;
}

bool MipsNaClRewritePass::SandboxLoadsInBlock(MachineBasicBlock &MBB) {
  bool Modified = false;
  for (MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
       MBBI != E;
       ++MBBI) {
    MachineInstr &MI = *MBBI;
    int AddrIdx;

    if (IsDangerousLoad(MI, &AddrIdx)) {
      SandboxLoadStore(MBB, MBBI, MI, AddrIdx);
      Modified = true;
    }
  }
  return Modified;
}

bool MipsNaClRewritePass::SandboxStoresInBlock(MachineBasicBlock &MBB) {
  bool Modified = false;
  for (MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
       MBBI != E;
       ++MBBI) {
    MachineInstr &MI = *MBBI;
    int AddrIdx;

    if (IsDangerousStore(MI, &AddrIdx)) {
      SandboxLoadStore(MBB, MBBI, MI, AddrIdx);
      Modified = true;
    }
  }
  return Modified;
}

// Make sure all jump targets are aligned
void MipsNaClRewritePass::AlignAllJumpTargets(MachineFunction &MF) {
  // JUMP TABLE TARGETS
  MachineJumpTableInfo *jt_info = MF.getJumpTableInfo();
  if (jt_info) {
    const std::vector<MachineJumpTableEntry> &JT = jt_info->getJumpTables();
    for (unsigned i=0; i < JT.size(); ++i) {
      std::vector<MachineBasicBlock*> MBBs = JT[i].MBBs;

      for (unsigned j=0; j < MBBs.size(); ++j) {
        MBBs[j]->setAlignment(4);
      }
    }
  }

  for (MachineFunction::iterator I = MF.begin(), E = MF.end();
                           I != E; ++I) {
    MachineBasicBlock &MBB = *I;
    if (MBB.hasAddressTaken())
      MBB.setAlignment(4);
  }
}

bool MipsNaClRewritePass::runOnMachineFunction(MachineFunction &MF) {
  TII = static_cast<const MipsInstrInfo*>(MF.getTarget().getInstrInfo());
  TRI = MF.getTarget().getRegisterInfo();

  bool Modified = false;
  for (MachineFunction::iterator MFI = MF.begin(), E = MF.end();
       MFI != E;
       ++MFI) {
    MachineBasicBlock &MBB = *MFI;

    if (FlagSfiLoad)
      Modified |= SandboxLoadsInBlock(MBB);
    if (FlagSfiStore)
      Modified |= SandboxStoresInBlock(MBB);
    if (FlagSfiBranch)
      Modified |= SandboxBranchesInBlock(MBB);
    if (FlagSfiStack)
      Modified |= SandboxStackChangesInBlock(MBB);
  }

  if (FlagSfiBranch)
    AlignAllJumpTargets(MF);

  return Modified;
}

/// createMipsNaClRewritePass - returns an instance of the NaClRewritePass.
FunctionPass *llvm::createMipsNaClRewritePass() {
  return new MipsNaClRewritePass();
}
