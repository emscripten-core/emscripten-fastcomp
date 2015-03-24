set(LLVM_LINK_COMPONENTS
  BitWriter
  Core
  IRReader
  Linker
  NaClBitWriter # @LOCALMOD
  Support
  )

add_llvm_tool(llvm-link
  llvm-link.cpp
  )
