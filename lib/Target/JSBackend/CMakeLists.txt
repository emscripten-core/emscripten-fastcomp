add_llvm_target(JSBackendCodeGen
  ExpandI64.cpp
  JSBackend.cpp
  Relooper.cpp
  SimplifyAllocas.cpp
  )

add_subdirectory(TargetInfo)
