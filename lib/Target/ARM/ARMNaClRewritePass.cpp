//===-- ARMNaClRewritePass.cpp - Native Client Rewrite Pass  ------*- C++ -*-=//
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

#define DEBUG_TYPE "arm-sfi"
#include "ARM.h"
#include "ARMBaseInstrInfo.h"
#include "ARMNaClRewritePass.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/Function.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include <set>
#include <stdio.h>

using namespace llvm;

namespace llvm {

cl::opt<bool>
FlagSfiData("sfi-data", cl::desc("use illegal at data bundle beginning"));

cl::opt<bool>
FlagSfiLoad("sfi-load", cl::desc("enable sandboxing for load"));

cl::opt<bool>
FlagSfiStore("sfi-store", cl::desc("enable sandboxing for stores"));

cl::opt<bool>
FlagSfiStack("sfi-stack", cl::desc("enable sandboxing for stack changes"));

cl::opt<bool>
FlagSfiBranch("sfi-branch", cl::desc("enable sandboxing for branches"));

}

namespace {
  class ARMNaClRewritePass : public MachineFunctionPass {
  public:
    static char ID;
    ARMNaClRewritePass() : MachineFunctionPass(ID) {}

    const ARMBaseInstrInfo *TII;
    const TargetRegisterInfo *TRI;
    virtual void getAnalysisUsage(AnalysisUsage &AU) const;
    virtual bool runOnMachineFunction(MachineFunction &Fn);

    virtual const char *getPassName() const {
      return "ARM Native Client Rewrite Pass";
    }

  private:

    bool SandboxMemoryReferencesInBlock(MachineBasicBlock &MBB);
    void SandboxMemory(MachineBasicBlock &MBB,
                       MachineBasicBlock::iterator MBBI,
                       MachineInstr &MI,
                       int AddrIdx,
                       bool CPSRLive,
                       bool IsLoad);
    bool TryPredicating(MachineInstr &MI, ARMCC::CondCodes);

    bool SandboxBranchesInBlock(MachineBasicBlock &MBB);
    bool SandboxStackChangesInBlock(MachineBasicBlock &MBB);

    void SandboxStackChange(MachineBasicBlock &MBB,
                              MachineBasicBlock::iterator MBBI);
    void LightweightVerify(MachineFunction &MF);
  };
  char ARMNaClRewritePass::ID = 0;
}

static bool IsReturn(const MachineInstr &MI) {
  return (MI.getOpcode() == ARM::BX_RET);
}

static bool IsIndirectJump(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
   default: return false;
   case ARM::BX:
   case ARM::TAILJMPr:
    return true;
  }
}

static bool IsIndirectCall(const MachineInstr &MI) {
  return MI.getOpcode() == ARM::BLX;
}

static bool IsDirectCall(const MachineInstr &MI) {
  switch (MI.getOpcode()) {
   default: return false;
   case ARM::BL:
   case ARM::BL_pred:
   case ARM::TPsoft:
     return true;
  }
}

static bool IsCPSRLiveOut(const MachineBasicBlock &MBB) {
  // CPSR is live-out if any successor lists it as live-in.
  for (MachineBasicBlock::const_succ_iterator SI = MBB.succ_begin(),
                                              E = MBB.succ_end();
       SI != E;
       ++SI) {
    const MachineBasicBlock *Succ = *SI;
    if (Succ->isLiveIn(ARM::CPSR)) return true;
  }
  return false;
}

static void DumpInstructionVerbose(const MachineInstr &MI) {
  dbgs() << MI;
  dbgs() << MI.getNumOperands() << " operands:" << "\n";
  for (unsigned i = 0; i < MI.getNumOperands(); ++i) {
    const MachineOperand& op = MI.getOperand(i);
    dbgs() << "  " << i << "(" << op.getType() << "):" << op << "\n";
  }
  dbgs() << "\n";
}

static void DumpBasicBlockVerbose(const MachineBasicBlock &MBB) {
  dbgs() << "\n<<<<< DUMP BASIC BLOCK START\n";
  for (MachineBasicBlock::const_iterator MBBI = MBB.begin(), MBBE = MBB.end();
       MBBI != MBBE;
       ++MBBI) {
    DumpInstructionVerbose(*MBBI);
  }
  dbgs() << "<<<<< DUMP BASIC BLOCK END\n\n";
}

