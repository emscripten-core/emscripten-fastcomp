add_llvm_library(LLVMNaClBitTestUtils
  NaClBitcodeMunge.cpp
  NaClBitcodeMungeReader.cpp
  NaClBitcodeMungeUtils.cpp
  NaClBitcodeMungeWriter.cpp
  )
add_dependencies(LLVMNaClBitTestUtils intrinsics_gen)
