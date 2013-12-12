set(LLVM_LINK_COMPONENTS naclbitreader naclbitwriter)

add_llvm_tool(pnacl-bccompress
  pnacl-bccompress.cpp
  )
