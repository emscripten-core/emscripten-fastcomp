set(LLVM_LINK_COMPONENTS
  NaClBitAnalysis
  NaClBitReader
  Support)

add_llvm_tool(pnacl-bcanalyzer
  pnacl-bcanalyzer.cpp
  )
