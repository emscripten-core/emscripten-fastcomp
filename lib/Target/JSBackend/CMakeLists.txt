add_llvm_target(JSBackendCodeGen
  AllocaManager.cpp
  ExpandI64.cpp
  JSBackend.cpp
  Relooper.cpp
  SimplifyAllocas.cpp
  )

add_subdirectory(TargetInfo)
