set(LLVM_LINK_COMPONENTS ${LLVM_TARGETS_TO_BUILD} bitreader asmparser naclanalysis)

add_llvm_tool(llc
# This file provides wrappers to lseek(2), read(2), etc. 
  nacl_file.cpp
  SRPCStreamer.cpp
  StubMaker.cpp
  TextStubWriter.cpp
  llc.cpp
  )
