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

  Out.EmitBundleLock();
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
  Out.EmitBundleAlignEnd();
  Out.EmitBundleLock();
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
  Out.EmitBundleAlignEnd();
  Out.EmitBundleLock();
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

  Out.EmitBundleLock();
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

  Out.EmitBundleLock();
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

  Out.EmitBundleLock();
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

  Out.EmitBundleLock();
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

  Out.EmitBundleLock();
  EmitBICMask(Out, AddrReg, Pred, 0xC0000000);
  Out.EmitInstruction(Saved[2]);
  EmitBICMask(Out, SpReg, Pred, 0xC0000000);
  Out.EmitBundleUnlock();
}

namespace llvm {
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
//
//   We need global state to keep track of the explicit prefix (PREFIX_*)
//   instructions. Unfortunately, the assembly parser prefers to generate
//   these instead of combined instructions. At this time, having only
//   one explicit prefix is supported.


bool CustomExpandInstNaClARM(const MCInst &Inst, MCStreamer &Out) {
  const int MaxSaved = 4;
  static MCInst Saved[MaxSaved];
  static int SaveCount  = 0;
  static int I = 0;
  // This routine only executes  if RecurseGuard == 0
  static bool RecurseGuard = false; 

  // If we are emitting to .s, just emit all pseudo-instructions directly.
  if (Out.hasRawTextSupport()) {
    return false;
  }

  //No recursive calls allowed;
  if (RecurseGuard) return false;

  unsigned Opc = Inst.getOpcode();

  DEBUG(dbgs() << "CustomExpandInstNaClARM("; Inst.dump(); dbgs() << ")\n");

  // Note: SFI_NOP_IF_AT_BUNDLE_END is only emitted directly as part of
  // a stack guard in conjunction with a SFI_DATA_MASK

  // Logic:
  // This is somewhat convoluted, but in the current model, the SFI
  // guard pseudo instructions occur PRIOR to the actual instruction.
  // So, the bundling/alignment operation has to refer to the FOLLOWING
  // one or two instructions.
  //
  // When a SFI_* pseudo is detected, it is saved. Then, the saved SFI_*
  // pseudo and the very next one or two instructions are used as arguments to
  // the Emit*() functions in this file.  This is the reason why we have a
  // doublely nested switch here.  First, to save the SFI_* pseudo, then to
  // emit it and the next instruction

  // By default, we only need to save two or three instructions

  if ((I == 0) && (SaveCount == 0)) {
    // Base State, no saved instructions.
    // If the current instruction is a SFI instruction, set the SaveCount
    // and fall through.
    switch (Opc) {
    default:
      SaveCount = 0; // Nothing to do.
      return false;  // Handle this Inst elsewhere.
    case ARM::SFI_NOP_IF_AT_BUNDLE_END:
      SaveCount = 3;
      break;
    case ARM::SFI_DATA_MASK:
      SaveCount = 0; // Do nothing.
      break;
    case ARM::SFI_GUARD_CALL:
    case ARM::SFI_GUARD_INDIRECT_CALL:
    case ARM::SFI_GUARD_INDIRECT_JMP:
    case ARM::SFI_GUARD_RETURN:
    case ARM::SFI_GUARD_LOADSTORE:
    case ARM::SFI_GUARD_LOADSTORE_TST:
      SaveCount = 2;
      break;
    case ARM::SFI_GUARD_SP_LOAD:
      SaveCount = 4;
      break;
    }
  }

  if (I < SaveCount) {
    // Othewise, save the current Inst and return
    Saved[I++] = Inst;
    if (I < SaveCount)
      return true;
    // Else fall through to next stat
  }

  if (SaveCount > 0) { 
    assert(I == SaveCount && "Bookeeping Error");
    SaveCount = 0; // Reset for next iteration
    // The following calls may call Out.EmitInstruction()
    // which must not again call CustomExpandInst ...
    // So set RecurseGuard = 1;
    RecurseGuard = true;

    switch (Saved[0].getOpcode()) {
    default:  /* No action required */      break;
    case ARM::SFI_NOP_IF_AT_BUNDLE_END:
      EmitDataMask(I, Saved, Out);
      break;
    case ARM::SFI_DATA_MASK:
      assert(0 && "Unexpected NOP_IF_AT_BUNDLE_END as a Saved Inst");
      break;
    case ARM::SFI_GUARD_CALL:
      EmitDirectGuardCall(I, Saved, Out);
      break;
    case ARM::SFI_GUARD_INDIRECT_CALL:
      EmitIndirectGuardCall(I, Saved, Out);
      break;
    case ARM::SFI_GUARD_INDIRECT_JMP:
      EmitIndirectGuardJmp(I, Saved, Out);
      break;
    case ARM::SFI_GUARD_RETURN:
      EmitGuardReturn(I, Saved, Out);
      break;
    case ARM::SFI_GUARD_LOADSTORE:
      EmitGuardLoadOrStore(I, Saved, Out);
      break;
    case ARM::SFI_GUARD_LOADSTORE_TST:
      EmitGuardLoadOrStoreTst(I, Saved, Out);
      break;
    case ARM::SFI_GUARD_SP_LOAD:
      EmitGuardSpLoad(I, Saved, Out);
      break;
    }
    I = 0; // Reset I for next.
    assert(RecurseGuard && "Illegal Depth");
    RecurseGuard = false;
    return true;
  }

  return false;
}

} // namespace llvm
