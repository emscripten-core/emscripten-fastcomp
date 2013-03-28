add_llvm_library(LLVMTransformsNaCl
  ExpandConstantExpr.cpp
  ExpandCtors.cpp
  ExpandGetElementPtr.cpp
  ExpandTls.cpp
  ExpandTlsConstantExpr.cpp
  ExpandVarArgs.cpp
  GlobalCleanup.cpp
  StripMetadata.cpp
  )

add_dependencies(LLVMTransformsNaCl intrinsics_gen)
