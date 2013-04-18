add_llvm_library(LLVMnacltransforms
  ExpandConstantExpr.cpp
  ExpandCtors.cpp
  ExpandGetElementPtr.cpp
  ExpandTls.cpp
  ExpandTlsConstantExpr.cpp
  ExpandUtils.cpp
  ExpandVarArgs.cpp
  GlobalCleanup.cpp
  StripMetadata.cpp
  )

add_dependencies(LLVMnacltransforms intrinsics_gen)
