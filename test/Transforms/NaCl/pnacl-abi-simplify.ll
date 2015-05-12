; RUN: opt %s -pnacl-abi-simplify-preopt -pnacl-abi-simplify-postopt -S \
; RUN:     | FileCheck %s
; RUN: opt %s -enable-pnacl-sjlj-eh -pnacl-abi-simplify-preopt \
; RUN:     -pnacl-abi-simplify-postopt -S | FileCheck %s

target datalayout = "p:32:32:32"

; Check that the "tail" attribute is preserved on calls.
define void @tail_call() {
  tail call void @tail_call()
  ret void
}
; CHECK: tail call void @tail_call()

; Check that unreachable blocks are pruned out, whether or not SJLJ-EH is used.
; Unreachable blocks can have instructions with strange properties like
; self references. Normally, self-references are disallowed.
define i32 @unreachable_block_self_ref() {
entry:
  br label %bb1

bb0:                                              ; preds = %bb0
  %x = add i32 %x, 0
  br i1 undef, label %bb1, label %bb0

bb1:                                              ; preds = %bb0, %entry
  %phi = phi i32 [ 321, %entry ], [ %x, %bb0 ]
  ret i32 %phi
}
; CHECK-LABEL: unreachable_block_self_ref() {
; CHECK-NEXT: entry:
; CHECK-NEXT: ret i32 321
; CHECK-NEXT: }

declare void @my_exit(i32)

; Another check for unreachable block pruning: in this case, the unreachable
; block can have instructions that confuse liveness analysis.
define i32 @unreachable_block_bad_liveness() {
entry:
  %ret_val = add i32 undef, undef
  call void @my_exit(i32 %ret_val)
  unreachable
label:
  ; ret_val has no reaching definitions, causing an inconsistency in
  ; liveness analysis.
  ret i32 %ret_val
}
; CHECK-LABEL: unreachable_block_bad_liveness() {
; CHECK-NEXT: entry:
; CHECK-NEXT: %ret_val = add i32 undef, undef
; CHECK-NEXT: call void @my_exit
; CHECK-NEXT: unreachable
; CHECK-NEXT: }
