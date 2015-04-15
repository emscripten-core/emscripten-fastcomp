; RUN: not pnacl-abicheck < %s | FileCheck %s

; Most arithmetic operations are not very useful on i1, so use of i1
; is restricted to a subset of operations.


; i1 is allowed on these bitwise operations because:
;  * These operations never overflow.
;  * They do get generated in practice for combining conditions.
define internal void @allowed_cases() {
  %and = and i1 0, 0
  %or = or i1 0, 0
  %xor = xor i1 0, 0

  %v4and = and <4 x i1> undef, undef
  %v4or = or <4 x i1> undef, undef
  %v4xor = xor <4 x i1> undef, undef

  %v8and = and <8 x i1> undef, undef
  %v8or = or <8 x i1> undef, undef
  %v8xor = xor <8 x i1> undef, undef

  %v16and = and <16 x i1> undef, undef
  %v16or = or <16 x i1> undef, undef
  %v16xor = xor <16 x i1> undef, undef
  ret void
}
; CHECK-NOT: disallowed


define internal void @rejected_cases(i32 %ptr) {
  ; Loads and stores of i1 are disallowed.  This is done by rejecting
  ; i1* as a pointer type.
  %ptr.p = inttoptr i32 %ptr to i1*
; CHECK: disallowed: bad result type: i1* %ptr.p = inttoptr
  %vptr.p = inttoptr i32 %ptr to <4 x i1>*
; CHECK: disallowed: bad result type: <4 x i1>* %vptr.p = inttoptr
  load i1* %ptr.p, align 1
; CHECK-NEXT: disallowed: bad pointer: {{.*}} load i1*
  load <4 x i1>* %vptr.p, align 4
; CHECK-NEXT: disallowed: bad pointer: {{.*}} load <4 x i1>*

  ; Check that we don't allow weird sizes.
  add i31 0, 0
; CHECK-NEXT:  disallowed: Invalid integer arithmetic type: {{.*}} add i31

  ; i1 arithmetic is of dubious usefulness, so it is rejected.
  add i1 0, 0
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} add i1
  sub i1 0, 0
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} sub i1
  mul i1 0, 0
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} = mul i1
  udiv i1 0, 0
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} = udiv i1
  sdiv i1 0, 0
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} = sdiv i1
  urem i1 0, 0
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} = urem i1
  srem i1 0, 0
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} = srem i1
  shl i1 0, 0
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} = shl i1
  lshr i1 0, 0
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} lshr i1
  ashr i1 0, 0
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} ashr i1
  add <4 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} add <4 x i1>
  sub <4 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} sub <4 x i1>
  mul <4 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} mul <4 x i1>
  udiv <4 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} udiv <4 x i1>
  sdiv <4 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} sdiv <4 x i1>
  urem <4 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} urem <4 x i1>
  srem <4 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} srem <4 x i1>
  shl <4 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} shl <4 x i1>
  lshr <4 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} lshr <4 x i1>
  ashr <4 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} ashr <4 x i1>
  add <8 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} add <8 x i1>
  sub <8 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} sub <8 x i1>
  mul <8 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} mul <8 x i1>
  udiv <8 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} udiv <8 x i1>
  sdiv <8 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} sdiv <8 x i1>
  urem <8 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} urem <8 x i1>
  srem <8 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} srem <8 x i1>
  shl <8 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} shl <8 x i1>
  lshr <8 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} lshr <8 x i1>
  ashr <8 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} ashr <8 x i1>
  add <16 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} add <16 x i1>
  sub <16 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} sub <16 x i1>
  mul <16 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} mul <16 x i1>
  udiv <16 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} udiv <16 x i1>
  sdiv <16 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} sdiv <16 x i1>
  urem <16 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} urem <16 x i1>
  srem <16 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} srem <16 x i1>
  shl <16 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} shl <16 x i1>
  lshr <16 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} lshr <16 x i1>
  ashr <16 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} ashr <16 x i1>

  ; The same applies to i1 comparisons.
  icmp eq i1 0, 0
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} icmp eq i1
  icmp ult i1 0, 0
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} icmp ult i1
  icmp eq <4 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} icmp eq <4 x i1>
  icmp ult <4 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} icmp ult <4 x i1>
  icmp eq <8 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} icmp eq <8 x i1>
  icmp ult <8 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} icmp ult <8 x i1>
  icmp eq <16 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} icmp eq <16 x i1>
  icmp ult <16 x i1> undef, undef
; CHECK-NEXT: disallowed: Invalid integer arithmetic type: {{.*}} icmp ult <16 x i1>

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


; This stops the verifier from complaining about the lack of an entry point.
define void @_start(i32 %arg) {
  ret void
}
