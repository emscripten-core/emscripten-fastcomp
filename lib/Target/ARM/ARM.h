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

#ifndef TARGET_ARM_H
#define TARGET_ARM_H

#include "MCTargetDesc/ARMBaseInfo.h"
#include "MCTargetDesc/ARMMCTargetDesc.h"
#include "llvm/Support/DataTypes.h"
#include "llvm/Target/TargetMachine.h"

// @LOCALMOD (for LowerARMMachineInstrToMCInstPCRel)
#include "llvm/MC/MCSymbol.h"

namespace llvm {

class ARMAsmPrinter;
class ARMBaseTargetMachine;
class FunctionPass;
class JITCodeEmitter;
class MachineInstr;
class MCInst;

FunctionPass *createARMISelDag(ARMBaseTargetMachine &TM,
                               CodeGenOpt::Level OptLevel);

FunctionPass *createARMJITCodeEmitterPass(ARMBaseTargetMachine &TM,
                                          JITCodeEmitter &JCE);

FunctionPass *createA15SDOptimizerPass();
FunctionPass *createARMLoadStoreOptimizationPass(bool PreAlloc = false);
FunctionPass *createARMExpandPseudoPass();
FunctionPass *createARMGlobalBaseRegPass();
FunctionPass *createARMGlobalMergePass(const TargetLowering* tli);
FunctionPass *createARMConstantIslandPass();
FunctionPass *createMLxExpansionPass();
FunctionPass *createThumb2ITBlockPass();
FunctionPass *createThumb2SizeReductionPass();

/* @LOCALMOD-START */
FunctionPass *createARMNaClRewritePass();
/* @LOCALMOD-END */

/// \brief Creates an ARM-specific Target Transformation Info pass.
ImmutablePass *createARMTargetTransformInfoPass(const ARMBaseTargetMachine *TM);


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
