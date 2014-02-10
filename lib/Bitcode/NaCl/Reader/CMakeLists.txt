add_llvm_library(LLVMNaClBitReader
  NaClBitCodes.cpp
  NaClBitcodeHeader.cpp
  NaClBitcodeReader.cpp
  NaClBitstreamReader.cpp
  NaClBitcodeParser.cpp
  NaClBitcodeAnalyzer.cpp
  NaClBitcodeDist.cpp
  NaClBitcodeAbbrevDist.cpp
  NaClBitcodeBitsDist.cpp
  NaClBitcodeBitsAndAbbrevsDist.cpp
  NaClBitcodeCodeDist.cpp
  NaClBitcodeBlockDist.cpp  
  NaClBitcodeSizeDist.cpp  
  NaClBitcodeSubblockDist.cpp
  NaClBitcodeValueDist.cpp
  NaClAnalyzerBlockDist.cpp
  NaClCompressBlockDist.cpp
  NaClCompressCodeDist.cpp
  )

add_dependencies(LLVMNaClBitReader intrinsics_gen)
