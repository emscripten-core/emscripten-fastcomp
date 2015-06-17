add_llvm_library(LLVMNaClBitTestUtils
  NaClBitcodeMunge.cpp
  NaClBitcodeMungeReader.cpp
  NaClBitcodeMungeUtils.cpp
  NaClBitcodeMungeWriter.cpp
  NaClBitcodeTextWriter.cpp
  NaClFuzz.cpp
  NaClRandNumGen.cpp
  NaClSimpleRecordFuzzer.cpp
  )
add_dependencies(LLVMNaClBitTestUtils intrinsics_gen)
