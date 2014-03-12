add_llvm_library(LLVMNaClBitAnalysis
  AbbrevTrieNode.cpp
  )

add_dependencies(LLVMNaClBitAnalysis intrinsics_gen)
