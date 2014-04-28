set(LLVM_LINK_COMPONENTS
  BitReader
  BitWriter
  NaClBitAnalysis
  NaClBitReader
  )

add_llvm_unittest(BitcodeTests
  BitReaderTest.cpp
  NaClAbbrevTrieTest.cpp
  NaClObjDumpTest.cpp
  NaClTextFormatterTest.cpp
  )
