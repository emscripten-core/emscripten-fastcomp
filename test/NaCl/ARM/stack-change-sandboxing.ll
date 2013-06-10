; RUN: pnacl-llc -mtriple=armv7-unknown-nacl -sfi-stack -filetype=obj %s -o - \
; RUN:  | llvm-objdump -disassemble -triple armv7 - | FileCheck %s

define i32 @foo(i32 %aa, i32 %bb) nounwind {
entry:

; CHECK:      sub sp, sp, #16
; CHECK-NEXT: bic	sp, sp, #3221225472

  %aa.addr = alloca i32, align 4
  %bb.addr = alloca i32, align 4
  %cc = alloca i32, align 4
  %dd = alloca i32, align 4
  store i32 %aa, i32* %aa.addr, align 4
  store i32 %bb, i32* %bb.addr, align 4
  %0 = load i32* %aa.addr, align 4
  %1 = load i32* %bb.addr, align 4
  %mul = mul nsw i32 %0, %1
  store i32 %mul, i32* %cc, align 4
  %2 = load i32* %aa.addr, align 4
  %mul1 = mul nsw i32 %2, 17
  %3 = load i32* %cc, align 4
  %sub = sub nsw i32 %mul1, %3
  store i32 %sub, i32* %dd, align 4
  %4 = load i32* %dd, align 4
  ret i32 %4

; The nop here is to prevent add/bic to straddle a bundle boundary
; CHECK:      nop
; CHECK-NEXT: add sp, sp, #16
; CHECK-NEXT: bic	sp, sp, #3221225472

}

