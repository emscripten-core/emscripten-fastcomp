set(LLVM_LINK_COMPONENTS bitreader bitwriter object support analysis)

add_llvm_tool(pso-stub
  pso-stub.cpp
  )
