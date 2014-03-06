; RUN: llc < %s -march=js -o - | FileCheck %s

; CHECK: function _simple_integer_math(
; CHECK:   return (((((((((((($a) + ($b))|0)*20)|0)|0) / ($a|0))&-1)) - 3)|0)|0);
define i32 @simple_integer_math(i32 %a, i32 %b) nounwind {
  %c = add i32 %a, %b
  %d = mul i32 %c, 20
  %e = sdiv i32 %d, %a
  %f = sub i32 %e, 3
  ret i32 %f
}

; CHECK: function _fneg(
; CHECK:   [[VAL_D:\$[a-z]+]] = +[[VAL_D]]
; CHECK:   return (+(-[[VAL_D]]));
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
