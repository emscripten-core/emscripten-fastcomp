; RUN: not pnacl-llc -mtriple=i386-unknown-nacl -filetype=asm %s -o - 2>&1 | FileCheck %s

; Test that the Verifier pass is running in pnacl-llc.

define i32 @f1(i32 %x) {
  %y = add i32 %z, 1
  %z = add i32 %x, 1
  ret i32 %y
; CHECK: Instruction does not dominate all uses!
; CHECK-NEXT:  %z = add i32 %x, 1
; CHECK-NEXT:  %y = add i32 %z, 1
}

