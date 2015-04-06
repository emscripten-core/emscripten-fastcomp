; RUN: llc < %s | FileCheck %s

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

; CHECK: function _simple_integer_math(
; CHECK:   [[VAL_A:\$[a-z]+]] = [[VAL_A]]|0;
; CHECK:   [[VAL_B:\$[a-z]+]] = [[VAL_B]]|0;
; CHECK:   [[VAL_C:\$[a-z]+]] = (([[VAL_A]]) + ([[VAL_B]]))|0;
; CHECK:   [[VAL_D:\$[a-z]+]] = ([[VAL_C]]*20)|0;
; CHECK:   [[VAL_E:\$[a-z]+]] = (([[VAL_D]]|0) / ([[VAL_A]]|0))&-1;
; CHECK:   [[VAL_F:\$[a-z]+]] = (([[VAL_E]]) - 3)|0;
; CHECK:   return ([[VAL_F]]|0);
define i32 @simple_integer_math(i32 %a, i32 %b) nounwind {
  %c = add i32 %a, %b
  %d = mul i32 %c, 20
  %e = sdiv i32 %d, %a
  %f = sub i32 %e, 3
  ret i32 %f
}

; CHECK: function _fneg(
; CHECK:   [[VAL_D:\$[a-z]+]] = +[[VAL_D]]
; CHECK:   [[VAL_F:\$[a-z]+]] = +0
; CHECK:   [[VAL_F]] = -[[VAL_D]]
; CHECK:   return (+[[VAL_F]]);
define double @fneg(double %d) nounwind {
  %f = fsub double -0.0, %d
  ret double %f
}

; CHECK: function _flt_rounds(
; CHECK: t = 1;
declare i32 @llvm.flt.rounds()
define i32 @flt_rounds() {
  %t = call i32 @llvm.flt.rounds()
  ret i32 %t
}
