; RUN: opt < %s -rewrite-llvm-intrinsic-calls -S | FileCheck %s
; RUN: opt < %s -rewrite-llvm-intrinsic-calls -S | FileCheck %s -check-prefix=CLEANED
; Test the @llvm.assume part of the RewriteLLVMIntrinsics pass

declare void @llvm.assume(i1)

; No declaration or definition of llvm.assume() should remain.
; CLEANED-NOT: @llvm.assume

define void @call_assume(i1 %val) {
; CHECK: call_assume
; CHECK-NEXT: ret void
  call void @llvm.assume(i1 %val)
  ret void
}

; A more complex example with a number of calls in several BBs.
define void @multiple_calls(i1 %val) {
; CHECK: multiple_calls
entryblock:
; CHECK: entryblock
; CHECK-NEXT: br
  call void @llvm.assume(i1 %val)
  br i1 %val, label %exitblock, label %never
never:
; CHECK: never:
; CHECK-NEXT: br
  call void @llvm.assume(i1 %val)
  br label %exitblock
exitblock:
; CHECK: exitblock:
; CHECK-NEXT: ret void
  call void @llvm.assume(i1 %val)
  ret void
}