static void DumpBasicBlockVerboseCond(const MachineBasicBlock &MBB, bool b) {
  if (b) {
    DumpBasicBlockVerbose(MBB);
  }
}

/**********************************************************************/
/* Exported functions */

namespace ARM_SFI {

bool IsStackChange(const MachineInstr &MI, const TargetRegisterInfo *TRI) {
  return MI.modifiesRegister(ARM::SP, TRI);
}

bool NextInstrMasksSP(const MachineInstr &MI) {
  MachineBasicBlock::const_iterator It = &MI;
  const MachineBasicBlock *MBB = MI.getParent();

  MachineBasicBlock::const_iterator next = ++It;
  if (next == MBB->end()) {
    return false;
  }

  const MachineInstr &next_instr = *next;
  unsigned opcode = next_instr.getOpcode();
  return (opcode == ARM::SFI_DATA_MASK) &&
      (next_instr.getOperand(0).getReg() == ARM::SP);
}

bool IsSandboxedStackChange(const MachineInstr &MI) {
  // Calls do not change the stack on ARM but they have implicit-defs, so
  // make sure they do not get sandboxed.
  if (MI.getDesc().isCall())
    return true;

  unsigned opcode = MI.getOpcode();
  switch (opcode) {
    default: break;

    // These just bump SP by a little (and access the stack),
    // so that is okay due to guard pages.
    case ARM::STMIA_UPD:
    case ARM::STMDA_UPD:
    case ARM::STMDB_UPD:
    case ARM::STMIB_UPD:

    case ARM::VSTMDIA_UPD:
    case ARM::VSTMDDB_UPD:
    case ARM::VSTMSIA_UPD:
    case ARM::VSTMSDB_UPD:
      return true;

    // Similar, unless it is a load into SP...
    case ARM::LDMIA_UPD:
    case ARM::LDMDA_UPD:
    case ARM::LDMDB_UPD:
    case ARM::LDMIB_UPD:

    case ARM::VLDMDIA_UPD:
    case ARM::VLDMDDB_UPD:
    case ARM::VLDMSIA_UPD:
    case ARM::VLDMSDB_UPD: {
      bool dest_SP = false;
      // Dest regs start at operand index 4.
      for (unsigned i = 4; i < MI.getNumOperands(); ++i) {
        const MachineOperand &DestReg = MI.getOperand(i);
        dest_SP = dest_SP || (DestReg.getReg() == ARM::SP);
      }
      if (dest_SP) {
        break;
      }
      return true;
    }

    // Some localmods *should* prevent selecting a reg offset
    // (see SelectAddrMode2 in ARMISelDAGToDAG.cpp).
    // Otherwise, the store is already a potential violation.
    case ARM::STR_PRE_REG:
    case ARM::STR_PRE_IMM:

    case ARM::STRH_PRE:

    case ARM::STRB_PRE_REG:
    case ARM::STRB_PRE_IMM:
      return true;

    // Similar, unless it is a load into SP...
    case ARM::LDRi12:
    case ARM::LDR_PRE_REG:
    case ARM::LDR_PRE_IMM:
    case ARM::LDRH_PRE:
    case ARM::LDRB_PRE_REG:
    case ARM::LDRB_PRE_IMM:
    case ARM::LDRSH_PRE:
    case ARM::LDRSB_PRE: {
      const MachineOperand &DestReg = MI.getOperand(0);
      if (DestReg.getReg() == ARM::SP) {
        break;
      }
      return true;
    }

    // Here, if SP is the base / write-back reg, we need to check if
    // a reg is used as offset (otherwise it is not a small nudge).
    case ARM::STR_POST_REG:
    case ARM::STR_POST_IMM:
    case ARM::STRH_POST:
    case ARM::STRB_POST_REG:
    case ARM::STRB_POST_IMM: {
      const MachineOperand &WBReg = MI.getOperand(0);
      const MachineOperand &OffReg = MI.getOperand(3);
      if (WBReg.getReg() == ARM::SP && OffReg.getReg() != 0) {
        break;
      }
      return true;
    }

    // Similar, but also check that DestReg is not SP.
    case ARM::LDR_POST_REG:
    case ARM::LDR_POST_IMM:
    case ARM::LDRB_POST_REG:
    case ARM::LDRB_POST_IMM:
    case ARM::LDRH_POST:
    case ARM::LDRSH_POST:
    case ARM::LDRSB_POST: {
      const MachineOperand &DestReg = MI.getOperand(0);
      if (DestReg.getReg() == ARM::SP) {
        break;
      }
      const MachineOperand &WBReg = MI.getOperand(1);
      const MachineOperand &OffReg = MI.getOperand(3);
      if (WBReg.getReg() == ARM::SP && OffReg.getReg() != 0) {
        break;
      }
      return true;
    }
  }

  return (NextInstrMasksSP(MI));
}

bool NeedSandboxStackChange(const MachineInstr &MI,
                               const TargetRegisterInfo *TRI) {
  return (IsStackChange(MI, TRI) && !IsSandboxedStackChange(MI));
}

} // namespace ARM_SFI

