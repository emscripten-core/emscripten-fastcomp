add_llvm_library(LLVMAnalysisNaCl
  PNaCLABITypeChecker.cpp
  PNaClABIVerifyFunctions.cpp
  PNaClABIVerifyModule.cpp
  )

add_dependencies(LLVMAnalysisNaCl intrinsics_gen)
