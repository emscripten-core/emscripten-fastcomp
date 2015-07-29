set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Analysis
  BitReader
  CodeGen
  Core
  IRReader
  MC
  NaClAnalysis
  NaClBitReader
  NaClBitTestUtils
  NaClTransforms
  ScalarOpts
  SelectionDAG
  Support
  Target)

add_llvm_tool(pnacl-llc
  srpc_main.cpp
  SRPCStreamer.cpp
  pnacl-llc.cpp
  ThreadedStreamingCache.cpp
  )

if(LLVM_ENABLE_THREADS AND HAVE_LIBPTHREAD)
  target_link_libraries(pnacl-llc pthread)
endif()