/**********************************************************************/

void ARMNaClRewritePass::getAnalysisUsage(AnalysisUsage &AU) const {
  // Slight (possibly unnecessary) efficiency tweak:
  // Promise not to modify the CFG.
  AU.setPreservesCFG();
  MachineFunctionPass::getAnalysisUsage(AU);
}

/*
 * A primitive validator to catch problems at compile time.
 * E.g., it could be used along with bugpoint to reduce a bitcode file.
 */
void ARMNaClRewritePass::LightweightVerify(MachineFunction &MF) {

  for (MachineFunction::iterator MFI = MF.begin(), MFE = MF.end();
       MFI != MFE;
       ++MFI) {
    MachineBasicBlock &MBB = *MFI;
    for (MachineBasicBlock::iterator MBBI = MBB.begin(), MBBE = MBB.end();
         MBBI != MBBE;
         ++MBBI) {
      MachineInstr &MI = *MBBI;

      if (ARM_SFI::NeedSandboxStackChange(MI, TRI)) {
        dbgs() << "LightWeightVerify for function: "
               << MF.getFunction()->getName() << "  (BAD STACK CHANGE)\n";
        DumpInstructionVerbose(MI);
        DumpBasicBlockVerbose(MBB);
        //        assert(false && "LightweightVerify Failed");
      }
    }
  }
}

void ARMNaClRewritePass::SandboxStackChange(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator MBBI) {
  // (1) Ensure there is room in the bundle for a data mask instruction
  // (nop'ing to the next bundle if needed).
  // (2) Do a data mask on SP after the instruction that updated SP.
  MachineInstr &MI = *MBBI;

  // Use same predicate as current instruction.
  ARMCC::CondCodes Pred = TII->getPredicate(&MI);

  BuildMI(MBB, MBBI, MI.getDebugLoc(),
          TII->get(ARM::SFI_NOP_IF_AT_BUNDLE_END));

  // Get to next instr (one + to get the original, and one more + to get past)
  MachineBasicBlock::iterator MBBINext = (MBBI++);
  MachineBasicBlock::iterator MBBINext2 = (MBBI++);

  BuildMI(MBB, MBBINext2, MI.getDebugLoc(),
          TII->get(ARM::SFI_DATA_MASK))
      .addReg(ARM::SP)         // modify SP (as dst)
      .addReg(ARM::SP)         // start with SP (as src)
      .addImm((int64_t) Pred)  // predicate condition
      .addReg(ARM::CPSR);      // predicate source register (CPSR)

  return;
}

bool ARMNaClRewritePass::SandboxStackChangesInBlock(MachineBasicBlock &MBB) {
  bool Modified = false;
  for (MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
       MBBI != E;
       ++MBBI) {
    MachineInstr &MI = *MBBI;
    if (ARM_SFI::NeedSandboxStackChange(MI, TRI)) {
      SandboxStackChange(MBB, MBBI);
      Modified |= true;
    }
  }
  return Modified;
}

