set(LLVM_LINK_COMPONENTS
  Core
  NaClBitReader
  NaClBitTestUtils
  NaClBitWriter
  Support)

add_llvm_tool(pnacl-bcfuzz
  pnacl-bcfuzz.cpp
  )
