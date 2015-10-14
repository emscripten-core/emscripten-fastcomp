//===-- ARM.h - Top-level interface for ARM representation ------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains the entry points for global functions defined in the LLVM
// ARM back-end.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ARM_ARM_H
#define LLVM_LIB_TARGET_ARM_ARM_H

#include "llvm/Support/CodeGen.h"
#include <functional>

// @LOCALMOD (for LowerARMMachineInstrToMCInstPCRel)
#include "llvm/MC/MCSymbol.h"

namespace llvm {

class ARMAsmPrinter;
class ARMBaseTargetMachine;
class Function;
class FunctionPass;
class ImmutablePass;
class MachineInstr;
class MCInst;
class TargetLowering;
class TargetMachine;

FunctionPass *createARMISelDag(ARMBaseTargetMachine &TM,
                               CodeGenOpt::Level OptLevel);
FunctionPass *createA15SDOptimizerPass();
FunctionPass *createARMLoadStoreOptimizationPass(bool PreAlloc = false);
FunctionPass *createARMExpandPseudoPass();
FunctionPass *createARMGlobalBaseRegPass();
FunctionPass *createARMConstantIslandPass();
FunctionPass *createMLxExpansionPass();
FunctionPass *createThumb2ITBlockPass();
FunctionPass *createARMOptimizeBarriersPass();
FunctionPass *createThumb2SizeReductionPass(
    std::function<bool(const Function &)> Ftor = nullptr);

/* @LOCALMOD-START */
FunctionPass *createARMNaClRewritePass();
/* @LOCALMOD-END */

void LowerARMMachineInstrToMCInst(const MachineInstr *MI, MCInst &OutMI,
                                  ARMAsmPrinter &AP);

/* @LOCALMOD-START */
// Used to lower the pc-relative MOVi16PIC / MOVTi16PIC pseudo instructions
// into the real MOVi16 / MOVTi16 instructions.
// See comment on MOVi16PIC for more details.
void LowerARMMachineInstrToMCInstPCRel(const MachineInstr *MI,
                                       MCInst &OutMI,
                                       ARMAsmPrinter &AP,
                                       unsigned ImmIndex,
                                       unsigned PCIndex,
                                       MCSymbol *PCLabel,
                                       unsigned PCAdjustment);
/* @LOCALMOD-END */

} // end namespace llvm;

#endif
