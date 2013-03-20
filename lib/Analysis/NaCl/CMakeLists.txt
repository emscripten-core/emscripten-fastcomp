add_llvm_library(LLVMNaClAnalysis
  PNaCLABITypeChecker.cpp
  PNaClABIVerifyFunctions.cpp
  PNaClABIVerifyModule.cpp
  )

add_dependencies(LLVMNaClAnalysis intrinsics_gen)
