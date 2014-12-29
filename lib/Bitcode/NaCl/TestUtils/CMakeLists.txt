add_llvm_library(LLVMNaClBitTestUtils
  NaClBitcodeMunge.cpp
  )
add_dependencies(LLVMNaClBitTestUtils intrinsics_gen)
