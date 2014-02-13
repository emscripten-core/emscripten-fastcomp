add_llvm_library(LLVMNaClBitWriter
  NaClBitcodeWriter.cpp
  NaClValueEnumerator.cpp
  )
add_dependencies(LLVMNaClBitWriter intrinsics_gen)
