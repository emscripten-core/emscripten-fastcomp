; Tests if we handle pointer argument for switch statement.
; https://code.google.com/p/nativeclient/issues/detail?id=4050

; RUN: llvm-as < %s | pnacl-freeze \
; RUN:              | pnacl-thaw \
; RUN:              | llvm-dis -o - | FileCheck %s

@hi = internal global [1 x i8] zeroinitializer, align 1
define internal void @foo() {
  %e = ptrtoint [1 x i8]* @hi to i32
  switch i32 %e, label %baz3 [
    i32 -4, label %baz1
    i32 -8, label %baz2
  ]
baz1:
  ret void
baz2:
  ret void
baz3:
  ret void
}

; CHECK-LABEL: @foo
; CHECK:       %1 = ptrtoint [1 x i8]* @hi to i32
; CHECK-NEXT:  switch i32 %1, label
; CHECK-NEXT:     i32
; CHECK-NEXT:     i32
