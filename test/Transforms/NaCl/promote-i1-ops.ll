; RUN: opt %s -nacl-promote-i1-ops -S | FileCheck %s

; Test that the PromoteI1Ops pass expands out i1 loads/stores and i1
; comparison and arithmetic operations, with the exception of "and",
; "or" and "xor".


; i1 loads and stores are converted to i8 load and stores with
; explicit casts.

define i1 @load(i1* %ptr) {
  %val = load i1, i1* %ptr
  ret i1 %val
}
; CHECK: define i1 @load
; CHECK-NEXT: %ptr.i8ptr = bitcast i1* %ptr to i8*
; CHECK-NEXT: %val.pre_trunc = load i8, i8* %ptr.i8ptr
; CHECK-NEXT: %val = trunc i8 %val.pre_trunc to i1

define void @store(i1 %val, i1* %ptr) {
  store i1 %val, i1* %ptr
  ret void
}
; CHECK: define void @store
; CHECK-NEXT: %ptr.i8ptr = bitcast i1* %ptr to i8*
; CHECK-NEXT: %val.expand_i1_val = zext i1 %val to i8
; CHECK-NEXT: store i8 %val.expand_i1_val, i8* %ptr.i8ptr


; i1 arithmetic and comparisons are converted to their i8 equivalents
; with explicit casts.

define i1 @add(i1 %x, i1 %y) {
  %result = add i1 %x, %y
  ret i1 %result
}
; CHECK: define i1 @add
; CHECK-NEXT: %x.expand_i1_val = zext i1 %x to i8
; CHECK-NEXT: %y.expand_i1_val = zext i1 %y to i8
; CHECK-NEXT: %result.pre_trunc = add i8 %x.expand_i1_val, %y.expand_i1_val
; CHECK-NEXT: %result = trunc i8 %result.pre_trunc to i1

define i1 @compare(i1 %x, i1 %y) {
  %result = icmp slt i1 %x, %y
  ret i1 %result
}
; CHECK: define i1 @compare
; CHECK-NEXT: %x.expand_i1_val = sext i1 %x to i8
; CHECK-NEXT: %y.expand_i1_val = sext i1 %y to i8
; CHECK-NEXT: %result = icmp slt i8 %x.expand_i1_val, %y.expand_i1_val


; Non-shift bitwise operations should not be modified.
define void @bitwise_ops(i1 %x, i1 %y) {
  %and = and i1 %x, %y
  %or = or i1 %x, %y
  %xor = xor i1 %x, %y
  ret void
}
; CHECK: define void @bitwise_ops
; CHECK-NEXT: %and = and i1 %x, %y
; CHECK-NEXT: %or = or i1 %x, %y
; CHECK-NEXT: %xor = xor i1 %x, %y


define void @unchanged_cases(i32 %x, i32 %y, i32* %ptr) {
  %add = add i32 %x, %y
  %cmp = icmp slt i32 %x, %y
  %val = load i32, i32* %ptr
  store i32 %x, i32* %ptr
  ret void
}
; CHECK: define void @unchanged_cases
; CHECK-NEXT: %add = add i32 %x, %y
; CHECK-NEXT: %cmp = icmp slt i32 %x, %y
; CHECK-NEXT: %val = load i32, i32* %ptr
; CHECK-NEXT: store i32 %x, i32* %ptr

define void @i1_switch(i1 %a) {
entry:
  switch i1 %a, label %impossible [
    i1 true, label %truedest
    i1 false, label %falsedest
  ]

impossible:
  %phi = phi i32 [ 123, %entry ]
  unreachable

truedest:
  unreachable

falsedest:
  unreachable
}
; CHECK-LABEL: define void @i1_switch
; CHECK-LABEL: entry:
; CHECK-NEXT:    br i1 %a, label %truedest, label %falsedest
; CHECK-LABEL: impossible:
; CHECK-NEXT:    unreachable
; CHECK-LABEL: truedest:
; CHECK-NEXT:    unreachable
; CHECK-LABEL: falsedest:
; CHECK-NEXT:    unreachable

define void @i1_switch_default_true(i1 %a) {
entry:
  switch i1 %a, label %truedest [
    i1 false, label %falsedest
  ]

truedest:
  unreachable
falsedest:
  unreachable
}
; CHECK-LABEL: define void @i1_switch_default_true(i1 %a)
; CHECK-LABEL: entry:
; CHECK-NEXT:    br i1 %a, label %truedest, label %falsedest
; CHECK-LABEL: truedest:
; CHECK-NEXT:    unreachable
; CHECK-LABEL: falsedest:
; CHECK-NEXT:    unreachable

define void @i1_switch_default_false(i1 %a) {
entry:
  switch i1 %a, label %falsedest [
    i1 true, label %truedest
  ]

truedest:
  unreachable
falsedest:
  unreachable
}
; CHECK-LABEL: define void @i1_switch_default_false(i1 %a)
; CHECK-LABEL: entry:
; CHECK-NEXT:    br i1 %a, label %truedest, label %falsedest
; CHECK-LABEL: truedest:
; CHECK-NEXT:    unreachable
; CHECK-LABEL: falsedest:
; CHECK-NEXT:    unreachable

