; RUN: llc < %s | not grep lifetime

; llc currently emits no code or bookkeeping for lifetime intrinsic calls
; or declarations.

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

declare void @bar(i8*)

define void @foo() {
  %p = alloca i8
  call void @llvm.lifetime.start(i64 1, i8* %p)
  call void @bar(i8* %p)
  call void @llvm.lifetime.end(i64 1, i8* %p)
  ret void
}

declare void @llvm.lifetime.start(i64, i8* nocapture)
declare void @llvm.lifetime.end(i64, i8* nocapture)
