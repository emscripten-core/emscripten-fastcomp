include_directories( ${CMAKE_CURRENT_BINARY_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/.. )

add_llvm_library(LLVMPNaClTransforms
  AddPNaClExternalDecls.cpp
  BackendCanonicalize.cpp
  CanonicalizeMemIntrinsics.cpp
  CleanupUsedGlobalsMetadata.cpp
  ConstantInsertExtractElementIndex.cpp
  ExceptionInfoWriter.cpp
  ExpandArithWithOverflow.cpp
  ExpandByVal.cpp
  ExpandConstantExpr.cpp
  ExpandCtors.cpp
  ExpandGetElementPtr.cpp
  ExpandIndirectBr.cpp
  ExpandLargeIntegers.cpp
  ExpandShuffleVector.cpp
  ExpandSmallArguments.cpp
  ExpandStructRegs.cpp
  ExpandTls.cpp
  ExpandTlsConstantExpr.cpp
  ExpandUtils.cpp
  ExpandVarArgs.cpp
  FixVectorLoadStoreAlignment.cpp
  FlattenGlobals.cpp
  SimplifiedFuncTypeMap.cpp
  GlobalCleanup.cpp
  GlobalizeConstantVectors.cpp
  InsertDivideCheck.cpp
  InternalizeUsedGlobals.cpp
  NormalizeAlignment.cpp
  PNaClSjLjEH.cpp
  PromoteI1Ops.cpp
  PromoteIntegers.cpp
  RemoveAsmMemory.cpp
  ReplacePtrsWithInts.cpp
  ResolvePNaClIntrinsics.cpp
  RewriteAtomics.cpp
  RewriteLLVMIntrinsics.cpp
  RewritePNaClLibraryCalls.cpp
  SimplifyAllocas.cpp
  SimplifyStructRegSignatures.cpp
  StripAttributes.cpp
  StripMetadata.cpp
  # Emscripten files:
  ExpandI64.cpp
  ExpandInsertExtractElement.cpp
  LowerEmAsyncify.cpp
  LowerEmExceptionsPass.cpp
  LowerEmSetjmp.cpp
  NoExitRuntime.cpp
  # Emscripten files end.
  )

add_dependencies(LLVMPNaClTransforms intrinsics_gen)
