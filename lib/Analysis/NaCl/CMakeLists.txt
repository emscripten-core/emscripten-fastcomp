add_llvm_library(LLVMAnalysisNaCl
  PNaClABIVerifyFunctions.cpp
  PNaClABIVerifyModule.cpp
  )

add_dependencies(LLVMAnalysisNaCl intrinsics_gen)
