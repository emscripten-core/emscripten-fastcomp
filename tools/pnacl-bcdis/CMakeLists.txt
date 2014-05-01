set(LLVM_LINK_COMPONENTS naclbitanalysis naclbitreader)

add_llvm_tool(pnacl-bcdis
  pnacl-bcdis.cpp
  )
