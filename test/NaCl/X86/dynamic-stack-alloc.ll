; RUN: llc -mtriple=x86_64-nacl %s -o - | FileCheck %s
; RUN: llc -mtriple=x86_64-nacl -fast-isel %s -o - | FileCheck %s

; Test calling with many parameters (such that some need to go on the stack),
; along with dynamic stack allocation and x86-64 NaCl stack/frame
; register sandboxing.

declare i32 @callee(i32 %a1, i32 %a2, i32 %a3, i32 %a4, i32 %a5, i32 %a6, i32 %a7, i32 %a8, i8* %ptr)

define i32 @dyn_stack_alloc (i32 %a1, i32 %a2, i32 %a3, i32 %a4, i32 %a5, i32 %a6, i32 %a7, i32 %a8, i32 %amt) {
  %p = alloca i8, i32 %amt, align 1
  %x = call i32 @callee(i32 %a1, i32 %a2, i32 %a3, i32 %a4, i32 %a5, i32 %a6, i32 %a7, i32 %a8, i8* %p)
  ret i32 %x
}

; CHECK-LABEL: dyn_stack_alloc
; Set up frame pointer and save two registers.
; CHECK: movl %ebp, %eax
; CHECK-NEXT: pushq %rax
; CHECK: movq %rsp, %rbp
; CHECK-NEXT: .L
; CHECK-NEXT: .cfi_def_cfa_register %rbp
; CHECK-NEXT: pushq
; CHECK-NEXT: pushq
; CHECK-NOT: pushq

; The dynamic alloc (aligned):
; CHECK: movl 32(%rbp), [[AMT:%.*]]
; CHECK: movl %esp, [[TMP:%.*]]
; CHECK-NEXT: addl $15, [[AMT]]
; CHECK-NEXT: andl $-16, [[AMT]]
; CHECK-NEXT: subl [[AMT]], [[TMP]]
; Set the SP based on the final value of TMP.
; CHECK-NEXT: naclrestsp_noflags [[TMP]], %r15

; Make more room for call arguments:
; CHECK: naclsspq $32, %r15
; CHECK: callq callee

; Restore stack pointer from frame pointer - 16. It is -16 because
; the code still needs to pop the two pushed regs, before restoring
; the frame pointer from the stack.
; CHECK: naclspadj $-16, %r15
; CHECK-NEXT: popq
; CHECK-NEXT: popq
; CHECK-NEXT: naclrestbp (%rsp), %r15
; CHECK-NEXT: naclaspq $8, %r15
