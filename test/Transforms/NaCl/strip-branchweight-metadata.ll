; RUN: opt -S -strip-metadata %s | FileCheck %s

; Test that !prof metadata is removed from branches
; CHECK: @foo
; CHECK-NOT: !prof
define i32 @foo(i32 %c) {
  switch i32 %c, label %3 [
    i32 5, label %4
    i32 0, label %1
    i32 4, label %2
  ], !prof !0

; <label>:1                                       ; preds = %0
  br label %4

; <label>:2                                       ; preds = %0
  br label %4

; <label>:3                                       ; preds = %0
  br label %4

; <label>:4                                       ; preds = %0, %3, %2, %1
  %.0 = phi i32 [ -1, %1 ], [ 99, %2 ], [ 1, %3 ], [ 0, %0 ]
  ret i32 %.0
}

; CHECK: ret i32 %.0
; CHECK-NOT: !0 =
!0 = !{!"branch_weights", i32 4, i32 256, i32 8, i32 4}
