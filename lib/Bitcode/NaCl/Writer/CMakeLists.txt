add_llvm_library(NaClBitWriter
  NaClBitcodeWriter.cpp
  NaClValueEnumerator.cpp
  )
add_dependencies(NaClBitWriter intinsics_gen)
