//=== ARMMCNaCl.cpp -  Expansion of NaCl pseudo-instructions     --*- C++ -*-=//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//
#define DEBUG_TYPE "arm-mc-nacl"

#include "MCTargetDesc/ARMBaseInfo.h"
#include "MCTargetDesc/ARMMCExpr.h"
#include "MCTargetDesc/ARMMCNaCl.h"
#include "MCTargetDesc/ARMMCTargetDesc.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

namespace llvm {
  cl::opt<bool> FlagSfiZeroMask("sfi-zero-mask");
}

/// Two helper functions for emitting the actual guard instructions

static void EmitBICMask(MCStreamer &Out,
                        unsigned Addr, int64_t  Pred, unsigned Mask) {
  // bic\Pred \Addr, \Addr, #Mask
  MCInst BICInst;
  BICInst.setOpcode(ARM::BICri);
  BICInst.addOperand(MCOperand::CreateReg(Addr)); // rD
  BICInst.addOperand(MCOperand::CreateReg(Addr)); // rS
  if (FlagSfiZeroMask) {
    BICInst.addOperand(MCOperand::CreateImm(0)); // imm
  } else {
    BICInst.addOperand(MCOperand::CreateImm(Mask)); // imm
  }
  BICInst.addOperand(MCOperand::CreateImm(Pred));  // predicate
  BICInst.addOperand(MCOperand::CreateReg(ARM::CPSR)); // CPSR
  BICInst.addOperand(MCOperand::CreateReg(0)); // flag out
  Out.EmitInstruction(BICInst);
}

static void EmitTST(MCStreamer &Out, unsigned Reg) {
  // tst \reg, #\MASK typically 0xc0000000
  const unsigned Mask = 0xC0000000;
  MCInst TSTInst;
  TSTInst.setOpcode(ARM::TSTri);
  TSTInst.addOperand(MCOperand::CreateReg(Reg));  // rS
  if (FlagSfiZeroMask) {
    TSTInst.addOperand(MCOperand::CreateImm(0)); // imm
  } else {
    TSTInst.addOperand(MCOperand::CreateImm(Mask)); // imm
  }
  TSTInst.addOperand(MCOperand::CreateImm((int64_t)ARMCC::AL)); // Always
  TSTInst.addOperand(MCOperand::CreateImm(0)); // flag out
  Out.EmitInstruction(TSTInst);
}


// This is ONLY used for sandboxing stack changes.
// The reason why SFI_NOP_IF_AT_BUNDLE_END gets handled here is that
// it must ensure that the two instructions are in the same bundle.
// It just so happens that the SFI_NOP_IF_AT_BUNDLE_END is always
// emitted in conjunction with a SFI_DATA_MASK
//
static void EmitDataMask(int I, MCInst Saved[], MCStreamer &Out) {
  assert(I == 3 &&
         (ARM::SFI_NOP_IF_AT_BUNDLE_END == Saved[0].getOpcode()) &&
         (ARM::SFI_DATA_MASK == Saved[2].getOpcode()) &&
         "Unexpected SFI Pseudo while lowering");

  unsigned Addr = Saved[2].getOperand(0).getReg();
  int64_t  Pred = Saved[2].getOperand(2).getImm();
  assert((ARM::SP == Addr) && "Unexpected register at stack guard");

  Out.EmitBundleLock(false);
  Out.EmitInstruction(Saved[1]);
  EmitBICMask(Out, Addr, Pred, 0xC0000000);
  Out.EmitBundleUnlock();
}

static void EmitDirectGuardCall(int I, MCInst Saved[],
                                MCStreamer &Out) {
  // sfi_call_preamble cond=
  //   sfi_nops_to_force_slot3
  assert(I == 2 && (ARM::SFI_GUARD_CALL == Saved[0].getOpcode()) &&
         "Unexpected SFI Pseudo while lowering SFI_GUARD_CALL");
  Out.EmitBundleLock(true);
  Out.EmitInstruction(Saved[1]);
  Out.EmitBundleUnlock();
}

static void EmitIndirectGuardCall(int I, MCInst Saved[],
                                  MCStreamer &Out) {
  // sfi_indirect_call_preamble link cond=
  //   sfi_nops_to_force_slot2
  //   sfi_code_mask \link \cond
  assert(I == 2 && (ARM::SFI_GUARD_INDIRECT_CALL == Saved[0].getOpcode()) &&
         "Unexpected SFI Pseudo while lowering SFI_GUARD_CALL");
  unsigned Reg = Saved[0].getOperand(0).getReg();
  int64_t Pred = Saved[0].getOperand(2).getImm();
  Out.EmitBundleLock(true);
  EmitBICMask(Out, Reg, Pred, 0xC000000F);
  Out.EmitInstruction(Saved[1]);
  Out.EmitBundleUnlock();
}

