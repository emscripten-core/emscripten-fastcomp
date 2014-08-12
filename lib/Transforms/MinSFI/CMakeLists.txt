add_llvm_library(LLVMMinSFITransforms
  ExpandAllocas.cpp
  SandboxMemoryAccesses.cpp
  AllocateDataSegment.cpp
  )

add_dependencies(LLVMMinSFITransforms LLVMNaClTransforms)
