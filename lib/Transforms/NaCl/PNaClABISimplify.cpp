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

static cl::opt<bool> // XXX EMSCRIPTEN
EnableEmCxxExceptions("enable-emscripten-cxx-exceptions",
                      cl::desc("Enables C++ exceptions in emscripten"),
                      cl::init(false));

static cl::opt<bool> // XXX EMSCRIPTEN
EnableEmAsyncify("emscripten-asyncify",
                cl::desc("Enable asyncify transformation (see emscripten ASYNCIFY option)"),
                cl::init(false));


void llvm::PNaClABISimplifyAddPreOptPasses(PassManager &PM) {
  if (EnableSjLjEH) {
    // This comes before ExpandTls because it introduces references to
    // a TLS variable, __pnacl_eh_stack.  This comes before
    // InternalizePass because it assumes various variables (including
    // __pnacl_eh_stack) have not been internalized yet.
    PM.add(createPNaClSjLjEHPass());
  } else if (EnableEmCxxExceptions) { // XXX EMSCRIPTEN
    PM.add(createLowerEmExceptionsPass());
  } else {
    // LowerInvoke prevents use of C++ exception handling by removing
    // references to BasicBlocks which handle exceptions.
    PM.add(createLowerInvokePass());
    // Remove landingpad blocks made unreachable by LowerInvoke.
    PM.add(createCFGSimplificationPass());
  }

  PM.add(createLowerEmSetjmpPass()); // XXX EMSCRIPTEN

#if 0 // EMSCRIPTEN: we allow arbitrary symbols to be preserved
  // Internalize all symbols in the module except _start, which is the only
  // symbol a stable PNaCl pexe is allowed to export.
  const char *SymbolsToPreserve[] = { "_start" };
  PM.add(createInternalizePass(SymbolsToPreserve));
#endif

  // LowerExpect converts Intrinsic::expect into branch weights,
  // which can then be removed after BlockPlacement.
#if 0 // XXX EMSCRIPTEN: We support the expect intrinsic.
  PM.add(createLowerExpectIntrinsicPass());
#endif
#if 0 // XXX EMSCRIPTEN: We don't need this.
  // Rewrite unsupported intrinsics to simpler and portable constructs.
  PM.add(createRewriteLLVMIntrinsicsPass());
#endif

  PM.add(createExpandVarArgsPass());
  // Expand out some uses of struct types.
  PM.add(createExpandArithWithOverflowPass());
  // ExpandStructRegs must be run after ExpandArithWithOverflow to
  // expand out the insertvalue instructions that
  // ExpandArithWithOverflow introduces.  ExpandStructRegs must be run
  // after ExpandVarArgs so that struct-typed "va_arg" instructions
  // have been removed.
  PM.add(createExpandStructRegsPass());

  PM.add(createExpandCtorsPass());
#if 0 // XXX EMSCRIPTEN: We handle aliases.
  PM.add(createResolveAliasesPass());
#endif
#if 0 // EMSCRIPTEN: no need for tls
  PM.add(createExpandTlsPass());
#endif
  // GlobalCleanup needs to run after ExpandTls because
  // __tls_template_start etc. are extern_weak before expansion
#if 0 // XXX EMSCRIPTEN: We don't currently have tls, and we don't have the same complications with extern_weak
  PM.add(createGlobalCleanupPass());
#endif

  if (EnableEmAsyncify) // XXX EMSCRIPTEN
    PM.add(createLowerEmAsyncifyPass());
}

void llvm::PNaClABISimplifyAddPostOptPasses(PassManager &PM) {
#if 0 // XXX EMSCRIPTEN: No need for this.
  PM.add(createRewritePNaClLibraryCallsPass());
#endif

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
#if 0 // EMSCRIPTEN: we don't need to worry about the issue this works around
  PM.add(createExpandSmallArgumentsPass());
#endif
  PM.add(createPromoteI1OpsPass());

  // Optimization passes and ExpandByVal introduce
  // memset/memcpy/memmove intrinsics with a 64-bit size argument.
  // This pass converts those arguments to 32-bit.
  PM.add(createCanonicalizeMemIntrinsicsPass());

#if 0 // XXX EMSCRIPTEN: PNaCl strips metadata to avoid making it ABI-exposed; emscripten doesn't need this.
  // We place StripMetadata after optimization passes because
  // optimizations depend on the metadata.
  PM.add(createStripMetadataPass());
#endif

  // FlattenGlobals introduces ConstantExpr bitcasts of globals which
  // are expanded out later.
  PM.add(createFlattenGlobalsPass());

  // We should not place arbitrary passes after ExpandConstantExpr
  // because they might reintroduce ConstantExprs.
  PM.add(createExpandConstantExprPass());
  // PromoteIntegersPass does not handle constexprs and creates GEPs,
  // so it goes between those passes.
  PM.add(createPromoteIntegersPass());
#if 0 // XXX EMSCRIPTEN: We can handle GEPs in our backend.
  // ExpandGetElementPtr must follow ExpandConstantExpr to expand the
  // getelementptr instructions it creates.
  PM.add(createExpandGetElementPtrPass());
#endif
  // Rewrite atomic and volatile instructions with intrinsic calls.
#if 0 // EMSCRIPTEN: we don't need to fix volatiles etc, and can use llvm intrinsics
  PM.add(createRewriteAtomicsPass());
#endif
  // Remove ``asm("":::"memory")``. This must occur after rewriting
  // atomics: a ``fence seq_cst`` surrounded by ``asm("":::"memory")``
  // has special meaning and is translated differently.
#if 0 // XXX EMSCRIPTEN: asm("":::"memory") does't have special semantics.
  PM.add(createRemoveAsmMemoryPass());
#endif
#if 0 // XXX EMSCRIPTEN: PNaCl replaces pointers with ints to simplify their ABI; empscripten doesn't need this.
  // ReplacePtrsWithInts assumes that getelementptr instructions and
  // ConstantExprs have already been expanded out.
  PM.add(createReplacePtrsWithIntsPass());
#endif

  // We place StripAttributes after optimization passes because many
  // analyses add attributes to reflect their results.
  // StripAttributes must come after ExpandByVal and
  // ExpandSmallArguments.
#if 0 // EMSCRIPTEN: we don't need to worry about the issue this works around
  PM.add(createStripAttributesPass());
#endif

  // Strip dead prototytes to appease the intrinsic ABI checks.
  // ExpandVarArgs leaves around vararg intrinsics, and
  // ReplacePtrsWithInts leaves the lifetime.start/end intrinsics.
#if 0 // XXX EMSCRIPTEN: We just ignore dead prototypes.
  PM.add(createStripDeadPrototypesPass());
#endif

  // Eliminate simple dead code that the post-opt passes could have
  // created.
#if 0 // EMSCRIPTEN: There's no point in running this since we're running DeadCodeElimination right after.
  PM.add(createDeadInstEliminationPass());
#endif
  PM.add(createDeadCodeEliminationPass());
}

