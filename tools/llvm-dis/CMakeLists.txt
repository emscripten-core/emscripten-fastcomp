set(LLVM_LINK_COMPONENTS
  BitReader
  Core
  NaClBitReader # @LOCALMOD
  Support
  )

add_llvm_tool(llvm-dis
  llvm-dis.cpp
  )
