; RUN: opt %s -internalize-used-globals  -S | FileCheck %s

target datalayout = "e-p:32:32-i64:64"
target triple = "le32-unknown-nacl"

@llvm.used = appending global [1 x i8*] [i8* bitcast (void ()* @foo to i8*)], section "llvm.metadata"
; The used list remains unchanged.
; CHECK: @llvm.used = appending global [1 x i8*] [i8* bitcast (void ()* @foo to i8*)], section "llvm.metadata"


define hidden void @foo() #0 {
  ret void
}
; Although in the used list, foo becomes internal.
; CHECK-LABEL: define internal void @foo


define i32 @_start() {
entry:
  ret i32 0
}
; @_start is left non-internal.
; CHECK-LABEL: define i32 @_start

define internal void @my_internal() {
  ret void
}

; Internals are left as-is.
; CHECK-LABEL: define internal void @my_internal()

!llvm.ident = !{!0}
!0 = !{!"clang version 3.5.0 "}

