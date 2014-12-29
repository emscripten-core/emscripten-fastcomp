set(LLVM_LINK_COMPONENTS ${LLVM_TARGETS_TO_BUILD} naclbitanalysis
    bitreader naclbitreader irreader asmparser naclanalysis
    nacltransforms)

add_llvm_tool(pnacl-benchmark
  pnacl-benchmark.cpp
  )
