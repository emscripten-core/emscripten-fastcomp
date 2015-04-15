; RUN: opt < %s -rewrite-pnacl-library-calls -S | FileCheck %s

; See https://code.google.com/p/nativeclient/issues/detail?id=3706
; Make sure that when @longjmp is used as an argument in a call instruction,
; the rewrite pass does the right thing and doesn't get confused.

; CHECK: define internal void @longjmp(i64* %env, i32 %val) {

declare void @longjmp(i64*, i32)

declare void @somefunc(i32, void (i64*, i32)*, i32)

define void @foo() {
entry:
  call void @somefunc(i32 1, void (i64*, i32)* @longjmp, i32 2)
; CHECK: call void @somefunc(i32 1, void (i64*, i32)* @longjmp, i32 2)
  ret void
}
