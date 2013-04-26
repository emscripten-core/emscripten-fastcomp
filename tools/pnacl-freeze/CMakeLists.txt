set(LLVM_LINK_COMPONENTS bitreader naclbitwriter)

add_llvm_tool(pnacl-freeze
  pnacl-freeze.cpp
  )
