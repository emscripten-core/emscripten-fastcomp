set(LLVM_LINK_COMPONENTS bitreader naclbitwriter naclbitreader)

add_llvm_tool(pnacl-freeze
  pnacl-freeze.cpp
  )
