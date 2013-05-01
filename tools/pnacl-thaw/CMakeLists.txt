set(LLVM_LINK_COMPONENTS bitwriter naclbitreader)

add_llvm_tool(pnacl-thaw
  pnacl-thaw.cpp
  )
