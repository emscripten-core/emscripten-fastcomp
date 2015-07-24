//===- MCNaClExpander.cpp ---------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the MCNaClExpander class. This is a base
// class that encapsulates the expansion logic for MCInsts, and holds
// state such as available scratch registers.
//
//===----------------------------------------------------------------------===//

#include "llvm/MC/MCNaClExpander.h"
#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/Support/SourceMgr.h"

namespace llvm {

void MCNaClExpander::Error(const MCInst &Inst, const char msg[]) {
  if (SrcMgr)
    SrcMgr->PrintMessage(Inst.getLoc(), SourceMgr::DK_Error, msg);
  else
    report_fatal_error(msg, true);
}

void MCNaClExpander::pushScratchReg(unsigned Reg) {
  ScratchRegs.push_back(Reg);
}

unsigned MCNaClExpander::popScratchReg() {
  assert(!ScratchRegs.empty() &&
         "Trying to pop an empty scratch register stack");

  unsigned Reg = ScratchRegs.back();
  ScratchRegs.pop_back();
  return Reg;
}

unsigned MCNaClExpander::getScratchReg(int index) {
  int len = numScratchRegs();
  return ScratchRegs[len - index - 1];
}

unsigned MCNaClExpander::numScratchRegs() { return ScratchRegs.size(); }

bool MCNaClExpander::isReturn(const MCInst &Inst) {
  return InstInfo->get(Inst.getOpcode()).isReturn();
}
}
