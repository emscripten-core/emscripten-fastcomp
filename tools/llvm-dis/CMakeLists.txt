set(LLVM_LINK_COMPONENTS
  BitReader
  naclbitreader
  Core
  Support
  )

add_llvm_tool(llvm-dis
  llvm-dis.cpp
  )
