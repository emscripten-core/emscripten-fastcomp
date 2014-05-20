add_llvm_library(LLVMNaClBitAnalysis
  AbbrevTrieNode.cpp
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
  NaClObjDumpStream.cpp
  NaClObjDump.cpp
  )

add_dependencies(LLVMNaClBitAnalysis intrinsics_gen)
