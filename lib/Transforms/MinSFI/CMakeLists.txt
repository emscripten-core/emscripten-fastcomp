add_llvm_library(LLVMMinSFITransforms
  AllocateDataSegment.cpp
  ExpandAllocas.cpp
  RenameEntryPoint.cpp
  SandboxIndirectCalls.cpp
  SandboxMemoryAccesses.cpp
  Utils.cpp
  )

add_dependencies(LLVMMinSFITransforms LLVMNaClTransforms)
