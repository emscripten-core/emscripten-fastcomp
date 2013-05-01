add_llvm_library(LLVMNaClBitReader
  NaClBitcodeReader.cpp
  NaClBitstreamReader.cpp
  )

add_dependencies(LLVMNaClBitReader intrinsics_gen)
