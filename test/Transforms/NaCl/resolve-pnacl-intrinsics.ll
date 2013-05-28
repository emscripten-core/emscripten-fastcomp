; RUN: opt < %s -resolve-pnacl-intrinsics -S | FileCheck %s

declare i32 @llvm.nacl.setjmp(i8*)
declare void @llvm.nacl.longjmp(i8*, i32)

; These declarations must be here because the function pass expects
; to find them. In real life they're inserted by the translator
; before the function pass runs.
declare i32 @setjmp(i8*)
declare void @longjmp(i8*, i32)

; CHECK-NOT: call i32 @llvm.nacl.setjmp
; CHECK-NOT: call void @llvm.nacl.longjmp

define i32 @call_setjmp(i8* %arg) {
  %val = call i32 @llvm.nacl.setjmp(i8* %arg)
; CHECK: %val = call i32 @setjmp(i8* %arg)
  ret i32 %val
}

define void @call_longjmp(i8* %arg, i32 %num) {
  call void @llvm.nacl.longjmp(i8* %arg, i32 %num)
; CHECK: call void @longjmp(i8* %arg, i32 %num)
  ret void
}
