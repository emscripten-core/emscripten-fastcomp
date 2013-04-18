add_llvm_library(LLVMnaclanalysis
  PNaClABITypeChecker.cpp
  PNaClABIVerifyFunctions.cpp
  PNaClABIVerifyModule.cpp
  )

add_dependencies(LLVMnaclanalysis intrinsics_gen)
