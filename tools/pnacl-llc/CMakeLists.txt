set(LLVM_LINK_COMPONENTS ${LLVM_TARGETS_TO_BUILD} bitreader naclbitreader
    irreader asmparser naclanalysis)

add_llvm_tool(pnacl-llc
# This file provides wrappers to lseek(2), read(2), etc. 
  nacl_file.cpp
  SRPCStreamer.cpp
  pnacl-llc.cpp
  )
