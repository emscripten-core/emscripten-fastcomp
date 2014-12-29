set(LLVM_LINK_COMPONENTS naclbitreader naclbitwriter naclbitanalysis)

add_llvm_tool(pnacl-bccompress
  pnacl-bccompress.cpp
  )
