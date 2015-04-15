; RUN: llc -mtriple=i386-unknown-nacl -filetype=asm %s -o - \
; RUN:  | FileCheck %s

declare i32 @bar(i32)

; CHECK-LABEL: @foo
; Check that the jump table for the switch goes in .rodata
; CHECK: .section .rodata
; CHECK: .long .LBB0
define void @foo(i32 %a) {
entry:
  switch i32 %a, label %sw.epilog [
    i32 3, label %sw.bb
    i32 2, label %sw.bb1
    i32 1, label %sw.bb3
    i32 0, label %sw.bb5
  ]

sw.bb:                                            ; preds = %entry
  %call = call i32 @bar(i32 1)
  br label %sw.bb1

sw.bb1:                                           ; preds = %entry, %sw.bb
  %call2 = call i32 @bar(i32 2)
  br label %sw.bb3

sw.bb3:                                           ; preds = %entry, %sw.bb1
  %call4 = call i32 @bar(i32 3)
  br label %sw.bb5

sw.bb5:                                           ; preds = %entry, %sw.bb3
  %call6 = call i32 @bar(i32 4)
  br label %sw.epilog

sw.epilog:                                        ; preds = %sw.bb5, %entry
  ret void
}


; CHECK: .section .text.foo_linkonce,"axG",@progbits,foo_linkonce,comdat
; CHECK: @foo_linkonce
; Check that the jump table for the linkonce_odr function goes into a comdat
; group uniqued like the function
; CHECK: .section .rodata.foo_linkonce,"aG",@progbits,foo_linkonce,comdat
; CHECK: .long .LBB1
define linkonce_odr void @foo_linkonce(i32 %a) {
entry:
  switch i32 %a, label %sw.epilog [
    i32 3, label %sw.bb
    i32 2, label %sw.bb1
    i32 1, label %sw.bb3
    i32 0, label %sw.bb5
  ]

sw.bb:                                            ; preds = %entry
  %call = call i32 @bar(i32 1)
  br label %sw.bb1

sw.bb1:                                           ; preds = %entry, %sw.bb
  %call2 = call i32 @bar(i32 2)
  br label %sw.bb3

sw.bb3:                                           ; preds = %entry, %sw.bb1
  %call4 = call i32 @bar(i32 3)
  br label %sw.bb5

sw.bb5:                                           ; preds = %entry, %sw.bb3
  %call6 = call i32 @bar(i32 4)
  br label %sw.epilog

sw.epilog:                                        ; preds = %sw.bb5, %entry
  ret void
}

