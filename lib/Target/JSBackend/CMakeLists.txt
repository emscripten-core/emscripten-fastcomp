add_llvm_target(JSBackendCodeGen
  JSBackend.cpp
  Relooper.cpp
  SimplifyAllocas.cpp
  )

add_subdirectory(TargetInfo)
