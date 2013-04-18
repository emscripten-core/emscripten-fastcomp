set(LLVM_LINK_COMPONENTS bitreader asmparser naclanalysis)

add_llvm_tool(pnacl-abicheck
  pnacl-abicheck.cpp
  )
