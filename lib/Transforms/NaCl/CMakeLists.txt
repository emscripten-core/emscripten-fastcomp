add_llvm_library(LLVMTransformsNaCl
  ExpandCtors.cpp
  )

add_dependencies(LLVMTransformsNaCl intrinsics_gen)