bool ARMNaClRewritePass::SandboxBranchesInBlock(MachineBasicBlock &MBB) {
  bool Modified = false;

  for (MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
       MBBI != E;
       ++MBBI) {
    MachineInstr &MI = *MBBI;

    if (IsReturn(MI)) {
      ARMCC::CondCodes Pred = TII->getPredicate(&MI);
      BuildMI(MBB, MBBI, MI.getDebugLoc(),
              TII->get(ARM::SFI_GUARD_RETURN))
        .addImm((int64_t) Pred)  // predicate condition
        .addReg(ARM::CPSR);      // predicate source register (CPSR)
      Modified = true;
    }

    if (IsIndirectJump(MI)) {
      MachineOperand &Addr = MI.getOperand(0);
      ARMCC::CondCodes Pred = TII->getPredicate(&MI);
      BuildMI(MBB, MBBI, MI.getDebugLoc(),
              TII->get(ARM::SFI_GUARD_INDIRECT_JMP))
        .addOperand(Addr)        // rD
        .addReg(0)               // apparently unused source register?
        .addImm((int64_t) Pred)  // predicate condition
        .addReg(ARM::CPSR);      // predicate source register (CPSR)
      Modified = true;
    }

    if (IsDirectCall(MI)) {
      ARMCC::CondCodes Pred = TII->getPredicate(&MI);
      BuildMI(MBB, MBBI, MI.getDebugLoc(),
              TII->get(ARM::SFI_GUARD_CALL))
        .addImm((int64_t) Pred)  // predicate condition
        .addReg(ARM::CPSR);      // predicate source register (CPSR)
      Modified = true;
    }

    if (IsIndirectCall(MI)) {
      MachineOperand &Addr = MI.getOperand(0);
      ARMCC::CondCodes Pred = TII->getPredicate(&MI);
      BuildMI(MBB, MBBI, MI.getDebugLoc(),
              TII->get(ARM::SFI_GUARD_INDIRECT_CALL))
        .addOperand(Addr)        // rD
        .addReg(0)               // apparently unused source register?
        .addImm((int64_t) Pred)  // predicate condition
        .addReg(ARM::CPSR);      // predicate source register (CPSR)
        Modified = true;
    }
  }

  return Modified;
}

bool ARMNaClRewritePass::TryPredicating(MachineInstr &MI, ARMCC::CondCodes Pred) {
  // Can't predicate if it's already predicated.
  // TODO(cbiffle): actually we can, if the conditions match.
  if (TII->isPredicated(&MI)) return false;

  /*
   * ARM predicate operands use two actual MachineOperands: an immediate
   * holding the predicate condition, and a register referencing the flags.
   */
  SmallVector<MachineOperand, 2> PredOperands;
  PredOperands.push_back(MachineOperand::CreateImm((int64_t) Pred));
  PredOperands.push_back(MachineOperand::CreateReg(ARM::CPSR, false));

  // This attempts to rewrite, but some instructions can't be predicated.
  return TII->PredicateInstruction(&MI, PredOperands);
}

static bool IsDangerousLoad(const MachineInstr &MI, int *AddrIdx) {
  unsigned Opcode = MI.getOpcode();
  switch (Opcode) {
  default: return false;

  // Instructions with base address register in position 0...
  case ARM::LDMIA:
  case ARM::LDMDA:
  case ARM::LDMDB:
  case ARM::LDMIB:

  case ARM::VLDMDIA:
  case ARM::VLDMSIA:
    *AddrIdx = 0;
    break;
  // Instructions with base address register in position 1...
  case ARM::LDMIA_UPD: // same reg at position 0 and position 1
  case ARM::LDMDA_UPD:
  case ARM::LDMDB_UPD:
  case ARM::LDMIB_UPD:

  case ARM::LDRSB:
  case ARM::LDRH:
  case ARM::LDRSH:

  case ARM::LDRi12:
  case ARM::LDRrs:
  case ARM::LDRBi12:
  case ARM::LDRBrs:
  case ARM::VLDMDIA_UPD:
  case ARM::VLDMDDB_UPD:
  case ARM::VLDMSIA_UPD:
  case ARM::VLDMSDB_UPD:
  case ARM::VLDRS:
  case ARM::VLDRD:

  case ARM::LDREX:
  case ARM::LDREXB:
  case ARM::LDREXH:
    *AddrIdx = 1;
    break;

  // Instructions with base address register in position 2...
  case ARM::LDR_PRE_REG:
  case ARM::LDR_PRE_IMM:
  case ARM::LDR_POST_REG:
  case ARM::LDR_POST_IMM:

  case ARM::LDRB_PRE_REG:
  case ARM::LDRB_PRE_IMM:
  case ARM::LDRB_POST_REG:
  case ARM::LDRB_POST_IMM:
  case ARM::LDRSB_PRE:
  case ARM::LDRSB_POST:

  case ARM::LDRH_PRE:
  case ARM::LDRH_POST:
  case ARM::LDRSH_PRE:
  case ARM::LDRSH_POST:

  case ARM::LDRD:
    *AddrIdx = 2;
    break;
  }

  if (MI.getOperand(*AddrIdx).getReg() == ARM::SP) {
    // The contents of SP do not require masking.
    return false;
  }

  return true;
}

