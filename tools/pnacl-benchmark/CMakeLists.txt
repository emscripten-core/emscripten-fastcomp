set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  BitReader
  Core
  IRReader
  NaClAnalysis
  NaClBitAnalysis
  NaClBitReader
  NaClTransforms
  Support)

add_llvm_tool(pnacl-benchmark
  pnacl-benchmark.cpp
  )
