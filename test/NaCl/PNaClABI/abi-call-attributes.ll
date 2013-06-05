; RUN: pnacl-abicheck < %s | FileCheck %s

define void @func(i32 %arg) {
  ret void
}

define void @calls() {
  call void @func(i32 1) noreturn nounwind
; CHECK: disallowed: bad call attributes: call void @func(i32 1) #
  call void @func(i32 inreg 1)
; CHECK-NEXT: disallowed: bad call attributes: call void @func(i32 inreg 1)
  ret void
}

; CHECK-NOT: disallowed