static void EmitIndirectGuardJmp(int I, MCInst Saved[], MCStreamer &Out) {
  //  sfi_indirect_jump_preamble link cond=
  //   sfi_nop_if_at_bundle_end
  //   sfi_code_mask \link \cond
  assert(I == 2 && (ARM::SFI_GUARD_INDIRECT_JMP == Saved[0].getOpcode()) &&
         "Unexpected SFI Pseudo while lowering SFI_GUARD_CALL");
  unsigned Reg = Saved[0].getOperand(0).getReg();
  int64_t Pred = Saved[0].getOperand(2).getImm();

  Out.EmitBundleLock(false);
  EmitBICMask(Out, Reg, Pred, 0xC000000F);
  Out.EmitInstruction(Saved[1]);
  Out.EmitBundleUnlock();
}

static void EmitGuardReturn(int I, MCInst Saved[], MCStreamer &Out) {
  // sfi_return_preamble reg cond=
  //    sfi_nop_if_at_bundle_end
  //    sfi_code_mask \reg \cond
  assert(I == 2 && (ARM::SFI_GUARD_RETURN == Saved[0].getOpcode()) &&
         "Unexpected SFI Pseudo while lowering SFI_GUARD_RETURN");
  int64_t Pred = Saved[0].getOperand(0).getImm();

  Out.EmitBundleLock(false);
  EmitBICMask(Out, ARM::LR, Pred, 0xC000000F);
  Out.EmitInstruction(Saved[1]);
  Out.EmitBundleUnlock();
}

static void EmitGuardLoadOrStore(int I, MCInst Saved[], MCStreamer &Out) {
  // sfi_store_preamble reg cond ---->
  //    sfi_nop_if_at_bundle_end
  //    sfi_data_mask \reg, \cond
  assert(I == 2 && (ARM::SFI_GUARD_LOADSTORE == Saved[0].getOpcode()) &&
         "Unexpected SFI Pseudo while lowering SFI_GUARD_RETURN");
  unsigned Reg = Saved[0].getOperand(0).getReg();
  int64_t Pred = Saved[0].getOperand(2).getImm();

  Out.EmitBundleLock(false);
  EmitBICMask(Out, Reg, Pred, 0xC0000000);
  Out.EmitInstruction(Saved[1]);
  Out.EmitBundleUnlock();
}

static void EmitGuardLoadOrStoreTst(int I, MCInst Saved[], MCStreamer &Out) {
  // sfi_cstore_preamble reg -->
  //   sfi_nop_if_at_bundle_end
  //   sfi_data_tst \reg
  assert(I == 2 && (ARM::SFI_GUARD_LOADSTORE_TST == Saved[0].getOpcode()) &&
         "Unexpected SFI Pseudo while lowering");
  unsigned Reg = Saved[0].getOperand(0).getReg();

  Out.EmitBundleLock(false);
  EmitTST(Out, Reg);
  Out.EmitInstruction(Saved[1]);
  Out.EmitBundleUnlock();
}

// This is ONLY used for loads into the stack pointer.
static void EmitGuardSpLoad(int I, MCInst Saved[], MCStreamer &Out) {
  assert(I == 4 &&
         (ARM::SFI_GUARD_SP_LOAD == Saved[0].getOpcode()) &&
         (ARM::SFI_NOP_IF_AT_BUNDLE_END == Saved[1].getOpcode()) &&
         (ARM::SFI_DATA_MASK == Saved[3].getOpcode()) &&
         "Unexpected SFI Pseudo while lowering");

  unsigned AddrReg = Saved[0].getOperand(0).getReg();
  unsigned SpReg = Saved[3].getOperand(0).getReg();
  int64_t  Pred = Saved[3].getOperand(2).getImm();
  assert((ARM::SP == SpReg) && "Unexpected register at stack guard");

  Out.EmitBundleLock(false);
  EmitBICMask(Out, AddrReg, Pred, 0xC0000000);
  Out.EmitInstruction(Saved[2]);
  EmitBICMask(Out, SpReg, Pred, 0xC0000000);
  Out.EmitBundleUnlock();
}

