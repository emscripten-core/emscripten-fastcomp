; RUN: llc < %s | FileCheck %s

; Handle the llvm.expect intrinsic.

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

; CHECK: $expval = $x;
; CHECK: $tobool = ($expval|0)!=(0);

define void @foo(i32 %x) {
entry:
  %expval = call i32 @llvm.expect.i32(i32 %x, i32 0)
  %tobool = icmp ne i32 %expval, 0
  br i1 %tobool, label %if.then, label %if.end

if.then:
  call void @callee()
  br label %if.end

if.end:
  ret void
}

; Function Attrs: nounwind readnone
declare i32 @llvm.expect.i32(i32, i32) #0

declare void @callee()

attributes #0 = { nounwind readnone }
