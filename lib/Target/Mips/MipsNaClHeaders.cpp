//===-- MipsNaClHeaders.cpp - Print SFI headers to an Mips .s file --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the initial header string needed
// for the Native Client target in Mips assembly.
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/raw_ostream.h"
#include "MipsNaClRewritePass.h"
#include <string>

using namespace llvm;

void EmitMipsSFIHeaders(raw_ostream &O) {
  O << " # ========================================\n";
  O << "# Branch: " << FlagSfiBranch << "\n";
  O << "# Stack: " << FlagSfiStack << "\n";
  O << "# Store: " << FlagSfiStore << "\n";
  O << "# Load: " << FlagSfiLoad << "\n";

  O << " # ========================================\n";
  // NOTE: this macro does bundle alignment as follows
  //       if current bundle pos is X emit pX data items of value "val"
  // NOTE: that pos will be one of: 0,4,8,12
  //
  O <<
    "\t.macro sfi_long_based_on_pos p0 p1 p2 p3 val\n"
    "\t.set pos, (. - XmagicX) % 16\n"
    "\t.fill  (((\\p3<<12)|(\\p2<<8)|(\\p1<<4)|\\p0)>>pos) & 15, 4, \\val\n"
    "\t.endm\n"
    "\n\n";

  O <<
    "\t.macro sfi_nop_if_at_bundle_end\n"
    "\tsfi_long_based_on_pos 0 0 0 1 0x00000000\n"
    "\t.endm\n"
      "\n\n";

  O <<
    "\t.macro sfi_nops_to_force_slot3\n"
    "\tsfi_long_based_on_pos 3 2 1 0 0x00000000\n"
    "\t.endm\n"
    "\n\n";

  O <<
    "\t.macro sfi_nops_to_force_slot2\n"
    "\tsfi_long_based_on_pos 2 1 0 3 0x00000000\n"
    "\t.endm\n"
    "\n\n";

  O <<
    "\t.macro sfi_nops_to_force_slot1\n"
    "\tsfi_long_based_on_pos 1 0 3 2 0x00000000\n"
    "\t.endm\n"
    "\n\n";

  O << " # ========================================\n";
  O <<
    "\t.macro sfi_data_mask reg1 reg2 maskreg\n"
    "\tand \\reg1, \\reg2, \\maskreg\n"
    "\t.endm\n"
    "\n\n";

  O <<
    "\t.macro sfi_code_mask reg1 reg2 maskreg\n"
    "\tand \\reg1, \\reg2, \\maskreg\n"
    "\t.endm\n"
    "\n\n";

  O << " # ========================================\n";
  if (FlagSfiBranch) {
    O <<
      "\t.macro sfi_call_preamble\n"
      "\tsfi_nops_to_force_slot2\n"
      "\t.endm\n"
      "\n\n";

    O <<
      "\t.macro sfi_return_preamble reg1 reg2 maskreg\n"
      "\tsfi_nop_if_at_bundle_end\n"
      "\tsfi_code_mask \\reg1, \\reg2, \\maskreg\n"
      "\t.endm\n"
      "\n\n";

    // This is used just before "jr"
    O <<
      "\t.macro sfi_indirect_jump_preamble reg1 reg2 maskreg\n"
      "\tsfi_nop_if_at_bundle_end\n"
      "\tsfi_code_mask \\reg1, \\reg2, \\maskreg\n"
      "\t.endm\n"
      "\n\n";

    // This is used just before "jalr"
    O <<
      "\t.macro sfi_indirect_call_preamble reg1 reg2 maskreg\n"
      "\tsfi_nops_to_force_slot1\n"
      "\tsfi_code_mask \\reg1, \\reg2, \\maskreg\n"
      "\t.endm\n"
      "\n\n";

  }

  if (FlagSfiStore) {
    O << " # ========================================\n";

    O <<
      "\t.macro sfi_load_store_preamble reg1 reg2 maskreg\n"
      "\tsfi_nop_if_at_bundle_end\n"
      "\tsfi_data_mask \\reg1, \\reg2 , \\maskreg\n"
      "\t.endm\n"
      "\n\n";
  } else {
    O <<
      "\t.macro sfi_load_store_preamble reg1 reg2 maskreg\n"
      "\t.endm\n"
      "\n\n";
  }

  O << " # ========================================\n";
  O << "\t.text\n";
}
