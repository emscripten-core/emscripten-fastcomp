set(LLVM_LINK_COMPONENTS bitreader asmparser)

add_llvm_tool(pnacl-abicheck
  pnacl-abicheck.cc
  )
