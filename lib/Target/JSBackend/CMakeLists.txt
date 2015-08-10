add_llvm_target(JSBackendCodeGen
  AllocaManager.cpp
  ExpandBigSwitches.cpp
  JSBackend.cpp
  JSTargetMachine.cpp
  JSTargetTransformInfo.cpp
  Relooper.cpp
  RemoveLLVMAssume.cpp
  SimplifyAllocas.cpp
  optimizer.cpp
  optimizer-main.cpp
  parser.cpp
  simple_ast.cpp
  )

add_dependencies(LLVMJSBackendCodeGen intrinsics_gen)

add_subdirectory(TargetInfo)
add_subdirectory(MCTargetDesc)