/*
 * Sandboxes a memory reference instruction by inserting an appropriate mask
 * or check operation before it.
 */
void ARMNaClRewritePass::SandboxMemory(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator MBBI,
                                       MachineInstr &MI,
                                       int AddrIdx,
                                       bool CPSRLive,
                                       bool IsLoad) {
  MachineOperand &Addr = MI.getOperand(AddrIdx);

  if (!CPSRLive && TryPredicating(MI, ARMCC::EQ)) {
    /*
     * For unconditional memory references where CPSR is not in use, we can use
     * a faster sandboxing sequence by predicating the load/store -- assuming we
     * *can* predicate the load/store.
     */

    // TODO(sehr): add SFI_GUARD_SP_LOAD_TST.
    // Instruction can be predicated -- use the new sandbox.
    BuildMI(MBB, MBBI, MI.getDebugLoc(),
            TII->get(ARM::SFI_GUARD_LOADSTORE_TST))
      .addOperand(Addr)   // rD
      .addReg(0);         // apparently unused source register?
  } else {
    unsigned Opcode;
    if (IsLoad && (MI.getOperand(0).getReg() == ARM::SP)) {
      Opcode = ARM::SFI_GUARD_SP_LOAD;
    } else {
      Opcode = ARM::SFI_GUARD_LOADSTORE;
    }
    // Use the older BIC sandbox, which is universal, but incurs a stall.
    ARMCC::CondCodes Pred = TII->getPredicate(&MI);
    BuildMI(MBB, MBBI, MI.getDebugLoc(), TII->get(Opcode))
      .addOperand(Addr)        // rD
      .addReg(0)               // apparently unused source register?
      .addImm((int64_t) Pred)  // predicate condition
      .addReg(ARM::CPSR);      // predicate source register (CPSR)

    /*
     * This pseudo-instruction is intended to generate something resembling the
     * following, but with alignment enforced.
     * TODO(cbiffle): move alignment into this function, use the code below.
     *
     *  // bic<cc> Addr, Addr, #0xC0000000
     *  BuildMI(MBB, MBBI, MI.getDebugLoc(),
     *          TII->get(ARM::BICri))
     *    .addOperand(Addr)        // rD
     *    .addOperand(Addr)        // rN
     *    .addImm(0xC0000000)      // imm
     *    .addImm((int64_t) Pred)  // predicate condition
     *    .addReg(ARM::CPSR)       // predicate source register (CPSR)
     *    .addReg(0);              // flag output register (0 == no flags)
     */
  }
}

static bool IsDangerousStore(const MachineInstr &MI, int *AddrIdx) {
  unsigned Opcode = MI.getOpcode();
  switch (Opcode) {
  default: return false;

  // Instructions with base address register in position 0...
  case ARM::STMIA:
  case ARM::STMDA:
  case ARM::STMDB:
  case ARM::STMIB:

  case ARM::VSTMDIA:
  case ARM::VSTMSIA:
    *AddrIdx = 0;
    break;

  // Instructions with base address register in position 1...
  case ARM::STMIA_UPD: // same reg at position 0 and position 1
  case ARM::STMDA_UPD:
  case ARM::STMDB_UPD:
  case ARM::STMIB_UPD:

  case ARM::STRH:
  case ARM::STRi12:
  case ARM::STRrs:
  case ARM::STRBi12:
  case ARM::STRBrs:
  case ARM::VSTMDIA_UPD:
  case ARM::VSTMDDB_UPD:
  case ARM::VSTMSIA_UPD:
  case ARM::VSTMSDB_UPD:
  case ARM::VSTRS:
  case ARM::VSTRD:
    *AddrIdx = 1;
    break;

  // Instructions with base address register in position 2...
  case ARM::STR_PRE_REG:
  case ARM::STR_PRE_IMM:
  case ARM::STR_POST_REG:
  case ARM::STR_POST_IMM:

  case ARM::STRB_PRE_REG:
  case ARM::STRB_PRE_IMM:
  case ARM::STRB_POST_REG:
  case ARM::STRB_POST_IMM:

  case ARM::STRH_PRE:
  case ARM::STRH_POST:


  case ARM::STRD:
  case ARM::STREX:
  case ARM::STREXB:
  case ARM::STREXH:
    *AddrIdx = 2;
    break;
  }

  if (MI.getOperand(*AddrIdx).getReg() == ARM::SP) {
    // The contents of SP do not require masking.
    return false;
  }

  return true;
}

