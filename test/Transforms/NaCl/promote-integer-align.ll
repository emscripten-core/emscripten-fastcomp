; RUN: opt -S -nacl-promote-ints < %s | FileCheck %s

target datalayout = "e-p:32:32:32-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:32:64-f32:32:32-f64:32:64-v64:64:64-v128:128:128-a0:0:64-f80:128:128-n8:16:32"

; CHECK:     define void @aligned_copy(i24* %p, i24* %q) {
; CHECK-NEXT:  %p.loty = bitcast i24* %p to i16*
; CHECK-NEXT:  %t.lo = load i16* %p.loty, align 64
; CHECK-NEXT:  %t.lo.ext = zext i16 %t.lo to i32
; CHECK-NEXT:  %p.hi = getelementptr i16* %p.loty, i32 1
; CHECK-NEXT:  %p.hity = bitcast i16* %p.hi to i8*
; CHECK-NEXT:  %t.hi = load i8* %p.hity, align 1
; CHECK-NEXT:  %t.hi.ext = zext i8 %t.hi to i32
; CHECK-NEXT:  %t.hi.ext.sh = shl i32 %t.hi.ext, 16
; CHECK-NEXT:  %t = or i32 %t.lo.ext, %t.hi.ext.sh
; CHECK-NEXT:  %q.loty = bitcast i24* %q to i16*
; CHECK-NEXT:  %t.lo1 = trunc i32 %t to i16
; CHECK-NEXT:  store i16 %t.lo1, i16* %q.loty, align 64
; CHECK-NEXT:  %t.hi.sh = lshr i32 %t, 16
; CHECK-NEXT:  %q.hi = getelementptr i16* %q.loty, i32 1
; CHECK-NEXT:  %t.hi2 = trunc i32 %t.hi.sh to i8
; CHECK-NEXT:  %q.hity = bitcast i16* %q.hi to i8*
; CHECK-NEXT:  store i8 %t.hi2, i8* %q.hity, align 1
; CHECK-NEXT:  ret void
; CHECK-NEXT:}
define void @aligned_copy(i24* %p, i24* %q) {
  %t = load i24* %p, align 64
  store i24 %t, i24* %q, align 64
  ret void
}
