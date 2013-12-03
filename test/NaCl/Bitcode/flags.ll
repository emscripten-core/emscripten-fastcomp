; Test that we no longer support the "nuw", "nsw", or the "exact" attributes on
; binary operators in PNaCl bitcode files, since the PNaClABI doesn't allow
; these attributes.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-thaw | llvm-dis - \
; RUN:              | FileCheck %s

define void @WrapFlags(i32, i32) {
  %3 = add nuw i32 %0, %1
  %4 = add nsw i32 %0, %1
  %5 = udiv exact i32 %0, %1
  ret void
}

; CHECK: %3 = add i32 %0, %1
; CHECK: %4 = add i32 %0, %1
; CHECK: %5 = udiv i32 %0, %1
