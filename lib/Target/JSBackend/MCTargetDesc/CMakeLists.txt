add_llvm_library(LLVMJSBackendDesc
  JSBackendMCTargetDesc.cpp
  )

add_dependencies(LLVMJSBackendDesc intrinsics_gen)

# Hack: we need to include 'main' target directory to grab private headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/..)
