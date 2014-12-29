set(LLVM_LINK_COMPONENTS naclbitanalysis naclbitreader)

add_llvm_tool(pnacl-bcanalyzer
  pnacl-bcanalyzer.cpp
  )
