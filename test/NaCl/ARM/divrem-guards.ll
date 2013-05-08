; RUN: opt < %s -insert-divide-check -S | FileCheck -check-prefix=OPT %s
; RUN: llc -mtriple=armv7-unknown-nacl -sfi-branch -filetype=obj %s -o - \
; RUN:  | llvm-objdump -disassemble -triple armv7 -mattr=+nacl-trap - \
; RUN:  | FileCheck -check-prefix=ARM %s


; Check for all four operators that need guards.
define i32 @mysdiv(i32 %x, i32 %y) #0 {
entry:
  %div1 = sdiv i32 %x, %y
; OPT: %0 = icmp eq i32 %y, 0
; OPT-NEXT: br i1 %0, label %divrem.by.zero, label %guarded.divrem
; OPT: guarded.divrem:
; OPT-NEXT: sdiv
; OPT-NEXT: ret
; OPT: divrem.by.zero:
; OPT-NEXT: call void @llvm.trap()
; OPT-NEXT: unreachable
; ARM: cmp r1, #0
; ARM-NEXT: beq
  ret i32 %div1
; ARM: f0 de fe e7
}

define i32 @myudiv(i32 %x, i32 %y) #0 {
entry:
  %div1 = udiv i32 %x, %y
; OPT: %0 = icmp eq i32 %y, 0
; OPT-NEXT: br i1 %0, label %divrem.by.zero, label %guarded.divrem
; OPT: guarded.divrem:
; OPT-NEXT: udiv
; OPT-NEXT: ret
; OPT: divrem.by.zero:
; OPT-NEXT: call void @llvm.trap()
; OPT-NEXT: unreachable
; ARM: cmp r1, #0
; ARM-NEXT: beq
  ret i32 %div1
; ARM: f0 de fe e7
}

define i32 @mysrem(i32 %x, i32 %y) #0 {
entry:
  %rem1 = srem i32 %x, %y
; OPT: %0 = icmp eq i32 %y, 0
; OPT-NEXT: br i1 %0, label %divrem.by.zero, label %guarded.divrem
; OPT: guarded.divrem:
; OPT-NEXT: srem
; OPT-NEXT: ret
; OPT: divrem.by.zero:
; OPT-NEXT: call void @llvm.trap()
; OPT-NEXT: unreachable
; ARM: cmp r1, #0
; ARM-NEXT: beq
  ret i32 %rem1
; ARM: f0 de fe e7
}

define i32 @myurem(i32 %x, i32 %y) #0 {
entry:
  %rem1 = urem i32 %x, %y
; OPT: %0 = icmp eq i32 %y, 0
; OPT-NEXT: br i1 %0, label %divrem.by.zero, label %guarded.divrem
; OPT: guarded.divrem:
; OPT-NEXT: urem
; OPT-NEXT: ret
; OPT: divrem.by.zero:
; OPT-NEXT: call void @llvm.trap()
; OPT-NEXT: unreachable
; ARM: cmp r1, #0
; ARM-NEXT: beq
  ret i32 %rem1
; ARM: f0 de fe e7
}

; Divides by non-zero constants should not be guarded.
define i32 @mysdiv_const(i32 %x) #0 {
entry:
  %div1 = sdiv i32 %x, 10
; OPT-NOT: icmp
; OPT-NOT: br
; OPT-NOT: guarded.divrem:
; OPT-NOT: divrem.by.zero:
; OPT-NOT: call void @llvm.trap()
; OPT-NOT: unreachable
; ARM-NOT: cmp r1, #0
; ARM-NOT: f0 de fe e7
  ret i32 %div1
}

; Divides by explicit zero should prefixed by a trap.
define i32 @mysdiv_zero(i32 %x) #0 {
entry:
  %div1 = sdiv i32 %x, 0
; OPT-NOT: guarded.divrem:
; OPT-NOT: divrem.by.zero:
; OPT: call void @llvm.trap()
; OPT-NEXT: sdiv
; ARM-NOT: cmp r1, #0
; ARM: f0 de fe e7
  ret i32 %div1
}

attributes #0 = { nounwind }
