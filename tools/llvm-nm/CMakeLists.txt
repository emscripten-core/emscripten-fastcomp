set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  Core
  NaClBitReader # @LOCALMOD
  NaClTransforms # XXX EMSCRIPTEN
  Object
  Support
  )

add_llvm_tool(llvm-nm
  llvm-nm.cpp
  )
