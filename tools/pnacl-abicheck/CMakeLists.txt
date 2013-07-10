set(LLVM_LINK_COMPONENTS bitreader naclbitreader irreader asmparser naclanalysis)

add_llvm_tool(pnacl-abicheck
  pnacl-abicheck.cpp
  )
