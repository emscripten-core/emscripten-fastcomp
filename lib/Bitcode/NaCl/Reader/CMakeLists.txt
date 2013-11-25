add_llvm_library(LLVMNaClBitReader
  NaClBitcodeHeader.cpp
  NaClBitcodeReader.cpp
  NaClBitstreamReader.cpp
  NaClBitcodeParser.cpp
  NaClBitcodeAnalyzer.cpp
  )

add_dependencies(LLVMNaClBitReader intrinsics_gen)
