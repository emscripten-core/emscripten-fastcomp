add_llvm_library(LLVMNaClTransforms
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
  PromoteIntegers.cpp
  RewritePNaClLibraryCalls.cpp
  StripMetadata.cpp
  )

add_dependencies(LLVMNaClTransforms intrinsics_gen)
