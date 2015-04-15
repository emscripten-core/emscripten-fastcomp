; RUN: opt < %s -rewrite-pnacl-library-calls -S | FileCheck %s
; RUN: opt < %s -rewrite-pnacl-library-calls -S | FileCheck %s -check-prefix=CLEANED
; Test that when there are no uses other than calls to longjmp,
; no function body is generated.

declare void @longjmp(i64*, i32)

; No declaration or definition of longjmp() should remain.
; CLEANED-NOT: @longjmp

define void @call_longjmp(i64* %arg, i32 %num) {
  call void @longjmp(i64* %arg, i32 %num)
; CHECK: call void @llvm.nacl.longjmp(i8* %jmp_buf_i8, i32 %num)
  ret void
}

