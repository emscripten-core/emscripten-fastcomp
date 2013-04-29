set(LLVM_LINK_COMPONENTS wrap support )

add_llvm_tool(bc-wrap
  bc_wrap.cpp
  )