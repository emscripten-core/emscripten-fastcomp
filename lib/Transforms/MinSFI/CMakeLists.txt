add_llvm_library(LLVMMinSFITransforms
  ExpandAllocas.cpp
  SandboxMemoryAccesses.cpp
  AllocateDataSegment.cpp
  Utils.cpp
  )

add_dependencies(LLVMMinSFITransforms LLVMNaClTransforms)
