set(LLVM_LINK_COMPONENTS
  BitWriter
  naclbitwriter
  Core
  IRReader
  Linker
  Support
  )

add_llvm_tool(llvm-link
  llvm-link.cpp
  )
