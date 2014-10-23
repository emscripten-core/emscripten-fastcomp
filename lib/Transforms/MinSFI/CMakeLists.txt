add_llvm_library(LLVMMinSFITransforms
  AllocateDataSegment.cpp
  ExpandAllocas.cpp
  MinSFI.cpp
  RenameEntryPoint.cpp
  SandboxIndirectCalls.cpp
  SandboxMemoryAccesses.cpp
  StripTls.cpp
  SubstituteUndefs.cpp
  Utils.cpp
  )

add_dependencies(LLVMMinSFITransforms LLVMNaClTransforms)
