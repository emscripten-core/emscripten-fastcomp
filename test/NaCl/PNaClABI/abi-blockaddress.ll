; RUN: pnacl-abicheck < %s | FileCheck %s

define void @func_with_block() {
  br label %some_block
some_block:
  ret void
}
; CHECK-NOT: disallowed

@blockaddr = global i8* blockaddress(@func_with_block, %some_block)
; CHECK: Global variable blockaddr has non-flattened initializer (disallowed): i8* blockaddress(@func_with_block, %some_block)
