add_llvm_library(LLVMNaClTransforms
  ExpandConstantExpr.cpp
  ExpandCtors.cpp
  ExpandGetElementPtr.cpp
  ExpandTls.cpp
  ExpandTlsConstantExpr.cpp
  ExpandUtils.cpp
  ExpandVarArgs.cpp
  FlattenGlobals.cpp
  GlobalCleanup.cpp
  StripMetadata.cpp
  )

add_dependencies(LLVMNaClTransforms intrinsics_gen)
