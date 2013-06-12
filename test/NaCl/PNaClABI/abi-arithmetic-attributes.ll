; RUN: pnacl-abicheck < %s | FileCheck %s

; This tests that the arithmetic attributes "nuw" and "nsw" ("no
; unsigned wrap" and "no signed wrap") and "exact" are disallowed by
; the PNaCl ABI verifier.

define void @allowed_cases() {
  %add = add i32 1, 2
  %shl = shl i32 3, 4
  %udiv = udiv i32 4, 2
  %lshr = lshr i32 2, 1
  %ashr = ashr i32 2, 1
  ret void
}
; CHECK-NOT: disallowed


define void @rejected_cases() {
  %add = add nsw i32 1, 2
; CHECK: disallowed: has "nsw" attribute: %add
  %shl1 = shl nuw i32 3, 4
; CHECK-NEXT: disallowed: has "nuw" attribute: %shl1
  %sub = sub nsw nuw i32 5, 6
; CHECK-NEXT: disallowed: has "nuw" attribute: %sub

  %lshr = lshr exact i32 2, 1
; CHECK-NEXT: disallowed: has "exact" attribute: %lshr
  %ashr = ashr exact i32 2, 1
; CHECK-NEXT: disallowed: has "exact" attribute: %ashr
  %udiv = udiv exact i32 4, 2
; CHECK-NEXT: disallowed: has "exact" attribute: %udiv

  ret void
}
; CHECK-NOT: disallowed
