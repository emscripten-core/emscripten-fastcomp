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
  // LowerInvoke prevents use of C++ exception handling, which is not
  // yet supported in the PNaCl ABI.
  PM.add(createLowerInvokePass());
  // Remove landingpad blocks made unreachable by LowerInvoke.
  PM.add(createCFGSimplificationPass());

  PM.add(createExpandVarArgsPass());
  PM.add(createExpandCtorsPass());
  PM.add(createResolveAliasesPass());
  PM.add(createExpandTlsPass());
  // GlobalCleanup needs to run after ExpandTls because
  // __tls_template_start etc. are extern_weak before expansion
  PM.add(createGlobalCleanupPass());
  // Strip dead prototytes to appease the intrinsic ABI checks
  // (ExpandVarArgs leaves around var-arg intrinsics).
  PM.add(createStripDeadPrototypesPass());
}

void llvm::PNaClABISimplifyAddPostOptPasses(PassManager &PM) {
  // We place ExpandByVal after optimization passes because some byval
  // arguments can be expanded away by the ArgPromotion pass.  Leaving
  // in "byval" during optimization also allows some dead stores to be
  // eliminated, because "byval" is a stronger constraint than what
  // ExpandByVal expands it to.
  PM.add(createExpandByValPass());

  // We place StripMetadata after optimization passes because
  // optimizations depend on the metadata.
  PM.add(createStripMetadataPass());

  // FlattenGlobals introduces ConstantExpr bitcasts of globals which
  // are expanded out later.
  PM.add(createFlattenGlobalsPass());

  // We should not place arbitrary passes after ExpandConstantExpr
  // because they might reintroduce ConstantExprs.
  PM.add(createExpandConstantExprPass());
  // ExpandGetElementPtr must follow ExpandConstantExpr to expand the
  // getelementptr instructions it creates.
  PM.add(createExpandGetElementPtrPass());
  // ReplacePtrsWithInts assumes that getelementptr instructions and
  // ConstantExprs have already been expanded out.
  PM.add(createReplacePtrsWithIntsPass());
}
