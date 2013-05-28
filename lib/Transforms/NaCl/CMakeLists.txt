set(LLVM_LINK_COMPONENTS ipo)

add_llvm_library(LLVMNaClTransforms
  AddPNaClExternalDecls.cpp
  ExpandArithWithOverflow.cpp
  ExpandByVal.cpp
  ExpandConstantExpr.cpp
  ExpandCtors.cpp
  ExpandGetElementPtr.cpp
  ExpandTls.cpp
  ExpandTlsConstantExpr.cpp
  ExpandUtils.cpp
  ExpandVarArgs.cpp
  InsertDivideCheck.cpp
  FlattenGlobals.cpp
  GlobalCleanup.cpp
  PNaClABISimplify.cpp
  PromoteIntegers.cpp
  ReplacePtrsWithInts.cpp
  ResolvePNaClIntrinsics.cpp
  RewritePNaClLibraryCalls.cpp
  StripMetadata.cpp
  )

add_dependencies(LLVMNaClTransforms intrinsics_gen)
