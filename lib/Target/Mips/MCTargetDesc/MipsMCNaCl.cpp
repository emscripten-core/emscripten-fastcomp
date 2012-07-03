//=== MipsMCNaCl.cpp -  Expansion of NaCl pseudo-instructions    --*- C++ -*-=//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//
#define DEBUG_TYPE "mips-mc-nacl"

#include "MCTargetDesc/MipsBaseInfo.h"
#include "MCTargetDesc/MipsMCTargetDesc.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"

using namespace llvm;

/// Two helper functions for emitting the actual guard instructions

static void EmitMask(MCStreamer &Out,
                        unsigned Addr, unsigned Mask) {
  // and \Addr, \Addr, \Mask
  MCInst MaskInst;
  MaskInst.setOpcode(Mips::AND);
  MaskInst.addOperand(MCOperand::CreateReg(Addr));
  MaskInst.addOperand(MCOperand::CreateReg(Addr));
  MaskInst.addOperand(MCOperand::CreateReg(Mask));
  Out.EmitInstruction(MaskInst);
}

// This is ONLY used for sandboxing stack changes.
// The reason why SFI_NOP_IF_AT_BUNDLE_END gets handled here is that
// it must ensure that the two instructions are in the same bundle.
// It just so happens that the SFI_NOP_IF_AT_BUNDLE_END is always
// emitted in conjunction with a SFI_DATA_MASK
//
static void EmitDataMask(int I, MCInst Saved[], MCStreamer &Out) {
  assert(I == 3 &&
         (Mips::SFI_NOP_IF_AT_BUNDLE_END == Saved[0].getOpcode()) &&
         (Mips::SFI_DATA_MASK == Saved[2].getOpcode()) &&
         "Unexpected SFI Pseudo while lowering");

  unsigned Addr = Saved[2].getOperand(0).getReg();
  unsigned Mask = Saved[2].getOperand(2).getReg();
  assert((Mips::SP == Addr) && "Unexpected register at stack guard");

  Out.EmitBundleLock();
  Out.EmitInstruction(Saved[1]);
  EmitMask(Out, Addr, Mask);
  Out.EmitBundleUnlock();
}

static void EmitDirectGuardCall(int I, MCInst Saved[],
                                MCStreamer &Out) {
  // sfi_call_preamble --->
  //   sfi_nops_to_force_slot2
  assert(I == 3 && (Mips::SFI_GUARD_CALL == Saved[0].getOpcode()) &&
         "Unexpected SFI Pseudo while lowering SFI_GUARD_CALL");
  Out.EmitBundleAlignEnd();
  Out.EmitBundleLock();
  Out.EmitInstruction(Saved[1]);
  Out.EmitInstruction(Saved[2]);
  Out.EmitBundleUnlock();
}

static void EmitIndirectGuardCall(int I, MCInst Saved[],
                                  MCStreamer &Out) {
  // sfi_indirect_call_preamble link --->
  //   sfi_nops_to_force_slot1
  //   sfi_code_mask \link \link \maskreg
  assert(I == 3 && (Mips::SFI_GUARD_INDIRECT_CALL == Saved[0].getOpcode()) &&
         "Unexpected SFI Pseudo while lowering SFI_GUARD_INDIRECT_CALL");

  unsigned Addr = Saved[0].getOperand(0).getReg();
  unsigned Mask = Saved[0].getOperand(2).getReg();

  Out.EmitBundleAlignEnd();
  Out.EmitBundleLock();
  EmitMask(Out, Addr, Mask);
  Out.EmitInstruction(Saved[1]);
  Out.EmitInstruction(Saved[2]);
  Out.EmitBundleUnlock();
}

