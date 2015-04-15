add_llvm_library(LLVMNaClBitWriter
  NaClBitcodeWriter.cpp
  NaClBitcodeWriterPass.cpp
  NaClValueEnumerator.cpp
  )
add_dependencies(LLVMNaClBitWriter intrinsics_gen)
