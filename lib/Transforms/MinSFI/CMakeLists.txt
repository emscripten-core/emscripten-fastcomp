add_llvm_library(LLVMMinSFITransforms
  SandboxMemoryAccesses.cpp
  AllocateDataSegment.cpp
  )

add_dependencies(LLVMMinSFITransforms LLVMNaClTransforms)
