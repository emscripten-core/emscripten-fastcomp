set(LLVM_LINK_COMPONENTS
  BitReader
  BitWriter
  NaClBitAnalysis
  NaClBitReader
  NaClBitWriter
  NaClBitAnalysis
  NaClBitTestUtils
  )

add_llvm_unittest(BitcodeTests
  BitReaderTest.cpp
  NaClAbbrevTrieTest.cpp
  NaClObjDumpTest.cpp
  NaClObjDumpTypesTest.cpp
  NaClTextFormatterTest.cpp
  )
