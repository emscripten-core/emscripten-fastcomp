; RUN: llc < %s | not grep invariant

; llc currently emits no code or bookkeeping for invariant intrinsic calls
; or declarations.

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

declare void @bar(i8*)

define void @foo() {
  %p = alloca i8
  %i = call {}* @llvm.invariant.start(i64 1, i8* %p)
  call void @bar(i8* %p)
  call void @llvm.invariant.end({}* %i, i64 1, i8* %p)
  ret void
}

declare {}* @llvm.invariant.start(i64, i8* nocapture)
declare void @llvm.invariant.end({}*, i64, i8* nocapture)
