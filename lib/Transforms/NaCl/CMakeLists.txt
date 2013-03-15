add_llvm_library(LLVMTransformsNaCl
  ExpandConstantExpr.cpp
  ExpandCtors.cpp
  ExpandTls.cpp
  ExpandTlsConstantExpr.cpp
  )

add_dependencies(LLVMTransformsNaCl intrinsics_gen)
