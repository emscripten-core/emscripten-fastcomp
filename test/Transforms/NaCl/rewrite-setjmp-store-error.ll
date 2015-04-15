; RUN: not opt < %s -rewrite-pnacl-library-calls -S 2>&1 | FileCheck %s
; Test that the pass enforces not being able to store the address
; of setjmp.

declare i32 @setjmp(i64*)

define i32 @takeaddr_setjmp(i64* %arg) {
  %fp = alloca i32 (i64*)*, align 8
; CHECK: Taking the address of setjmp is invalid
  store i32 (i64*)* @setjmp, i32 (i64*)** %fp, align 8
  ret i32 7
}

