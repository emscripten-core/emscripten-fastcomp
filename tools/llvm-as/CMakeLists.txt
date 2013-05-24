set(LLVM_LINK_COMPONENTS asmparser bitwriter naclbitwriter)

add_llvm_tool(llvm-as
  llvm-as.cpp
  )
