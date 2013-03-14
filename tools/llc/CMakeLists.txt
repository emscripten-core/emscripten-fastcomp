set(LLVM_LINK_COMPONENTS ${LLVM_TARGETS_TO_BUILD} bitreader asmparser NaClAnalysis)

add_llvm_tool(llc
# LOCALMOD BEGIN
# This file provides wrappers to lseek(2), read(2), etc. 
  nacl_file.cpp
  StubMaker.cpp
  TextStubWriter.cpp
# LOCALMOD END
  llc.cpp
  )
