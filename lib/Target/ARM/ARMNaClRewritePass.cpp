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
#include "llvm/IR/Function.h"
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

cl::opt<bool>
FlagNaClUseM23ArmAbi("nacl-use-m23-arm-abi",
                     cl::desc("use the Chrome M23 ARM ABI"));

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
                       bool IsLoad);

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

static void DumpBasicBlockVerbose(const MachineBasicBlock &MBB) {
  DEBUG({
      dbgs() << "\n<<<<< DUMP BASIC BLOCK START\n";
      for (MachineBasicBlock::const_iterator
               MBBI = MBB.begin(), MBBE = MBB.end();
           MBBI != MBBE;
           ++MBBI) {
        DumpInstructionVerbose(*MBBI);
      }
      dbgs() << "<<<<< DUMP BASIC BLOCK END\n\n";
    });
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

    // Our mask instructions correctly update the stack pointer.
    case ARM::SFI_DATA_MASK:
      return true;

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
  DEBUG({
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
          }
        }
      }
    });
}

void ARMNaClRewritePass::SandboxStackChange(MachineBasicBlock &MBB,
                                       MachineBasicBlock::iterator MBBI) {
  // (1) Ensure there is room in the bundle for a data mask instruction
  // (nop'ing to the next bundle if needed).
  // (2) Do a data mask on SP after the instruction that updated SP.
  MachineInstr &MI = *MBBI;

  // Use same predicate as current instruction.
  unsigned PredReg = 0;
  ARMCC::CondCodes Pred = llvm::getInstrPredicate(&MI, PredReg);

  BuildMI(MBB, MBBI, MI.getDebugLoc(),
          TII->get(ARM::SFI_NOP_IF_AT_BUNDLE_END));

  // Get to next instr.
  MachineBasicBlock::iterator MBBINext = (++MBBI);

  BuildMI(MBB, MBBINext, MI.getDebugLoc(),
          TII->get(ARM::SFI_DATA_MASK))
      .addReg(ARM::SP, RegState::Define)  // modify SP (as dst)
      .addReg(ARM::SP, RegState::Kill)    // start with SP (as src)
      .addImm((int64_t) Pred)             // predicate condition
      .addReg(PredReg);                   // predicate source register (CPSR)
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
    // Use same predicate as current instruction.
    unsigned PredReg = 0;
    ARMCC::CondCodes Pred = llvm::getInstrPredicate(&MI, PredReg);

    if (IsReturn(MI)) {
      BuildMI(MBB, MBBI, MI.getDebugLoc(),
              TII->get(ARM::SFI_GUARD_RETURN))
        .addImm((int64_t) Pred)  // predicate condition
        .addReg(PredReg);        // predicate source register (CPSR)
      Modified = true;
    }

    if (IsIndirectJump(MI)) {
      unsigned Addr = MI.getOperand(0).getReg();
      BuildMI(MBB, MBBI, MI.getDebugLoc(),
              TII->get(ARM::SFI_GUARD_INDIRECT_JMP))
        .addReg(Addr, RegState::Define)  // Destination definition (as dst)
        .addReg(Addr, RegState::Kill)    // Destination read (as src)
        .addImm((int64_t) Pred)          // predicate condition
        .addReg(PredReg);                // predicate source register (CPSR)
      Modified = true;
    }

    if (IsDirectCall(MI)) {
      BuildMI(MBB, MBBI, MI.getDebugLoc(),
              TII->get(ARM::SFI_GUARD_CALL))
        .addImm((int64_t) Pred)  // predicate condition
        .addReg(PredReg);        // predicate source register (CPSR)
      Modified = true;
    }

    if (IsIndirectCall(MI)) {
      unsigned Addr = MI.getOperand(0).getReg();
      BuildMI(MBB, MBBI, MI.getDebugLoc(),
              TII->get(ARM::SFI_GUARD_INDIRECT_CALL))
        .addReg(Addr, RegState::Define)  // Destination definition (as dst)
        .addReg(Addr, RegState::Kill)    // Destination read (as src)
        .addImm((int64_t) Pred)          // predicate condition
        .addReg(PredReg);                // predicate source register (CPSR)
        Modified = true;
    }
  }

  return Modified;
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

  case ARM::PLDi12:
  case ARM::PLDWi12:
  case ARM::PLIi12:
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
  case ARM::LDREXD:
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

  //
  // NEON loads
  //

  // VLD1
  case ARM::VLD1d8:
  case ARM::VLD1d16:
  case ARM::VLD1d32:
  case ARM::VLD1d64:
  case ARM::VLD1q8:
  case ARM::VLD1q16:
  case ARM::VLD1q32:
  case ARM::VLD1q64:
    *AddrIdx = 1;
    break;

  case ARM::VLD1d8wb_fixed:
  case ARM::VLD1d16wb_fixed:
  case ARM::VLD1d32wb_fixed:
  case ARM::VLD1d64wb_fixed:
  case ARM::VLD1q8wb_fixed:
  case ARM::VLD1q16wb_fixed:
  case ARM::VLD1q32wb_fixed:
  case ARM::VLD1q64wb_fixed:
  case ARM::VLD1d8wb_register:
  case ARM::VLD1d16wb_register:
  case ARM::VLD1d32wb_register:
  case ARM::VLD1d64wb_register:
  case ARM::VLD1q8wb_register:
  case ARM::VLD1q16wb_register:
  case ARM::VLD1q32wb_register:
  case ARM::VLD1q64wb_register:
    *AddrIdx = 2;
    break;

  // VLD1T
  case ARM::VLD1d8T:
  case ARM::VLD1d16T:
  case ARM::VLD1d32T:
  case ARM::VLD1d64T:
    *AddrIdx = 1;
    break;

  case ARM::VLD1d8Twb_fixed:
  case ARM::VLD1d16Twb_fixed:
  case ARM::VLD1d32Twb_fixed:
  case ARM::VLD1d64Twb_fixed:
  case ARM::VLD1d8Twb_register:
  case ARM::VLD1d16Twb_register:
  case ARM::VLD1d32Twb_register:
  case ARM::VLD1d64Twb_register:
    *AddrIdx = 2;
    break;

  // VLD1Q
  case ARM::VLD1d8Q:
  case ARM::VLD1d16Q:
  case ARM::VLD1d32Q:
  case ARM::VLD1d64Q:
    *AddrIdx = 1;
    break;

  case ARM::VLD1d8Qwb_fixed:
  case ARM::VLD1d16Qwb_fixed:
  case ARM::VLD1d32Qwb_fixed:
  case ARM::VLD1d64Qwb_fixed:
  case ARM::VLD1d8Qwb_register:
  case ARM::VLD1d16Qwb_register:
  case ARM::VLD1d32Qwb_register:
  case ARM::VLD1d64Qwb_register:
    *AddrIdx = 2;
    break;

  // VLD1LN
  case ARM::VLD1LNd8:
  case ARM::VLD1LNd16:
  case ARM::VLD1LNd32:
  case ARM::VLD1LNd8_UPD:
  case ARM::VLD1LNd16_UPD:
  case ARM::VLD1LNd32_UPD:

  // VLD1DUP
  case ARM::VLD1DUPd8:
  case ARM::VLD1DUPd16:
  case ARM::VLD1DUPd32:
  case ARM::VLD1DUPq8:
  case ARM::VLD1DUPq16:
  case ARM::VLD1DUPq32:
  case ARM::VLD1DUPd8wb_fixed:
  case ARM::VLD1DUPd16wb_fixed:
  case ARM::VLD1DUPd32wb_fixed:
  case ARM::VLD1DUPq8wb_fixed:
  case ARM::VLD1DUPq16wb_fixed:
  case ARM::VLD1DUPq32wb_fixed:
  case ARM::VLD1DUPd8wb_register:
  case ARM::VLD1DUPd16wb_register:
  case ARM::VLD1DUPd32wb_register:
  case ARM::VLD1DUPq8wb_register:
  case ARM::VLD1DUPq16wb_register:
  case ARM::VLD1DUPq32wb_register:

  // VLD2
  case ARM::VLD2d8:
  case ARM::VLD2d16:
  case ARM::VLD2d32:
  case ARM::VLD2b8:
  case ARM::VLD2b16:
  case ARM::VLD2b32:
  case ARM::VLD2q8:
  case ARM::VLD2q16:
  case ARM::VLD2q32:
    *AddrIdx = 1;
    break;

  case ARM::VLD2d8wb_fixed:
  case ARM::VLD2d16wb_fixed:
  case ARM::VLD2d32wb_fixed:
  case ARM::VLD2b8wb_fixed:
  case ARM::VLD2b16wb_fixed:
  case ARM::VLD2b32wb_fixed:
  case ARM::VLD2q8wb_fixed:
  case ARM::VLD2q16wb_fixed:
  case ARM::VLD2q32wb_fixed:
  case ARM::VLD2d8wb_register:
  case ARM::VLD2d16wb_register:
  case ARM::VLD2d32wb_register:
  case ARM::VLD2b8wb_register:
  case ARM::VLD2b16wb_register:
  case ARM::VLD2b32wb_register:
  case ARM::VLD2q8wb_register:
  case ARM::VLD2q16wb_register:
  case ARM::VLD2q32wb_register:
    *AddrIdx = 2;
    break;

  // VLD2LN
  case ARM::VLD2LNd8:
  case ARM::VLD2LNd16:
  case ARM::VLD2LNd32:
  case ARM::VLD2LNq16:
  case ARM::VLD2LNq32:
    *AddrIdx = 2;
    break;

  case ARM::VLD2LNd8_UPD:
  case ARM::VLD2LNd16_UPD:
  case ARM::VLD2LNd32_UPD:
  case ARM::VLD2LNq16_UPD:
  case ARM::VLD2LNq32_UPD:
    *AddrIdx = 3;
    break;

  // VLD2DUP
  case ARM::VLD2DUPd8:
  case ARM::VLD2DUPd16:
  case ARM::VLD2DUPd32:
  case ARM::VLD2DUPd8x2:
  case ARM::VLD2DUPd16x2:
  case ARM::VLD2DUPd32x2:
    *AddrIdx = 1;
    break;

  case ARM::VLD2DUPd8wb_fixed:
  case ARM::VLD2DUPd16wb_fixed:
  case ARM::VLD2DUPd32wb_fixed:
  case ARM::VLD2DUPd8wb_register:
  case ARM::VLD2DUPd16wb_register:
  case ARM::VLD2DUPd32wb_register:
  case ARM::VLD2DUPd8x2wb_fixed:
  case ARM::VLD2DUPd16x2wb_fixed:
  case ARM::VLD2DUPd32x2wb_fixed:
  case ARM::VLD2DUPd8x2wb_register:
  case ARM::VLD2DUPd16x2wb_register:
  case ARM::VLD2DUPd32x2wb_register:
    *AddrIdx = 2;
    break;

  // VLD3
  case ARM::VLD3d8:
  case ARM::VLD3d16:
  case ARM::VLD3d32:
  case ARM::VLD3q8:
  case ARM::VLD3q16:
  case ARM::VLD3q32:
  case ARM::VLD3d8_UPD:
  case ARM::VLD3d16_UPD:
  case ARM::VLD3d32_UPD:
  case ARM::VLD3q8_UPD:
  case ARM::VLD3q16_UPD:
  case ARM::VLD3q32_UPD:

  // VLD3LN
  case ARM::VLD3LNd8:
  case ARM::VLD3LNd16:
  case ARM::VLD3LNd32:
  case ARM::VLD3LNq16:
  case ARM::VLD3LNq32:
    *AddrIdx = 3;
    break;

  case ARM::VLD3LNd8_UPD:
  case ARM::VLD3LNd16_UPD:
  case ARM::VLD3LNd32_UPD:
  case ARM::VLD3LNq16_UPD:
  case ARM::VLD3LNq32_UPD:
    *AddrIdx = 4;
    break;

  // VLD3DUP
  case ARM::VLD3DUPd8:
  case ARM::VLD3DUPd16:
  case ARM::VLD3DUPd32:
  case ARM::VLD3DUPq8:
  case ARM::VLD3DUPq16:
  case ARM::VLD3DUPq32:
    *AddrIdx = 3;
    break;

  case ARM::VLD3DUPd8_UPD:
  case ARM::VLD3DUPd16_UPD:
  case ARM::VLD3DUPd32_UPD:
  case ARM::VLD3DUPq8_UPD:
  case ARM::VLD3DUPq16_UPD:
  case ARM::VLD3DUPq32_UPD:
    *AddrIdx = 4;
    break;

  // VLD4
  case ARM::VLD4d8:
  case ARM::VLD4d16:
  case ARM::VLD4d32:
  case ARM::VLD4q8:
  case ARM::VLD4q16:
  case ARM::VLD4q32:
    *AddrIdx = 4;
    break;

  case ARM::VLD4d8_UPD:
  case ARM::VLD4d16_UPD:
  case ARM::VLD4d32_UPD:
  case ARM::VLD4q8_UPD:
  case ARM::VLD4q16_UPD:
  case ARM::VLD4q32_UPD:
    *AddrIdx = 5;
    break;

  // VLD4LN
  case ARM::VLD4LNd8:
  case ARM::VLD4LNd16:
  case ARM::VLD4LNd32:
  case ARM::VLD4LNq16:
  case ARM::VLD4LNq32:
    *AddrIdx = 4;
    break;

  case ARM::VLD4LNd8_UPD:
  case ARM::VLD4LNd16_UPD:
  case ARM::VLD4LNd32_UPD:
  case ARM::VLD4LNq16_UPD:
  case ARM::VLD4LNq32_UPD:
    *AddrIdx = 5;
    break;

  case ARM::VLD4DUPd8:
  case ARM::VLD4DUPd16:
  case ARM::VLD4DUPd32:
  case ARM::VLD4DUPq16:
  case ARM::VLD4DUPq32:
    *AddrIdx = 4;
    break;

  case ARM::VLD4DUPd8_UPD:
  case ARM::VLD4DUPd16_UPD:
  case ARM::VLD4DUPd32_UPD:
  case ARM::VLD4DUPq16_UPD:
  case ARM::VLD4DUPq32_UPD:
    *AddrIdx = 5;
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
                                       bool IsLoad) {
  unsigned Addr = MI.getOperand(AddrIdx).getReg();

  if (!FlagNaClUseM23ArmAbi && Addr == ARM::R9) {
    // R9-relative loads are no longer sandboxed.
    assert(IsLoad && "There should be no r9-relative stores");
  } else {
    unsigned Opcode;
    if (IsLoad && (MI.getOperand(0).getReg() == ARM::SP)) {
      Opcode = ARM::SFI_GUARD_SP_LOAD;
    } else {
      Opcode = ARM::SFI_GUARD_LOADSTORE;
    }
    // Use same predicate as current instruction.
    unsigned PredReg = 0;
    ARMCC::CondCodes Pred = llvm::getInstrPredicate(&MI, PredReg);
    // Use the older BIC sandbox, which is universal, but incurs a stall.
    BuildMI(MBB, MBBI, MI.getDebugLoc(), TII->get(Opcode))
      .addReg(Addr, RegState::Define)  // Address definition (as dst).
      .addReg(Addr, RegState::Kill)    // Address read (as src).
      .addImm((int64_t) Pred)          // predicate condition
      .addReg(PredReg);                // predicate source register (CPSR)

    /*
     * This pseudo-instruction is intended to generate something resembling the
     * following, but with alignment enforced.
     * TODO(cbiffle): move alignment into this function, use the code below.
     *
     *  // bic<cc> Addr, Addr, #0xC0000000
     *  BuildMI(MBB, MBBI, MI.getDebugLoc(),
     *          TII->get(ARM::BICri))
     *    .addReg(Addr)            // rD
     *    .addReg(Addr)            // rN
     *    .addImm(0xC0000000)      // imm
     *    .addImm((int64_t) Pred)  // predicate condition
     *    .addReg(PredReg)         // predicate source register (CPSR)
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

  //
  // NEON stores
  //

  // VST1
  case ARM::VST1d8:
  case ARM::VST1d16:
  case ARM::VST1d32:
  case ARM::VST1d64:
  case ARM::VST1q8:
  case ARM::VST1q16:
  case ARM::VST1q32:
  case ARM::VST1q64:
    *AddrIdx = 0;
    break;

  case ARM::VST1d8wb_fixed:
  case ARM::VST1d16wb_fixed:
  case ARM::VST1d32wb_fixed:
  case ARM::VST1d64wb_fixed:
  case ARM::VST1q8wb_fixed:
  case ARM::VST1q16wb_fixed:
  case ARM::VST1q32wb_fixed:
  case ARM::VST1q64wb_fixed:
  case ARM::VST1d8wb_register:
  case ARM::VST1d16wb_register:
  case ARM::VST1d32wb_register:
  case ARM::VST1d64wb_register:
  case ARM::VST1q8wb_register:
  case ARM::VST1q16wb_register:
  case ARM::VST1q32wb_register:
  case ARM::VST1q64wb_register:
    *AddrIdx = 1;
    break;

  // VST1LN
  case ARM::VST1LNd8:
  case ARM::VST1LNd16:
  case ARM::VST1LNd32:
    *AddrIdx = 0;
    break;

  case ARM::VST1LNd8_UPD:
  case ARM::VST1LNd16_UPD:
  case ARM::VST1LNd32_UPD:
    *AddrIdx = 1;
    break;

  // VST2
  case ARM::VST2d8:
  case ARM::VST2d16:
  case ARM::VST2d32:
  case ARM::VST2q8:
  case ARM::VST2q16:
  case ARM::VST2q32:
    *AddrIdx = 0;
    break;

  case ARM::VST2d8wb_fixed:
  case ARM::VST2d16wb_fixed:
  case ARM::VST2d32wb_fixed:
  case ARM::VST2q8wb_fixed:
  case ARM::VST2q16wb_fixed:
  case ARM::VST2q32wb_fixed:
  case ARM::VST2d8wb_register:
  case ARM::VST2d16wb_register:
  case ARM::VST2d32wb_register:
  case ARM::VST2q8wb_register:
  case ARM::VST2q16wb_register:
  case ARM::VST2q32wb_register:
    *AddrIdx = 1;
    break;

  // VST2LN
  case ARM::VST2LNd8:
  case ARM::VST2LNd16:
  case ARM::VST2LNq16:
  case ARM::VST2LNd32:
  case ARM::VST2LNq32:
    *AddrIdx = 0;
    break;

  case ARM::VST2LNd8_UPD:
  case ARM::VST2LNd16_UPD:
  case ARM::VST2LNq16_UPD:
  case ARM::VST2LNd32_UPD:
  case ARM::VST2LNq32_UPD:
    *AddrIdx = 1;
    break;

  // VST3
  case ARM::VST3d8:
  case ARM::VST3d16:
  case ARM::VST3d32:
  case ARM::VST3q8:
  case ARM::VST3q16:
  case ARM::VST3q32:
    *AddrIdx = 0;
    break;

  case ARM::VST3d8_UPD:
  case ARM::VST3d16_UPD:
  case ARM::VST3d32_UPD:
  case ARM::VST3q8_UPD:
  case ARM::VST3q16_UPD:
  case ARM::VST3q32_UPD:
    *AddrIdx = 1;
    break;

  // VST3LN
  case ARM::VST3LNd8:
  case ARM::VST3LNd16:
  case ARM::VST3LNq16:
  case ARM::VST3LNd32:
  case ARM::VST3LNq32:
    *AddrIdx = 0;
    break;

  case ARM::VST3LNd8_UPD:
  case ARM::VST3LNd16_UPD:
  case ARM::VST3LNq16_UPD:
  case ARM::VST3LNd32_UPD:
  case ARM::VST3LNq32_UPD:
    *AddrIdx = 1;
    break;

  // VST4
  case ARM::VST4d8:
  case ARM::VST4d16:
  case ARM::VST4d32:
  case ARM::VST4q8:
  case ARM::VST4q16:
  case ARM::VST4q32:
    *AddrIdx = 0;
    break;

  case ARM::VST4d8_UPD:
  case ARM::VST4d16_UPD:
  case ARM::VST4d32_UPD:
  case ARM::VST4q8_UPD:
  case ARM::VST4q16_UPD:
  case ARM::VST4q32_UPD:
    *AddrIdx = 1;
    break;

  // VST4LN
  case ARM::VST4LNd8:
  case ARM::VST4LNd16:
  case ARM::VST4LNq16:
  case ARM::VST4LNd32:
  case ARM::VST4LNq32:
    *AddrIdx = 0;
    break;

  case ARM::VST4LNd8_UPD:
  case ARM::VST4LNd16_UPD:
  case ARM::VST4LNq16_UPD:
  case ARM::VST4LNd32_UPD:
  case ARM::VST4LNq32_UPD:
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
  case ARM::STREXD:
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
  bool Modified = false;
  for (MachineBasicBlock::iterator MBBI = MBB.begin(), E = MBB.end();
       MBBI != E;
       ++MBBI) {
    MachineInstr &MI = *MBBI;
    int AddrIdx;

    if (FlagSfiLoad && IsDangerousLoad(MI, &AddrIdx)) {
      SandboxMemory(MBB, MBBI, MI, AddrIdx, true);
      Modified = true;
    }
    if (FlagSfiStore && IsDangerousStore(MI, &AddrIdx)) {
      SandboxMemory(MBB, MBBI, MI, AddrIdx, false);
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
