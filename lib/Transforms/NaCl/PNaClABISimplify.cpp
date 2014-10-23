//===-- PNaClABISimplify.cpp - Lists PNaCl ABI simplification passes ------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the meta-passes "-pnacl-abi-simplify-preopt"
// and "-pnacl-abi-simplify-postopt".  It lists their constituent
// passes.
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/NaCl.h"
#include "llvm/PassManager.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/NaCl.h"
#include "llvm/Transforms/Scalar.h"

using namespace llvm;

static cl::opt<bool>
EnableSjLjEH("enable-pnacl-sjlj-eh",
             cl::desc("Enable use of SJLJ-based C++ exception handling "
                      "as part of the pnacl-abi-simplify passes"),
             cl::init(false));

void llvm::PNaClABISimplifyAddPreOptPasses(PassManagerBase &PM) {
  if (EnableSjLjEH) {
    // This comes before ExpandTls because it introduces references to
    // a TLS variable, __pnacl_eh_stack.  This comes before
    // InternalizePass because it assumes various variables (including
    // __pnacl_eh_stack) have not been internalized yet.
    PM.add(createPNaClSjLjEHPass());
  } else {
    // LowerInvoke prevents use of C++ exception handling by removing
    // references to BasicBlocks which handle exceptions.
    PM.add(createLowerInvokePass());
    // Remove landingpad blocks made unreachable by LowerInvoke.
    PM.add(createCFGSimplificationPass());
  }

  // Internalize all symbols in the module except the entry point.  A PNaCl
  // pexe is only allowed to export "_start", whereas a PNaCl PSO is only
  // allowed to export "__pnacl_pso_root".
  const char *SymbolsToPreserve[] = { "_start", "__pnacl_pso_root" };
  PM.add(createInternalizePass(SymbolsToPreserve));

  // Expand out computed gotos (indirectbr and blockaddresses) into switches.
  PM.add(createExpandIndirectBrPass());

  // LowerExpect converts Intrinsic::expect into branch weights,
  // which can then be removed after BlockPlacement.
  PM.add(createLowerExpectIntrinsicPass());
  // Rewrite unsupported intrinsics to simpler and portable constructs.
  PM.add(createRewriteLLVMIntrinsicsPass());

  // Expand out some uses of struct types.
  PM.add(createExpandVarArgsPass());
  PM.add(createExpandArithWithOverflowPass());
  // ExpandStructRegs must be run after ExpandArithWithOverflow to
  // expand out the insertvalue instructions that
  // ExpandArithWithOverflow introduces.  ExpandStructRegs must be run
  // after ExpandVarArgs so that struct-typed "va_arg" instructions
  // have been removed.
  PM.add(createExpandStructRegsPass());

  PM.add(createExpandCtorsPass());
  PM.add(createResolveAliasesPass());
  PM.add(createExpandTlsPass());
  // GlobalCleanup needs to run after ExpandTls because
  // __tls_template_start etc. are extern_weak before expansion
  PM.add(createGlobalCleanupPass());
}

void llvm::PNaClABISimplifyAddPostOptPasses(PassManagerBase &PM) {
  PM.add(createRewritePNaClLibraryCallsPass());

  // We place ExpandByVal after optimization passes because some byval
  // arguments can be expanded away by the ArgPromotion pass.  Leaving
  // in "byval" during optimization also allows some dead stores to be
  // eliminated, because "byval" is a stronger constraint than what
  // ExpandByVal expands it to.
  PM.add(createExpandByValPass());

  // We place ExpandSmallArguments after optimization passes because
  // some optimizations undo its changes.  Note that
  // ExpandSmallArguments requires that ExpandVarArgs has already been
  // run.
  PM.add(createExpandSmallArgumentsPass());

  PM.add(createPromoteI1OpsPass());

  // Vector simplifications.
  //
  // The following pass relies on ConstantInsertExtractElementIndex running
  // after it, and it must run before GlobalizeConstantVectors because the mask
  // argument of shufflevector must be a constant (the pass would otherwise
  // violate this requirement).
  PM.add(createExpandShuffleVectorPass());
  // We should not place arbitrary passes after ExpandConstantExpr
  // because they might reintroduce ConstantExprs.
  PM.add(createExpandConstantExprPass());
  // GlobalizeConstantVectors does not handle nested ConstantExprs, so we
  // run ExpandConstantExpr first.
  PM.add(createGlobalizeConstantVectorsPass());
  // The following pass inserts GEPs, it must precede ExpandGetElementPtr. It
  // also creates vector loads and stores, the subsequent pass cleans them up to
  // fix their alignment.
  PM.add(createConstantInsertExtractElementIndexPass());
  PM.add(createFixVectorLoadStoreAlignmentPass());

  // Optimization passes and ExpandByVal introduce
  // memset/memcpy/memmove intrinsics with a 64-bit size argument.
  // This pass converts those arguments to 32-bit.
  PM.add(createCanonicalizeMemIntrinsicsPass());

  // We place StripMetadata after optimization passes because
  // optimizations depend on the metadata.
  PM.add(createStripMetadataPass());

  // ConstantMerge cleans up after passes such as GlobalizeConstantVectors. It
  // must run before the FlattenGlobals pass because FlattenGlobals loses
  // information that otherwise helps ConstantMerge do a good job.
  PM.add(createConstantMergePass());
  // FlattenGlobals introduces ConstantExpr bitcasts of globals which
  // are expanded out later. ReplacePtrsWithInts also creates some
  // ConstantExprs, and it locally creates an ExpandConstantExprPass
  // to clean both of these up.
  PM.add(createFlattenGlobalsPass());

  // PromoteIntegersPass does not handle constexprs and creates GEPs,
  // so it goes between those passes.
  PM.add(createPromoteIntegersPass());
  // ExpandGetElementPtr must follow ExpandConstantExpr to expand the
  // getelementptr instructions it creates.
  PM.add(createExpandGetElementPtrPass());
  // Rewrite atomic and volatile instructions with intrinsic calls.
  PM.add(createRewriteAtomicsPass());
  // Remove ``asm("":::"memory")``. This must occur after rewriting
  // atomics: a ``fence seq_cst`` surrounded by ``asm("":::"memory")``
  // has special meaning and is translated differently.
  PM.add(createRemoveAsmMemoryPass());
  // ReplacePtrsWithInts assumes that getelementptr instructions and
  // ConstantExprs have already been expanded out.
  PM.add(createReplacePtrsWithIntsPass());

  // The atomic cmpxchg instruction returns a struct, and is rewritten to an
  // intrinsic as a post-opt pass, we therefore need to expand struct regs one
  // last time.
  PM.add(createExpandStructRegsPass());

  // We place StripAttributes after optimization passes because many
  // analyses add attributes to reflect their results.
  // StripAttributes must come after ExpandByVal and
  // ExpandSmallArguments.
  PM.add(createStripAttributesPass());

  // Strip dead prototytes to appease the intrinsic ABI checks.
  // ExpandVarArgs leaves around vararg intrinsics, and
  // ReplacePtrsWithInts leaves the lifetime.start/end intrinsics.
  PM.add(createStripDeadPrototypesPass());

  // Eliminate simple dead code that the post-opt passes could have
  // created.
  PM.add(createDeadInstEliminationPass());
  PM.add(createDeadCodeEliminationPass());
}
