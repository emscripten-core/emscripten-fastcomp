set(LLVM_LINK_COMPONENTS
  BitWriter
  Core
  NaClBitReader
  NaClBitTestUtils
  Support)

add_llvm_tool(pnacl-thaw
  pnacl-thaw.cpp
  )
