//===-- ARMAsmBackendNaClELF.h  ARM Asm Backend NaCl ELF --------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_ARM_ARMASMBACKENDNACLELF_H
#define LLVM_LIB_TARGET_ARM_ARMASMBACKENDNACLELF_H

// This whole file is a @LOCALMOD
#include "MCTargetDesc/ARMAsmBackendELF.h"
#include "MCTargetDesc/ARMMCNaCl.h"

using namespace llvm;

namespace {
class ARMAsmBackendNaClELF : public ARMAsmBackendELF {
public:
  ARMAsmBackendNaClELF(const Target &T, const StringRef TT, uint8_t _OSABI,
                    bool isLittle)
      : ARMAsmBackendELF(T, TT, _OSABI, isLittle),
        STI(ARM_MC::createARMMCSubtargetInfo(TT, "", "")) {
    assert(isLittle && "NaCl only supports little-endian");
    State.SaveCount = 0;
    State.I = 0;
    State.RecursiveCall = false;
  }

  ~ARMAsmBackendNaClELF() override {}

  bool CustomExpandInst(const MCInst &Inst, MCStreamer &Out) {
    return CustomExpandInstNaClARM(*STI, Inst, Out, State);
  }

private:
  // TODO(jfb) When upstreaming this class we can drop STI since ARMAsmBackend
  //           already has one. It's unfortunately private so we recreate one
  //           here to avoid the extra localmod.
  std::unique_ptr<MCSubtargetInfo> STI;
  ARMMCNaClSFIState State;
};
} // end anonymous namespace

#endif
