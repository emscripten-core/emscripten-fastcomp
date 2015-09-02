//===- X86MCNaClExpander.cpp ------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the X86MCNaClExpander class, the X86 specific
// subclass of MCNaClExpander.
//
//===----------------------------------------------------------------------===//
#include "X86MCNaClExpander.h"
#include "X86BaseInfo.h"

#include "llvm/MC/MCInst.h"
#include "llvm/MC/MCInstrDesc.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCNaClExpander.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCStreamer.h"

using namespace llvm;

static const int kBundleSize = 32;

unsigned getReg32(unsigned Reg);

void X86::X86MCNaClExpander::expandIndirectBranch(const MCInst &Inst,
                                                  MCStreamer &Out,
                                                  const MCSubtargetInfo &STI) {
  bool ThroughMemory = false, isCall = false;
  switch (Inst.getOpcode()) {
  case X86::CALL16m:
  case X86::CALL32m:
    ThroughMemory = true;
  case X86::CALL16r:
  case X86::CALL32r:
    isCall = true;
    break;
  case X86::JMP16m:
  case X86::JMP32m:
    ThroughMemory = true;
  case X86::JMP16r:
  case X86::JMP32r:
    break;
  default:
    llvm_unreachable("invalid indirect jmp/call");
  }

  MCOperand Target;
  if (ThroughMemory) {
    if (numScratchRegs() == 0) {
      Error(Inst, "No scratch registers specified");
      exit(1);
    }

    Target = MCOperand::CreateReg(getReg32(getScratchReg(0)));

    MCInst Mov;
    Mov.setOpcode(X86::MOV32rm);
    Mov.addOperand(Target);
    Mov.addOperand(Inst.getOperand(0)); // Base
    Mov.addOperand(Inst.getOperand(1)); // Scale
    Mov.addOperand(Inst.getOperand(2)); // Index
    Mov.addOperand(Inst.getOperand(3)); // Offset
    Mov.addOperand(Inst.getOperand(4)); // Segment
    Out.EmitInstruction(Mov, STI);
  } else {
    Target = MCOperand::CreateReg(getReg32(Inst.getOperand(0).getReg()));
  }

  Out.EmitBundleLock(isCall);

  MCInst And;
  And.setOpcode(X86::AND32ri8);
  And.addOperand(Target);
  And.addOperand(Target);
  And.addOperand(MCOperand::CreateImm(-kBundleSize));
  Out.EmitInstruction(And, STI);

  MCInst Branch;
  Branch.setOpcode(isCall ? X86::CALL32r : X86::JMP32r);
  Branch.addOperand(Target);
  Out.EmitInstruction(Branch, STI);

  Out.EmitBundleUnlock();
}

void X86::X86MCNaClExpander::expandReturn(const MCInst &Inst, MCStreamer &Out,
                                          const MCSubtargetInfo &STI) {
  if (numScratchRegs() == 0) {
    Error(Inst, "No scratch registers specified.");
    exit(1);
  }

  MCOperand ScratchReg = MCOperand::CreateReg(getReg32(getScratchReg(0)));
  MCInst Pop;
  Pop.setOpcode(X86::POP32r);
  Pop.addOperand(ScratchReg);
  Out.EmitInstruction(Pop, STI);

  if (Inst.getNumOperands() > 0) {
    assert(Inst.getOpcode() == X86::RETIL);
    MCInst Add;
    Add.setOpcode(X86::ADD32ri);
    Add.addOperand(MCOperand::CreateReg(X86::ESP));
    Add.addOperand(MCOperand::CreateReg(X86::ESP));
    Add.addOperand(Inst.getOperand(0));
    Out.EmitInstruction(Add, STI);
  }

  MCInst Jmp;
  Jmp.setOpcode(X86::JMP32r);
  Jmp.addOperand(ScratchReg);
  expandIndirectBranch(Jmp, Out, STI);
}

static bool isPrefix(const MCInst &Inst) {
  switch (Inst.getOpcode()) {
  case X86::LOCK_PREFIX:
  case X86::REP_PREFIX:
  case X86::REPNE_PREFIX:
  case X86::REX64_PREFIX:
    return true;
  default:
    return false;
  }
}

void X86::X86MCNaClExpander::emitPrefixes(MCStreamer &Out,
                                          const MCSubtargetInfo &STI) {
  for (const MCInst &Prefix : Prefixes)
    Out.EmitInstruction(Prefix, STI);
  Prefixes.clear();
}

void X86::X86MCNaClExpander::doExpandInst(const MCInst &Inst, MCStreamer &Out,
                                          const MCSubtargetInfo &STI) {
  if (isPrefix(Inst)) {
    Prefixes.push_back(Inst);
  } else {
    switch (Inst.getOpcode()) {
    case X86::CALL16r:
    case X86::CALL32r:
    case X86::CALL16m:
    case X86::CALL32m:
    case X86::JMP16r:
    case X86::JMP32r:
    case X86::JMP16m:
    case X86::JMP32m:
      return expandIndirectBranch(Inst, Out, STI);
    case X86::RETL:
    case X86::RETIL:
      return expandReturn(Inst, Out, STI);
    default:
      emitPrefixes(Out, STI);
      Out.EmitInstruction(Inst, STI);
    }
  }
}

bool X86::X86MCNaClExpander::expandInst(const MCInst &Inst, MCStreamer &Out,
                                        const MCSubtargetInfo &STI) {
  if (Guard)
    return false;
  Guard = true;

  doExpandInst(Inst, Out, STI);

  Guard = false;
  return true;
}
