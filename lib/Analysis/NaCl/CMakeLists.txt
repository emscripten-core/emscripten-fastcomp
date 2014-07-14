add_llvm_library(LLVMNaClAnalysis
  PNaClABITypeChecker.cpp
  PNaClABIProps.cpp
  PNaClABIVerifyFunctions.cpp
  PNaClABIVerifyModule.cpp
  PNaClAllowedIntrinsics.cpp
  )

add_dependencies(LLVMNaClAnalysis intrinsics_gen)
