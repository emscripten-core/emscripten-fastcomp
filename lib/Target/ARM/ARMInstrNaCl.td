//====- ARMInstrNaCl.td - Describe NaCl Instructions ----*- tablegen -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file describes the modifications to the X86 instruction set needed for
// Native Client code generation.
//
//===----------------------------------------------------------------------===//


//===----------------------------------------------------------------------===//
//
//                       Native Client Pseudo-Instructions
//
// These instructions implement the Native Client pseudo-instructions, such
// as nacljmp and naclasp.
//
// TableGen and MC consider these to be "real" instructions. They can be
// parsed by the AsmParser and emitted by the AsmStreamer as if they
// were just regular instructions. They are not marked "Pseudo" because
// this would imply isCodeGenOnly=1, which would stop them from being
// parsed by the assembler.
//
// These instructions cannot be encoded (written into an object file) by the
// MCCodeEmitter. Instead, during direct object emission, they get lowered to
// a sequence of streamer emits. (see ARMInstrNaCl.cpp)
// 
// These instructions should not be used in CodeGen. They have no pattern
// and lack CodeGen metadata. Instead, the ARMNaClRewritePass should
// generate these instructions after CodeGen is finished.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// ARM Native Client "Pseudo" Instructions
//===----------------------------------------------------------------------===//

// It does not seem possible to define a single base class for both the 
// synthetic isCodeGenOnly=1 instructions as well as the isAsmParserOnly=1 
// versions.

// These are the fixed flags:
// AddrMode am = AddrModeNone
// SizeFlagVal sz = SizeSpecial
// IndexMode im = IndexModeNone
// Domain d = GenericDomain
// InstrItinClass = NoItinerary

// The non-fixed flags need their own class 
// InOperandList = !con(iops, (ins pred:$p)) or left alone
// AsmString = !strconcat(opc, "${p}", asm) or left alone
// Format f = MiscFrm/Pseudo
// isPredicable = 0/1 

/// However, it is possible to make a set of two base classes for the isAsmParserOnly=1
/// synthetic instructions.


/***** FIXME: ADD in isAsmParserOnly naclguard instructions ***************************
/// required args:
// dag outs, dag ins, string opc, string asm, string cstr, Format f, list<dag> pat

class NaClSI<dag outs, dag ins, string opc, string asm, string cstr> 
  : I<outs, ins, AddrModeNone, SizeSpecial, IndexModeNone, MiscFrm, 
              NoItinerary, opc, asm, cstr, pat>, Requires<[IsNaCl]>;

class NaClSINoP<dag outs, dag ins, string opc, string asm, string cstr> 
  : InoP <outs, ins, AddrModeNone, SizeSpecial, IndexModeNone, MiscFrm, 
              NoItinerary, opc, asm, cstr, pat>, Requires<[IsNaCl]>;

class NaClSI<dag outs, dag ins, string opc, string asm, string cstr, Format f, list<dag> pat> 
  : InstARM<AddrModeNone, SizeSpecial, IndexModeNone, f,
            GenericDomain, cstr, NoItinerary>, Requires<[IsNaCl]> {
  let OutOperandList = oops;
  let InOperandList = iops;
  let Pattern = pattern;
  let AsmString = !strconcat(opc, asm);
};


/// For not pseudo instructionable      
class NaClSINoP<dag outs, dag ins, string opc, string asm, string cstr, Format f, list<dag> pat> 
  : InstARM<AddrModeNone, SizeSpecial, IndexModeNone, f,
            GenericDomain, cstr, NoItinerary>, Requires<[IsNaCl]> {
  let OutOperandList = oops;
  let InOperandList = iops;
  let Pattern = pattern;
  let AsmString = !strconcat(opc, asm);
};

/// This is the guarded isCodeGenOnly pseudo instruction for BX_RET
let isReturn = 1, isTerminator = 1, isBarrier = 1, isCodeGenOnly = 1,
    // Make sure this is selected in lieu of 
    AddedComplexity = 1 
 in {
  // ARMV4T and above
  def NACL_CG_BX_RET : 
     ARMPseudoInst<(outs), (ins), BrMiscFrm, IIC_Br, 
                   "naclbx", "\tlr", [(ARMretflag)]>,
     Requires<[HasV4T, IsNaCl]> {
  }
}


// These are assembler only instructions 
let isAsmParserOnly = 1 in {
  def NACL_GUARD_LOADSTORE :
  NaClSI<(outs GPR:$dst), (ins GPR:$a), 
         "naclguard", "\t${dst}, ${a}", "" []>;

  let Defs = [CPSR] in
  def NACL_GUARD_LOADSTORE_TST :
    NaClSINoP<
  PseudoInst<(outs GPR:$dst), (ins GPR:$a), NoItinerary, []> ;


  let Defs = [CPSR] in
  def NACL_GUARD_LOADSTORE_TST :
  PseudoInst<(outs GPR:$dst), (ins GPR:$a), NoItinerary, []>;

  def NACL_GUARD_INDIRECT_CALL :
  PseudoInst<(outs GPR:$dst), (ins GPR:$a, pred:$p), NoItinerary, []>;

  def NACL_GUARD_INDIRECT_JMP :
  PseudoInst<(outs GPR:$dst), (ins GPR:$a, pred:$p), NoItinerary, []>;

  def NACL_GUARD_CALL :
  PseudoInst<(outs), (ins pred:$p), NoItinerary, []>;

  // NOTE: the BX_RET instruction hardcodes lr as well
  def NACL_GUARD_RETURN :
  PseudoInst<(outs), (ins pred:$p), NoItinerary, []>;

  // Note: intention is that $src and $dst are the same register.
  def NACL_DATA_MASK :
  PseudoInst<(outs GPR:$dst), (ins GPR:$src, pred:$p), NoItinerary, []>;
}


**************************************************************************/
