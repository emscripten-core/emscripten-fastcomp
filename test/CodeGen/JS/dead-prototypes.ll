; RUN: llc < %s | not grep printf

; llc shouldn't emit any code or bookkeeping for unused declarations.

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

define void @foo() {
  ret void
}

declare i32 @printf(i8* nocapture, ...)
