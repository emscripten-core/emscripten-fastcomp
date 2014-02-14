; RUN: opt -S -nacl-promote-ints < %s | FileCheck %s

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:32"

@from = external global [300 x i8]
@to = external global [300 x i8]

; CHECK:      define void @load_bc_to_i80() {
; CHECK-NEXT:   %loaded.short.lo = load i64* bitcast ([300 x i8]* @from to i64*), align 4
; CHECK-NEXT:   %loaded.short.lo.ext = zext i64 %loaded.short.lo to i128
; CHECK-NEXT:   %loaded.short.hi = load i16* bitcast (i64* getelementptr (i64* bitcast ([300 x i8]* @from to i64*), i32 1) to i16*)
; CHECK-NEXT:   %loaded.short.hi.ext = zext i16 %loaded.short.hi to i128
; CHECK-NEXT:   %loaded.short.hi.ext.sh = shl i128 %loaded.short.hi.ext, 64
; CHECK-NEXT:   %loaded.short = or i128 %loaded.short.lo.ext, %loaded.short.hi.ext.sh
; CHECK-NEXT:   %loaded.short.lo1 = trunc i128 %loaded.short to i64
; CHECK-NEXT:   store i64 %loaded.short.lo1, i64* bitcast ([300 x i8]* @to to i64*), align 4
; CHECK-NEXT:   %loaded.short.hi.sh = lshr i128 %loaded.short, 64
; CHECK-NEXT:   %loaded.short.hi2 = trunc i128 %loaded.short.hi.sh to i16
; CHECK-NEXT:   store i16 %loaded.short.hi2, i16* bitcast (i64* getelementptr (i64* bitcast ([300 x i8]* @to to i64*), i32 1) to i16*)
; CHECK-NEXT:   ret void
; CHECK-NEXT: }
define void @load_bc_to_i80() {
  %loaded.short = load i80* bitcast ([300 x i8]* @from to i80*), align 4
  store i80 %loaded.short, i80* bitcast ([300 x i8]* @to to i80*), align 4
  ret void
}
