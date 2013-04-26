add_llvm_library(LLVMNaClBitWriter
  NaClBitcodeWriter.cpp
  NaClValueEnumerator.cpp
  )
add_dependencies(LLVMNaClBitWriter intinsics_gen)
