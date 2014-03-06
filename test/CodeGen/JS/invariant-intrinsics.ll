; RUN: llc -march=js < %s | not grep invariant

; llc currently emits no code or bookkeeping for invariant intrinsic calls
; or declarations.

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
