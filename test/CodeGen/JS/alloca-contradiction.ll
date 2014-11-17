; RUN: llc < %s

; In theory, the @llvm.lifetime intrinsics shouldn't contradict each other, but
; in practice they apparently do sometimes. When they do, we should probably be
; conservative.

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

; Don't merge these two allocas, even though lifetime markers may initially
; appear to indicate that it's safe, because they also indicate that it's
; unsafe.

; CHECK: foo
; CHECK: HEAP8[$p] = 0;
; CHECK: HEAP8[$q] = 1;
define void @foo() nounwind {
entry:
  %p = alloca i8
  %q = alloca i8
  br label %loop

loop:
  call void @llvm.lifetime.end(i64 1, i8* %q)
  store volatile i8 0, i8* %p
  store volatile i8 1, i8* %q
  call void @llvm.lifetime.start(i64 1, i8* %p)
  br i1 undef, label %loop, label %end

end:                                              ; preds = %red
  ret void
}

declare void @llvm.lifetime.start(i64, i8* nocapture) nounwind
declare void @llvm.lifetime.end(i64, i8* nocapture) nounwind
