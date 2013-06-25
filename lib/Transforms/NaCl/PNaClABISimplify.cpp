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

void llvm::PNaClABISimplifyAddPreOptPasses(PassManager &PM) {
  // LowerExpect converts Intrinsic::expect into branch weights,
  // which can then be removed after BlockPlacement.
  PM.add(createLowerExpectIntrinsicPass());
  // Rewrite unsupported intrinsics to simpler constructs.
  PM.add(createRewriteLLVMIntrinsicsPass());
  // LowerInvoke prevents use of C++ exception handling, which is not
  // yet supported in the PNaCl ABI.
  PM.add(createLowerInvokePass());
  // Remove landingpad blocks made unreachable by LowerInvoke.
  PM.add(createCFGSimplificationPass());

  // Expand out some uses of struct types.
  PM.add(createExpandArithWithOverflowPass());
  // ExpandStructRegs must be run after ExpandArithWithOverflow to
  // expand out the insertvalue instructions that
  // ExpandArithWithOverflow introduces.
  PM.add(createExpandStructRegsPass());

  PM.add(createExpandVarArgsPass());
  PM.add(createExpandCtorsPass());
  PM.add(createResolveAliasesPass());
  PM.add(createExpandTlsPass());
  // GlobalCleanup needs to run after ExpandTls because
  // __tls_template_start etc. are extern_weak before expansion
  PM.add(createGlobalCleanupPass());
}

void llvm::PNaClABISimplifyAddPostOptPasses(PassManager &PM) {
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

  // We place StripMetadata after optimization passes because
  // optimizations depend on the metadata.
  PM.add(createStripMetadataPass());

  // FlattenGlobals introduces ConstantExpr bitcasts of globals which
  // are expanded out later.
  PM.add(createFlattenGlobalsPass());

  // We should not place arbitrary passes after ExpandConstantExpr
  // because they might reintroduce ConstantExprs.
  PM.add(createExpandConstantExprPass());
  // PromoteIntegersPass does not handle constexprs and creates GEPs,
  // so it goes between those passes.
  PM.add(createPromoteIntegersPass());
  // ExpandGetElementPtr must follow ExpandConstantExpr to expand the
  // getelementptr instructions it creates.
  PM.add(createExpandGetElementPtrPass());
  // ReplacePtrsWithInts assumes that getelementptr instructions and
  // ConstantExprs have already been expanded out.
  PM.add(createReplacePtrsWithIntsPass());

  // We place StripAttributes after optimization passes because many
  // analyses add attributes to reflect their results.
  // StripAttributes must come after ExpandByVal and
  // ExpandSmallArguments.
  PM.add(createStripAttributesPass());

  // Strip dead prototytes to appease the intrinsic ABI checks.
  // ExpandVarArgs leaves around vararg intrinsics, and
  // ReplacePtrsWithInts leaves the lifetime.start/end intrinsics.
  PM.add(createStripDeadPrototypesPass());
}
