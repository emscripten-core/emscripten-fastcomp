set(LLVM_LINK_COMPONENTS
  NaClBitAnalysis
  NaClBitReader
  Support)

add_llvm_tool(pnacl-bcdis
  pnacl-bcdis.cpp
  )
