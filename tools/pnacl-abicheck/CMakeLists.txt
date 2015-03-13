set(LLVM_LINK_COMPONENTS
  BitReader
  Core
  IRReader
  NaClAnalysis
  NaClBitReader
  Support)

add_llvm_tool(pnacl-abicheck
  pnacl-abicheck.cpp
  )
