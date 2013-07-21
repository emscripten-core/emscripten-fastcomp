; RUN: opt < %s -rewrite-llvm-intrinsic-calls -S | FileCheck %s
; RUN: opt < %s -rewrite-llvm-intrinsic-calls -S | FileCheck %s -check-prefix=CLEANED
; Test the @llvm.flt.rounds part of the RewriteLLVMIntrinsics pass

declare i32 @llvm.flt.rounds()

; No declaration or definition of llvm.flt.rounds() should remain.
; CLEANED-NOT: @llvm.flt.rounds

define i32 @call_flt_rounds() {
; CHECK: call_flt_rounds
; CHECK-NEXT: ret i32 1
  %val = call i32 @llvm.flt.rounds()
  ret i32 %val
}

; A more complex example with a number of calls in several BBs.
define i32 @multiple_calls(i64* %arg, i32 %num) {
; CHECK: multiple_calls
entryblock:
; CHECK: entryblock
  %v1 = call i32 @llvm.flt.rounds()
  br label %block1
block1:
; CHECK: block1:
; CHECK-NEXT: %v3 = add i32 1, 1
  %v2 = call i32 @llvm.flt.rounds()
  %v3 = add i32 %v2, %v1
  br label %exitblock
exitblock:
; CHECK: exitblock:
; CHECK-NEXT: %v4 = add i32 1, %v3
; CHECK-NEXT: %v6 = add i32 1, %v4
  %v4 = add i32 %v2, %v3
  %v5 = call i32 @llvm.flt.rounds()
  %v6 = add i32 %v5, %v4
  ret i32 %v6
}
