set(LLVM_LINK_COMPONENTS linker bitreader bitwriter naclbitwriter asmparser)

add_llvm_tool(llvm-link
  llvm-link.cpp
  )
