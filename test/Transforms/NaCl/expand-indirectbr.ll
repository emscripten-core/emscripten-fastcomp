; RUN: opt %s -expand-indirectbr -S | FileCheck %s


@addresses = global [2 x i8*]
    [i8* blockaddress(@indirectbr_example, %label1),
     i8* blockaddress(@indirectbr_example, %label2)]
; CHECK: @addresses = global [2 x i8*] [i8* inttoptr (i32 1 to i8*), i8* inttoptr (i32 2 to i8*)]


define i32 @indirectbr_example(i8* %addr) {
  indirectbr i8* %addr, [label %label1, label %label2]
label1:
  ret i32 100
label2:
  ret i32 200
}
; CHECK: define i32 @indirectbr_example
; CHECK-NEXT: %indirectbr_cast = ptrtoint i8* %addr to i32
; CHECK-NEXT: switch i32 %indirectbr_cast, label %indirectbr_default [
; CHECK-NEXT:   i32 1, label %label1
; CHECK-NEXT:   i32 2, label %label2
; CHECK-NEXT: ]
; CHECK: indirectbr_default:
; CHECK-NEXT: unreachable


define i32 @label_appears_twice(i8* %addr) {
entry:
  indirectbr i8* %addr, [label %label, label %label]
label:
  %val = phi i32 [ 123, %entry ], [ 123, %entry ]
  ret i32 %val
}
; CHECK: define i32 @label_appears_twice
; CHECK: switch i32 %indirectbr_cast, label %indirectbr_default [
; CHECK-NEXT:   i32 1, label %label
; CHECK-NEXT: ]
; CHECK: %val = phi i32 [ 123, %entry ]


define i8* @unused_blockaddress() {
  ret i8* blockaddress (@unused_blockaddress, %dead_label)
dead_label:
  ret i8* null
}
; CHECK: define i8* @unused_blockaddress
; CHECK-NEXT: ret i8* inttoptr (i32 -1 to i8*)


; Check that the label is given a consistent switch value across all
; indirectbr expansions.
define i32 @multiple_indirectbr(i8* %addr) {
  indirectbr i8* %addr, [label %label]
  indirectbr i8* %addr, [label %label]
label:
  ret i32 100
}
; CHECK: define i32 @multiple_indirectbr
; CHECK: switch i32 %indirectbr_cast{{[0-9]*}}, label %indirectbr_default [
; CHECK-NEXT: i32 1, label %label
; CHECK: switch i32 %indirectbr_cast{{[0-9]*}}, label %indirectbr_default [
; CHECK-NEXT: i32 1, label %label
