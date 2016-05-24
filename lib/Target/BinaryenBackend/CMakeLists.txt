add_llvm_target(BinaryenBackendCodeGen
  AllocaManager.cpp
  ExpandBigSwitches.cpp
  BinaryenBackend.cpp
  JSTargetMachine.cpp
  JSTargetTransformInfo.cpp
  Relooper.cpp
  RemoveLLVMAssume.cpp
  SimplifyAllocas.cpp
  )

add_dependencies(LLVMBinaryenBackendCodeGen intrinsics_gen)

add_subdirectory(Binaryen)
add_subdirectory(TargetInfo)
add_subdirectory(MCTargetDesc)
