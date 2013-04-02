; RUN: opt < %s -instcombine -S | FileCheck %s
; Test that instcombine does not introduce non-power-of-two integers into
; the module

target datalayout = "p:32:32:32"

; This test is a counterpart to icmp_shl16 in
; test/Transforms/InstCombine/icmp.ll, which should still pass.
; CHECK: @icmp_shl31
; CHECK-NOT: i31
define i1 @icmp_shl31(i32 %x) {
  %shl = shl i32 %x, 1
  %cmp = icmp slt i32 %shl, 36
  ret i1 %cmp
}

; Check that we don't introduce i4, which is a power of 2 but still not allowed.
; CHECK: @icmp_shl4
; CHECK-NOT: i4
define i1 @icmp_shl4(i32 %x) {
  %shl = shl i32 %x, 28
  %cmp = icmp slt i32 %shl, 1073741824
  ret i1 %cmp
}
