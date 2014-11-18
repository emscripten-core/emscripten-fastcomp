set(LLVM_LINK_COMPONENTS ${LLVM_TARGETS_TO_BUILD} bitreader naclbitreader
    irreader asmparser naclanalysis nacltransforms)

add_llvm_tool(pnacl-llc
  srpc_main.cpp
  SRPCStreamer.cpp
  pnacl-llc.cpp
  ThreadedStreamingCache.cpp
  )