static void EmitIndirectGuardJmp(int I, MCInst Saved[], MCStreamer &Out) {
  //  sfi_indirect_jump_preamble link --->
  //    sfi_nop_if_at_bundle_end
  //    sfi_code_mask \link \link \maskreg
  assert(I == 2 && (Mips::SFI_GUARD_INDIRECT_JMP == Saved[0].getOpcode()) &&
         "Unexpected SFI Pseudo while lowering SFI_GUARD_INDIRECT_JMP");
  unsigned Addr = Saved[0].getOperand(0).getReg();
  unsigned Mask = Saved[0].getOperand(2).getReg();

  Out.EmitBundleLock();
  EmitMask(Out, Addr, Mask);
  Out.EmitInstruction(Saved[1]);
  Out.EmitBundleUnlock();
}

static void EmitGuardReturn(int I, MCInst Saved[], MCStreamer &Out) {
  // sfi_return_preamble reg --->
  //    sfi_nop_if_at_bundle_end
  //    sfi_code_mask \reg \reg \maskreg
  assert(I == 2 && (Mips::SFI_GUARD_RETURN == Saved[0].getOpcode()) &&
         "Unexpected SFI Pseudo while lowering SFI_GUARD_RETURN");
  unsigned Reg = Saved[0].getOperand(0).getReg();
  unsigned Mask = Saved[0].getOperand(2).getReg();

  Out.EmitBundleLock();
  EmitMask(Out, Reg, Mask);
  Out.EmitInstruction(Saved[1]);
  Out.EmitBundleUnlock();
}

static void EmitGuardLoadOrStore(int I, MCInst Saved[], MCStreamer &Out) {
  // sfi_load_store_preamble reg --->
  //    sfi_nop_if_at_bundle_end
  //    sfi_data_mask \reg \reg \maskreg
  assert(I == 2 && (Mips::SFI_GUARD_LOADSTORE == Saved[0].getOpcode()) &&
         "Unexpected SFI Pseudo while lowering SFI_GUARD_LOADSTORE");
  unsigned Reg = Saved[0].getOperand(0).getReg();
  unsigned Mask = Saved[0].getOperand(2).getReg();

  Out.EmitBundleLock();
  EmitMask(Out, Reg, Mask);
  Out.EmitInstruction(Saved[1]);
  Out.EmitBundleUnlock();
}

namespace llvm {
// CustomExpandInstNaClMips -
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


bool CustomExpandInstNaClMips(const MCInst &Inst, MCStreamer &Out) {
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

  DEBUG(dbgs() << "CustomExpandInstNaClMips("; Inst.dump(); dbgs() << ")\n");

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
    case Mips::SFI_NOP_IF_AT_BUNDLE_END:
    case Mips::SFI_GUARD_CALL:
    case Mips::SFI_GUARD_INDIRECT_CALL:
      SaveCount = 3;
      break;
    case Mips::SFI_DATA_MASK:
      SaveCount = 0; // Do nothing.
      break;
    case Mips::SFI_GUARD_INDIRECT_JMP:
    case Mips::SFI_GUARD_RETURN:
    case Mips::SFI_GUARD_LOADSTORE:
      SaveCount = 2;
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
    case Mips::SFI_NOP_IF_AT_BUNDLE_END:
      EmitDataMask(I, Saved, Out);
      break;
    case Mips::SFI_DATA_MASK:
      assert(0 && "Unexpected NOP_IF_AT_BUNDLE_END as a Saved Inst");
      break;
    case Mips::SFI_GUARD_CALL:
      EmitDirectGuardCall(I, Saved, Out);
      break;
    case Mips::SFI_GUARD_INDIRECT_CALL:
      EmitIndirectGuardCall(I, Saved, Out);
      break;
    case Mips::SFI_GUARD_INDIRECT_JMP:
      EmitIndirectGuardJmp(I, Saved, Out);
      break;
    case Mips::SFI_GUARD_RETURN:
      EmitGuardReturn(I, Saved, Out);
      break;
    case Mips::SFI_GUARD_LOADSTORE:
      EmitGuardLoadOrStore(I, Saved, Out);
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
