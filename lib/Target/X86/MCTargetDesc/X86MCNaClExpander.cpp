#include "llvm/MC/MCNaClExpander.h"
#include "X86MCNaClExpander.h"
#include "X86BaseInfo.h"

using namespace llvm;

const int kNaClX86InstructionBundleSize = 32;

static MCInst getMaskInst(const MCOperand &Target, int mask);
static MCInst getPopInst(const MCOperand &Reg);
static MCInst getJmpRegInst(const MCOperand &TargetReg);

static MCInst getMaskInst(const MCOperand &Target, int mask) {
  MCInst Inst;
  Inst.setOpcode(X86::AND32ri8);
  Inst.addOperand(Target);
  Inst.addOperand(Target);
  Inst.addOperand(MCOperand::CreateImm(mask));
  return Inst;
}

static MCInst getPopInst(const MCOperand &Reg) {
  MCInst Inst;
  Inst.setOpcode(X86::POP32r);
  Inst.addOperand(Reg);
  return Inst;
}

static MCInst getJmpRegInst(const MCOperand &Target) {
  MCInst Inst;
  Inst.setOpcode(X86::JMP32r);
  Inst.addOperand(Target);
  return Inst;
}

void X86::X86MCNaClExpander::emitReturn(const MCInst &Inst, MCStreamer &Out,
                                        const MCSubtargetInfo &STI) {
  MCOperand scratchReg = MCOperand::CreateReg(X86::ECX);
  Out.EmitBundleLock(false);
  Out.EmitInstruction(getPopInst(scratchReg), STI);
  Out.EmitInstruction(getMaskInst(scratchReg, -kNaClX86InstructionBundleSize),
                      STI);
  if (Inst.getOpcode() == RETIL) {
    MCInst ADDInst;
    ADDInst.setOpcode(X86::ADD32ri);
    ADDInst.addOperand(MCOperand::CreateReg(X86::ESP));
    ADDInst.addOperand(MCOperand::CreateReg(X86::ESP));
    ADDInst.addOperand(Inst.getOperand(0));
    Out.EmitInstruction(ADDInst, STI);
  }
  Out.EmitInstruction(getJmpRegInst(scratchReg), STI);

  Out.EmitBundleUnlock();
}

void X86::X86MCNaClExpander::doExpandInst(const MCInst &Inst, MCStreamer &Out,
                                          const MCSubtargetInfo &STI) {
  unsigned opcode = Inst.getOpcode();
  switch (opcode) {
  case X86::RETL:
  case X86::RETIL:
    emitReturn(Inst, Out, STI);
    return;

  default:
    Out.EmitInstruction(Inst, STI);
  }
}

bool X86::X86MCNaClExpander::expandInst(const MCInst &Inst, MCStreamer &Out,
                                        const MCSubtargetInfo &STI) {
  if (guard)
    return false;
  guard = true;

  doExpandInst(Inst, Out, STI);

  guard = false;
  return true;
}
