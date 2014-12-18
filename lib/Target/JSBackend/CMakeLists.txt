add_llvm_target(JSBackendCodeGen
  AllocaManager.cpp
  ExpandI64.cpp
  ExpandInsertExtractElement.cpp
  JSBackend.cpp
  JSTargetMachine.cpp
  JSTargetTransformInfo.cpp
  Relooper.cpp
  SimplifyAllocas.cpp
  )

add_dependencies(LLVMJSBackendCodeGen intrinsics_gen)

add_subdirectory(TargetInfo)
add_subdirectory(MCTargetDesc)
