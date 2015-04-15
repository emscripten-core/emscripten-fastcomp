; RUN: llc -mtriple=x86_64-pc-nacl -force-align-stack %s -o - | FileCheck %s

; Check that stack alignment (ANDri32 with the stack pointer) expands to
; the bundle-locked adjustment for asm output, instead of a naclandsp pseudo
define void @stackalign () {
; CHECK-LABEL: stackalign
  ret void
; CHECK: .bundle_lock
; CHECK-NEXT: andl $-8, %esp
; CHECK-NEXT: leaq (%rsp,%r15), %rsp
; CHECK-NEXT: .bundle_unlock
}