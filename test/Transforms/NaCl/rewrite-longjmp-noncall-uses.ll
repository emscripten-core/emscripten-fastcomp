; RUN: opt < %s -rewrite-pnacl-library-calls -S | FileCheck %s
; Check that the rewrite pass behaves correctly in the presence 
; of various uses of longjmp that are not calls.

@fp = global void (i64*, i32)* @longjmp, align 8
; CHECK: @fp = global void (i64*, i32)* @longjmp, align 8
@arrfp = global [3 x void (i64*, i32)*] [void (i64*, i32)* null, void (i64*, i32)* @longjmp, void (i64*, i32)* null], align 16
; CHECK: @arrfp = global [3 x void (i64*, i32)*] [void (i64*, i32)* null, void (i64*, i32)* @longjmp, void (i64*, i32)* null], align 16

; CHECK: define internal void @longjmp(i64* %env, i32 %val) {

declare void @longjmp(i64*, i32)

declare void @somefunc(i8*)

define void @foo() {
entry:
  call void @somefunc(i8* bitcast (void (i64*, i32)* @longjmp to i8*))
; CHECK: call void @somefunc(i8* bitcast (void (i64*, i32)* @longjmp to i8*))
  ret void
}
