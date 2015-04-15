; RUN: opt < %s -rewrite-llvm-intrinsic-calls -S | FileCheck %s
; RUN: opt < %s -rewrite-llvm-intrinsic-calls -S | FileCheck %s -check-prefix=CLEANED
; Test the @llvm.prefetch part of the RewriteLLVMIntrinsics pass

declare void @llvm.prefetch(i8 *%ptr, i32 %rw, i32 %locality, i32 %cache_type)

; No declaration or definition of llvm.prefetch() should remain.
; CLEANED-NOT: @llvm.prefetch

define void @call_prefetch(i8 *%ptr, i32 %rw, i32 %locality, i32 %cache_type) {
; CHECK: call_prefetch
; CHECK-NEXT: ret void
  call void @llvm.prefetch(i8 *%ptr, i32 %rw, i32 %locality, i32 %cache_type)
  ret void
}

; A more complex example with a number of calls in several BBs.
define void @multiple_calls(i8 *%ptr, i32 %rw, i32 %locality, i32 %cache_type) {
; CHECK: multiple_calls
entryblock:
; CHECK: entryblock
; CHECK-NEXT: br
  call void @llvm.prefetch(i8 *%ptr, i32 %rw, i32 %locality, i32 %cache_type)
  br label %block1
block1:
; CHECK: block1:
; CHECK-NEXT: br
  call void @llvm.prefetch(i8 *%ptr, i32 %rw, i32 %locality, i32 %cache_type)
  br label %exitblock
exitblock:
; CHECK: exitblock:
; CHECK-NEXT: ret void
  call void @llvm.prefetch(i8 *%ptr, i32 %rw, i32 %locality, i32 %cache_type)
  ret void
}
