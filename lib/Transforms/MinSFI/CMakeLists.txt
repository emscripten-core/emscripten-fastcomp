add_llvm_library(LLVMMinSFITransforms
  SandboxMemoryAccesses.cpp
  )

add_dependencies(LLVMMinSFITransforms LLVMNaClTransforms)
