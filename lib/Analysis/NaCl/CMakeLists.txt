add_llvm_library(LLVMNaClAnalysis
  PNaClABITypeChecker.cpp
  PNaClABIVerifyFunctions.cpp
  PNaClABIVerifyModule.cpp
  )

add_dependencies(LLVMNaClAnalysis intrinsics_gen)
