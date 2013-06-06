add_llvm_library(LLVMNaClBitReader
  NaClBitcodeHeader.cpp
  NaClBitcodeReader.cpp
  NaClBitstreamReader.cpp
  )

add_dependencies(LLVMNaClBitReader intrinsics_gen)
