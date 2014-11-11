; RUN: pnacl-llc -mtriple=x86_64-unknown-nacl -filetype=asm %s -o - \
; RUN:   | FileCheck %s

; CHECK: func:
define void @func(i32* %i) {
entry:
  %0 = load i32* %i, align 4
; Check that the inline asm expression is correctly transformed to NaCl
; pseudo-segment memory operand syntax.
; CHECK: movl %e{{[a-z]+}}, %e[[REG:[a-z]{2}]]
; CHECK: mov %nacl:(%r15,%r[[REG]]), %eax
  call void asm sideeffect "mov $0, %eax", "*m,~{dirflag},~{fpsr},~{flags}"(i32* %i)
  ret void
}

