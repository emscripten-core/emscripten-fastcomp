set(LLVM_LINK_COMPONENTS naclbitreader naclbitanalysis)

add_llvm_tool(pnacl-bcanalyzer
  pnacl-bcanalyzer.cpp
  )
