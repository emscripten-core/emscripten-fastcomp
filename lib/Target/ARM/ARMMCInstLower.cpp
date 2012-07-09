//===-- ARMMCInstLower.cpp - Convert ARM MachineInstr to an MCInst --------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains code to lower ARM MachineInstrs to their corresponding
// MCInst records.
//
//===----------------------------------------------------------------------===//

#include "ARM.h"
#include "ARMAsmPrinter.h"
#include "MCTargetDesc/ARMMCExpr.h"
#include "llvm/Constants.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"
#include "llvm/Target/Mangler.h"
using namespace llvm;


MCOperand ARMAsmPrinter::GetSymbolRef(const MachineOperand &MO,
                                      const MCSymbol *Symbol) {
  const MCExpr *Expr;
  switch (MO.getTargetFlags()) {
  default: {
    Expr = MCSymbolRefExpr::Create(Symbol, MCSymbolRefExpr::VK_None,
                                   OutContext);
    switch (MO.getTargetFlags()) {
    default: llvm_unreachable("Unknown target flag on symbol operand");
    case 0:
      break;
    case ARMII::MO_LO16:
      Expr = MCSymbolRefExpr::Create(Symbol, MCSymbolRefExpr::VK_None,
                                     OutContext);
      Expr = ARMMCExpr::CreateLower16(Expr, OutContext);
      break;
    case ARMII::MO_HI16:
      Expr = MCSymbolRefExpr::Create(Symbol, MCSymbolRefExpr::VK_None,
                                     OutContext);
      Expr = ARMMCExpr::CreateUpper16(Expr, OutContext);
      break;
    }
    break;
  }

  case ARMII::MO_PLT:
    Expr = MCSymbolRefExpr::Create(Symbol, MCSymbolRefExpr::VK_ARM_PLT,
                                   OutContext);
    break;
  }

  if (!MO.isJTI() && MO.getOffset())
    Expr = MCBinaryExpr::CreateAdd(Expr,
                                   MCConstantExpr::Create(MO.getOffset(),
                                                          OutContext),
                                   OutContext);
  return MCOperand::CreateExpr(Expr);

}

bool ARMAsmPrinter::lowerOperand(const MachineOperand &MO,
                                 MCOperand &MCOp) {
  switch (MO.getType()) {
  default: llvm_unreachable("unknown operand type");
  case MachineOperand::MO_Register:
    // Ignore all non-CPSR implicit register operands.
    if (MO.isImplicit() && MO.getReg() != ARM::CPSR)
      return false;
    assert(!MO.getSubReg() && "Subregs should be eliminated!");
    MCOp = MCOperand::CreateReg(MO.getReg());
    break;
  case MachineOperand::MO_Immediate:
    MCOp = MCOperand::CreateImm(MO.getImm());
    break;
  case MachineOperand::MO_MachineBasicBlock:
    MCOp = MCOperand::CreateExpr(MCSymbolRefExpr::Create(
        MO.getMBB()->getSymbol(), OutContext));
    break;
  case MachineOperand::MO_GlobalAddress:
    MCOp = GetSymbolRef(MO, Mang->getSymbol(MO.getGlobal()));
    break;
  case MachineOperand::MO_ExternalSymbol:
   MCOp = GetSymbolRef(MO,
                        GetExternalSymbolSymbol(MO.getSymbolName()));
    break;
  case MachineOperand::MO_JumpTableIndex:
    MCOp = GetSymbolRef(MO, GetJTISymbol(MO.getIndex()));
    break;
  case MachineOperand::MO_ConstantPoolIndex:
    MCOp = GetSymbolRef(MO, GetCPISymbol(MO.getIndex()));
    break;
  case MachineOperand::MO_BlockAddress:
    MCOp = GetSymbolRef(MO, GetBlockAddressSymbol(MO.getBlockAddress()));
    break;
  case MachineOperand::MO_FPImmediate: {
    APFloat Val = MO.getFPImm()->getValueAPF();
    bool ignored;
    Val.convert(APFloat::IEEEdouble, APFloat::rmTowardZero, &ignored);
    MCOp = MCOperand::CreateFPImm(Val.convertToDouble());
    break;
  }
  case MachineOperand::MO_RegisterMask:
    // Ignore call clobbers.
    return false;
  }
  return true;
}

void llvm::LowerARMMachineInstrToMCInst(const MachineInstr *MI, MCInst &OutMI,
                                        ARMAsmPrinter &AP) {
  OutMI.setOpcode(MI->getOpcode());

  for (unsigned i = 0, e = MI->getNumOperands(); i != e; ++i) {
    const MachineOperand &MO = MI->getOperand(i);

    MCOperand MCOp;
    if (AP.lowerOperand(MO, MCOp))
      OutMI.addOperand(MCOp);
  }
}

// @LOCALMOD-BEGIN
// Unlike LowerARMMachineInstrToMCInst, the opcode has already been set.
// Otherwise, this is like LowerARMMachineInstrToMCInst, but with special
// handling where the "immediate" is PC Relative
// (used for MOVi16PIC / MOVTi16PIC, etc. -- see .td file)
void llvm::LowerARMMachineInstrToMCInstPCRel(const MachineInstr *MI,
                                             MCInst &OutMI,
                                             ARMAsmPrinter &AP,
                                             unsigned ImmIndex,
                                             unsigned PCIndex,
                                             MCSymbol *PCLabel,
                                             unsigned PCAdjustment) {

  for (unsigned i = 0, e = MI->getNumOperands(); i != e; ++i) {
    if (i == ImmIndex) {
      MCContext &Ctx = AP.OutContext;
      const MCExpr *PCRelExpr = MCSymbolRefExpr::Create(PCLabel, Ctx);
      if (PCAdjustment) {
        const MCExpr *AdjExpr = MCConstantExpr::Create(PCAdjustment, Ctx);
        PCRelExpr = MCBinaryExpr::CreateAdd(PCRelExpr, AdjExpr, Ctx);
      }

      // Get the usual symbol operand, then subtract the PCRelExpr.
      const MachineOperand &MOImm = MI->getOperand(ImmIndex);
      MCOperand SymOp;
      bool DidLower = AP.lowerOperand(MOImm, SymOp);
      assert (DidLower && "Immediate-like operand should have been lowered");

      const MCExpr *Expr = SymOp.getExpr();
      ARMMCExpr::VariantKind TargetKind = ARMMCExpr::VK_ARM_None;
      /* Unwrap and rewrap the ARMMCExpr */
      if (Expr->getKind() == MCExpr::Target) {
        const ARMMCExpr *TargetExpr = cast<ARMMCExpr>(Expr);
        TargetKind = TargetExpr->getKind();
        Expr = TargetExpr->getSubExpr();
      }
      Expr = MCBinaryExpr::CreateSub(Expr, PCRelExpr, Ctx);
      if (TargetKind != ARMMCExpr::VK_ARM_None) {
        Expr = ARMMCExpr::Create(TargetKind, Expr, Ctx);
      }
      MCOperand MCOp = MCOperand::CreateExpr(Expr);
      OutMI.addOperand(MCOp);
    } else if (i == PCIndex) {  // dummy index already handled as PCLabel
      continue;
    } else {
      MCOperand MCOp;
      if (AP.lowerOperand(MI->getOperand(i), MCOp)) {
        OutMI.addOperand(MCOp);
      }
    }
  }
}
// @LOCALMOD-END
