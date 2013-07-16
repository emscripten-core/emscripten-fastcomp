; RUN: pnacl-llc -mtriple=armv7-unknown-nacl -sfi-branch -filetype=obj %s -o - \
; RUN:  | llvm-objdump -disassemble -triple armv7 - | FileCheck %s

define i32 @foo(i32 %aa, i32 %bb) nounwind {
entry:
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

; This checks two things:
; 1. bx lr is sandboxed by prepending a bic
; 2. The bic/bx pair don't straddle a 16-byte bundle boundary, hence the nop
; CHECK:      nop
; CHECK-NEXT: bic	lr, lr, #3221225487
; CHECK-NEXT: bx lr

}

define i32 @bar(i32 %aa, i32 %bb) nounwind {
entry:

; Check that the function start is padded with nops to start at a bundle
; boundary
; CHECK:      nop
; CHECK-NEXT: nop
; CHECK-NOT:  :
; CHECK: bar:
; CHECK-NEXT: push

  %aa.addr = alloca i32, align 4
  %bb.addr = alloca i32, align 4
  store i32 %aa, i32* %aa.addr, align 4
  store i32 %bb, i32* %bb.addr, align 4
  %0 = load i32* %aa.addr, align 4
  %mul = mul nsw i32 %0, 19
  %call = call i32 @foo(i32 %mul, i32 7)

; Check that the call is padded to be at the end of a bundle
; CHECK:      nop
; CHECK-NEXT: nop
; CHECK-NEXT: nop
; CHECK-NEXT: bl

  %1 = load i32* %bb.addr, align 4
  %mul1 = mul nsw i32 %1, 31
  %2 = load i32* %bb.addr, align 4
  %div = sdiv i32 %2, 7
  %add = add nsw i32 %div, 191
  %call2 = call i32 @foo(i32 %mul1, i32 %add)

; Check that the call is padded to be at the end of a bundle
; CHECK:      nop
; CHECK-NEXT: nop
; CHECK-NEXT: bl

  %add3 = add nsw i32 %call, %call2
  ret i32 %add3
}

