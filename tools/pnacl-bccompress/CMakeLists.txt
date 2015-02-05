set(LLVM_LINK_COMPONENTS
  NaClBitAnalysis
  NaClBitReader
  NaClBitWriter
  Support)

add_llvm_tool(pnacl-bccompress
  pnacl-bccompress.cpp
  )
