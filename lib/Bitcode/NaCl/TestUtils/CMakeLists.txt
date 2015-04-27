add_llvm_library(LLVMNaClBitTestUtils
  NaClBitcodeMunge.cpp
  NaClBitcodeMungeUtils.cpp
  )
add_dependencies(LLVMNaClBitTestUtils intrinsics_gen)
