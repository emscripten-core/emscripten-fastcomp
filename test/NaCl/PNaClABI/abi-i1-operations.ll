; RUN: pnacl-abicheck < %s | FileCheck %s

; Most arithmetic operations are not very useful on i1, so use of i1
; is restricted to a subset of operations.


; i1 is allowed on these bitwise operations because:
;  * These operations never overflow.
;  * They do get generated in practice for combining conditions.
define internal void @allowed_cases() {
  %and = and i1 0, 0
  %or = or i1 0, 0
  %xor = xor i1 0, 0
  ret void
}
; CHECK-NOT: disallowed


define internal void @rejected_cases(i32 %ptr) {
  ; Loads and stores of i1 are disallowed.  This is done by rejecting
  ; i1* as a pointer type.
  %ptr.p = inttoptr i32 %ptr to i1*
; CHECK: disallowed: bad result type: %ptr.p = inttoptr
  load i1* %ptr.p, align 1
; CHECK-NEXT: disallowed: bad pointer: {{.*}} load i1*

  ; i1 arithmetic is of dubious usefulness, so it is rejected.
  add i1 0, 0
; CHECK-NEXT: disallowed: arithmetic on i1: {{.*}} add
  sub i1 0, 0
; CHECK-NEXT: disallowed: arithmetic on i1: {{.*}} sub
  mul i1 0, 0
; CHECK-NEXT: disallowed: arithmetic on i1: {{.*}} mul
  udiv i1 0, 0
; CHECK-NEXT: disallowed: arithmetic on i1: {{.*}} udiv
  sdiv i1 0, 0
; CHECK-NEXT: disallowed: arithmetic on i1: {{.*}} sdiv
  urem i1 0, 0
; CHECK-NEXT: disallowed: arithmetic on i1: {{.*}} urem
  srem i1 0, 0
; CHECK-NEXT: disallowed: arithmetic on i1: {{.*}} srem
  shl i1 0, 0
; CHECK-NEXT: disallowed: arithmetic on i1: {{.*}} shl
  lshr i1 0, 0
; CHECK-NEXT: disallowed: arithmetic on i1: {{.*}} lshr
  ashr i1 0, 0
; CHECK-NEXT: disallowed: arithmetic on i1: {{.*}} ashr

  ; The same applies to i1 comparisons.
  icmp eq i1 0, 0
; CHECK-NEXT: disallowed: arithmetic on i1: {{.*}} icmp eq
  icmp ult i1 0, 0
; CHECK-NEXT: disallowed: arithmetic on i1: {{.*}} icmp ult

  ; There should be no implicit zero-extension in alloca.
  alloca i8, i1 1
; CHECK-NEXT: disallowed: alloca array size is not i32

  ; Switch on i1 is not useful.  "br" should be used instead.
  switch i1 0, label %next [i1 0, label %next]
; CHECK-NEXT: disallowed: switch on i1
next:

  ret void
}
; CHECK-NOT: disallowed
