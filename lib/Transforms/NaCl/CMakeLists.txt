add_llvm_library(LLVMTransformsNaCl
  ExpandCtors.cpp
  ExpandTls.cpp
  ExpandTlsConstantExpr.cpp
  )

add_dependencies(LLVMTransformsNaCl intrinsics_gen)
