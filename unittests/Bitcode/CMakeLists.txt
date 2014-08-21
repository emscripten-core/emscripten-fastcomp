set(LLVM_LINK_COMPONENTS
  BitReader
  BitWriter
  NaClBitAnalysis
  NaClBitReader
  Core
  Support
  )

add_llvm_unittest(BitcodeTests
  BitReaderTest.cpp
  NaClAbbrevTrieTest.cpp
  NaClObjDumpTest.cpp
  NaClTextFormatterTest.cpp
  )
