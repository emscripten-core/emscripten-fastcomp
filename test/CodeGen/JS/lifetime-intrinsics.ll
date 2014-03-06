; RUN: llc -march=js < %s | not grep lifetime

; llc currently emits no code or bookkeeping for lifetime intrinsic calls
; or declarations.

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
