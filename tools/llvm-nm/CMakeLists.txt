set(LLVM_LINK_COMPONENTS bitreader naclbitreader object)

add_llvm_tool(llvm-nm
  llvm-nm.cpp
  )
