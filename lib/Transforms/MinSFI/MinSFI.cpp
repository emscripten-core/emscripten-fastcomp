//===-- MinSFI.cpp - Lists MinSFI sandboxing passes -----------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the meta-pass "-minsfi". It lists its constituent
// passes and explains the reasons for their ordering.
//
//===----------------------------------------------------------------------===//

#include "llvm/PassManager.h"
#include "llvm/Analysis/NaCl.h"
#include "llvm/Transforms/MinSFI.h"

using namespace llvm;

void llvm::MinSFIPasses(PassManagerBase &PM) {
  // Nondeterminism is generally undesirable in sandboxed code but more
  // importantly, use of undefined values can leak protected data. This pass
  // replaces all undefs with predefined constants. It only modifies operands
  // of instructions and therefore is not dependent on any other MinSFI or
  // PNaCl passes.
  PM.add(createSubstituteUndefsPass());

  // Most MinSFI passes rely on the safety properties guaranteed by the PNaCl
  // bitcode format. We run the PNaCl ABI verifier to make sure these hold.
  PNaClABIErrorReporter *ErrorReporter = new PNaClABIErrorReporter();
  PM.add(createPNaClABIVerifyModulePass(ErrorReporter, false));
  PM.add(createPNaClABIVerifyFunctionsPass(ErrorReporter));

  // The naming of NaCl's entry point causes a conflict when linking into
  // native executables. This pass renames the entry function to resolve it.
  // The pass must be invoked after the PNaCl ABI verifier but otherwise could
  // be invoked at any point. To avoid confusion, we rename the function
  // immediately after the verifier and have all the subsequent passes refer to
  // the new name.
  PM.add(createRenameEntryPointPass());

  // Sandboxed code cannot access memory allocated on the native stack. This
  // pass creates an untrusted stack inside the sandbox's memory region, with
  // the stack pointer stored in a global variable. With some modifications,
  // the pass could be invoked after SFI, allowing unsandboxed updates of the
  // stack pointer, but that would increase the size of the compiler-side TCB.
  PM.add(createExpandAllocasPass());

  // The data segment of the sandbox lies outside its memory region. This pass
  // generates a template, which the MinSFI runtime copies into the sandbox
  // during initialization. All globals defined before this pass therefore
  // remain addressable by the sandboxed code.
  PM.add(createAllocateDataSegmentPass());

  // Next, we apply SFI sandboxing to pointer-type operands of all memory
  // access instructions. The pass guarantees that the sandboxed code cannot
  // read or write beyond the scope of the memory region allocated to it.
  // All passes running before this one do not have to be trusted in this
  // respect. Passes running later must not break the guarantee.
  PM.add(createSandboxMemoryAccessesPass());

  // Lastly, we apply CFI sandboxing on indirect calls. The pass creates
  // tables of address-taken functions and replaces function pointers with
  // indices into the tables. This pass is invoked after SFI because it is
  // crucial that the tables cannot be modified by the sandboxed code.
  PM.add(createSandboxIndirectCallsPass());
}