namespace llvm {

const int ARMMCNaClSFIState::MaxSaved;

// CustomExpandInstNaClARM -
//   If Inst is a NaCl pseudo instruction, emits the substitute
//   expansion to the MCStreamer and returns true.
//   Otherwise, returns false.
//
//   NOTE: Each time this function calls Out.EmitInstruction(), it will be
//   called again recursively to rewrite the new instruction being emitted.
//   Care must be taken to ensure that this does not result in an infinite
//   loop. Also, global state must be managed carefully so that it is
//   consistent during recursive calls.
bool CustomExpandInstNaClARM(const MCInst &Inst, MCStreamer &Out,
                             ARMMCNaClSFIState &State) {
  // Logic:
  // This is somewhat convoluted, but in the current model, the SFI
  // guard pseudo instructions occur PRIOR to the actual instruction.
  // So, the bundling/alignment operation has to refer to the FOLLOWING
  // instructions.
  //
  // When a SFI pseudo is detected, it is saved. Then, the saved SFI
  // pseudo and the very next instructions (their amount depending on the kind
  // of the SFI pseudo) are used as arguments to the Emit*() functions in
  // this file.
  //
  // Some state data is used to preserve state accross calls:
  //
  // Saved:      the saved instructions (starting with the SFI_ pseudo).
  // SavedCount: the amount of saved instructions required for the SFI pseudo
  //             that's being expanded.
  // I:          the index of the currently saved instruction - used to track
  //             where in Saved to insert the instruction and how many more
  //             remain.
  //

  // If we are emitting to .s, just emit all pseudo-instructions directly.
  if (Out.hasRawTextSupport()) {
    return false;
  }

  // Protect against recursive execution. If State.RecurseiveCall == true, it
  // means we're already in the process of expanding a custom instruction, and
  // we don't need to run recursively on anything generated by such an
  // expansion.
  if (State.RecursiveCall)
    return false;

  DEBUG(dbgs() << "CustomExpandInstNaClARM("; Inst.dump(); dbgs() << ")\n");

  if ((State.I == 0) && (State.SaveCount == 0)) {
    // Base state: no SFI guard identified yet and no saving started.
    switch (Inst.getOpcode()) {
      default:
        // We don't handle non-SFI guards here
        return false;
      case ARM::SFI_NOP_IF_AT_BUNDLE_END:
        // Note: SFI_NOP_IF_AT_BUNDLE_END is only emitted directly as part of
        // a stack guard in conjunction with a SFI_DATA_MASK.
        State.SaveCount = 3;
        break;
      case ARM::SFI_DATA_MASK:
        assert(0 &&
            "SFI_DATA_MASK found without preceding SFI_NOP_IF_AT_BUNDLE_END");
        return false;
      case ARM::SFI_GUARD_CALL:
      case ARM::SFI_GUARD_INDIRECT_CALL:
      case ARM::SFI_GUARD_INDIRECT_JMP:
      case ARM::SFI_GUARD_RETURN:
      case ARM::SFI_GUARD_LOADSTORE:
      case ARM::SFI_GUARD_LOADSTORE_TST:
        State.SaveCount = 2;
        break;
      case ARM::SFI_GUARD_SP_LOAD:
        State.SaveCount = 4;
        break;
    }
  }

  // We're in "saving instructions" state
  if (State.I < State.SaveCount) {
    // This instruction has to be saved
    assert(State.I < State.MaxSaved && "Trying to save too many instructions");
    State.Saved[State.I++] = Inst;
    if (State.I < State.SaveCount)
      return true;
  }

  // We're in "saved enough instructions, time to emit" state
  assert(State.I == State.SaveCount && State.SaveCount > 0 && "Bookeeping Error");

  // When calling Emit* functions, do that with RecurseGuard set (the comment
  // at the beginning of this function explains why)
  State.RecursiveCall = true;
  switch (State.Saved[0].getOpcode()) {
    default:
      break;
    case ARM::SFI_NOP_IF_AT_BUNDLE_END:
      EmitDataMask(State.I, State.Saved, Out);
      break;
    case ARM::SFI_DATA_MASK:
      assert(0 && "SFI_DATA_MASK can't start a SFI sequence");
      break;
    case ARM::SFI_GUARD_CALL:
      EmitDirectGuardCall(State.I, State.Saved, Out);
      break;
    case ARM::SFI_GUARD_INDIRECT_CALL:
      EmitIndirectGuardCall(State.I, State.Saved, Out);
      break;
    case ARM::SFI_GUARD_INDIRECT_JMP:
      EmitIndirectGuardJmp(State.I, State.Saved, Out);
      break;
    case ARM::SFI_GUARD_RETURN:
      EmitGuardReturn(State.I, State.Saved, Out);
      break;
    case ARM::SFI_GUARD_LOADSTORE:
      EmitGuardLoadOrStore(State.I, State.Saved, Out);
      break;
    case ARM::SFI_GUARD_LOADSTORE_TST:
      EmitGuardLoadOrStoreTst(State.I, State.Saved, Out);
      break;
    case ARM::SFI_GUARD_SP_LOAD:
      EmitGuardSpLoad(State.I, State.Saved, Out);
      break;
  }
  assert(State.RecursiveCall && "Illegal Depth");
  State.RecursiveCall = false;

  // We're done expanding a SFI guard. Reset state vars.
  State.SaveCount = 0;
  State.I = 0;
  return true;
}

} // namespace llvm
