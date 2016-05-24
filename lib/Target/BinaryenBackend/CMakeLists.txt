add_llvm_target(BinaryenBackendCodeGen
  AllocaManager.cpp
  ExpandBigSwitches.cpp
  BinaryenBackend.cpp
  BinaryenTargetMachine.cpp
  BinaryenTargetTransformInfo.cpp
  RemoveLLVMAssume.cpp
  SimplifyAllocas.cpp
  )

add_dependencies(LLVMBinaryenBackendCodeGen intrinsics_gen)

add_subdirectory(Binaryen)
add_subdirectory(TargetInfo)
add_subdirectory(MCTargetDesc)