bool ARMNaClRewritePass::SandboxMemoryReferencesInBlock(
    MachineBasicBlock &MBB) {
  /*
   * This is a simple local reverse-dataflow analysis to determine where CPSR
   * is live.  We cannot use the conditional store sequence anywhere that CPSR
   * is live, or we'd affect correctness.  The existing liveness analysis passes
   * barf when applied pre-emit, after allocation, so we must do it ourselves.
   */

  // LOCALMOD(pdox): Short-circuit this function. Assume CPSR is always live,
  //                 until we figure out why the assert is tripping.
  bool Modified2 = false;
  for (MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
       MBBI != E;
       ++MBBI) {
    MachineInstr &MI = *MBBI;
    int AddrIdx;

    if (FlagSfiLoad && IsDangerousLoad(MI, &AddrIdx)) {
      bool CPSRLive = true;
      SandboxMemory(MBB, MBBI, MI, AddrIdx, CPSRLive, true);
      Modified2 = true;
    }
    if (FlagSfiStore && IsDangerousStore(MI, &AddrIdx)) {
      bool CPSRLive = true;
      SandboxMemory(MBB, MBBI, MI, AddrIdx, CPSRLive, false);
      Modified2 = true;
    }
  }
  return Modified2;
  // END LOCALMOD(pdox)

  bool CPSRLive = IsCPSRLiveOut(MBB);

  // Given that, record which instructions should not be altered to trash CPSR:
  std::set<const MachineInstr *> InstrsWhereCPSRLives;
  for (MachineBasicBlock::const_reverse_iterator MBBI = MBB.rbegin(),
                                                 E = MBB.rend();
       MBBI != E;
       ++MBBI) {
    const MachineInstr &MI = *MBBI;
    // Check for kills first.
    if (MI.modifiesRegister(ARM::CPSR, TRI)) CPSRLive = false;
    // Then check for uses.
    if (MI.readsRegister(ARM::CPSR)) CPSRLive = true;

    if (CPSRLive) InstrsWhereCPSRLives.insert(&MI);
  }

  // Sanity check:
  assert(CPSRLive == MBB.isLiveIn(ARM::CPSR)
         && "CPSR Liveness analysis does not match cached live-in result.");

  // Now: find and sandbox stores.
  bool Modified = false;
  for (MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
       MBBI != E;
       ++MBBI) {
    MachineInstr &MI = *MBBI;
    int AddrIdx;

    if (FlagSfiLoad && IsDangerousLoad(MI, &AddrIdx)) {
      bool CPSRLive =
        (InstrsWhereCPSRLives.find(&MI) != InstrsWhereCPSRLives.end());
      SandboxMemory(MBB, MBBI, MI, AddrIdx, CPSRLive, true);
      Modified = true;
    }
    if (FlagSfiStore && IsDangerousStore(MI, &AddrIdx)) {
      bool CPSRLive =
        (InstrsWhereCPSRLives.find(&MI) != InstrsWhereCPSRLives.end());
      SandboxMemory(MBB, MBBI, MI, AddrIdx, CPSRLive, false);
      Modified = true;
    }
  }

  return Modified;
}

/**********************************************************************/

bool ARMNaClRewritePass::runOnMachineFunction(MachineFunction &MF) {
  TII = static_cast<const ARMBaseInstrInfo*>(MF.getTarget().getInstrInfo());
  TRI = MF.getTarget().getRegisterInfo();

  bool Modified = false;
  for (MachineFunction::iterator MFI = MF.begin(), E = MF.end();
       MFI != E;
       ++MFI) {
    MachineBasicBlock &MBB = *MFI;

    if (MBB.hasAddressTaken()) {
      //FIXME: use symbolic constant or get this value from some configuration
      MBB.setAlignment(4);
      Modified = true;
    }

    if (FlagSfiLoad || FlagSfiStore)
      Modified |= SandboxMemoryReferencesInBlock(MBB);
    if (FlagSfiBranch) Modified |= SandboxBranchesInBlock(MBB);
    if (FlagSfiStack)  Modified |= SandboxStackChangesInBlock(MBB);
  }
  DEBUG(LightweightVerify(MF));
  return Modified;
}

/// createARMNaClRewritePass - returns an instance of the NaClRewritePass.
FunctionPass *llvm::createARMNaClRewritePass() {
  return new ARMNaClRewritePass();
}
