; RUN: pnacl-abicheck < %s | FileCheck %s

; Make sure that external symbols are properly rejected or accepted

define void @foo() {
  ret void
}

; CHECK: foo is not a valid external symbol (disallowed)

define external void @main() {
  ret void
}
; CHECK: main is not a valid external symbol (disallowed)

define external void @_start() {
  ret void
}
; _start is whitelisted
; CHECK-NOT: _start is not a valid external symbol (disallowed)

; Intrinsics can be external too
declare void @llvm.trap()

