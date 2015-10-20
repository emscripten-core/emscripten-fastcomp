set(LLVM_LINK_COMPONENTS
  AsmParser
  BitReader
  BitWriter
  NaClBitAnalysis
  NaClBitTestUtils
  Core
  Support
  )

add_llvm_unittest(BitcodeTests
  BitReaderTest.cpp
  BitstreamReaderTest.cpp
  )
