add_llvm_library(LLVMTransformsNaCl
  ExpandConstantExpr.cpp
  ExpandCtors.cpp
  ExpandTls.cpp
  ExpandTlsConstantExpr.cpp
  ExpandVarArgs.cpp
  GlobalCleanup.cpp
  )

add_dependencies(LLVMTransformsNaCl intrinsics_gen)
