; RUN: llc < %s | FileCheck %s

; Test simple global variable codegen.

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

; CHECK: function _loads() {
; CHECK:  [[VAR_t:\$[a-z]+]] = HEAP32[4]|0;
; CHECK:  [[VAR_s:\$[a-z]+]] = +HEAPF64[1];
; CHECK:  [[VAR_u:\$[a-z]+]] = HEAP8[20]|0;
; CHECK:  [[VAR_a:\$[a-z]+]] = (~~(([[VAR_s:\$[a-z]+]]))>>>0);
; CHECK:  [[VAR_b:\$[a-z]+]] = [[VAR_u:\$[a-z]+]] << 24 >> 24;
; CHECK:  [[VAR_c:\$[a-z]+]] = (([[VAR_t:\$[a-z]+]]) + ([[VAR_a:\$[a-z]+]]))|0;
; CHECK:  [[VAR_d:\$[a-z]+]] = (([[VAR_c:\$[a-z]+]]) + ([[VAR_b:\$[a-z]+]]))|0;
; CHECK:  return ([[VAR_d:\$[a-z]+]]|0);
define i32 @loads() {
  %t = load i32, i32* @A
  %s = load double, double* @B
  %u = load i8, i8* @C
  %a = fptoui double %s to i32
  %b = sext i8 %u to i32
  %c = add i32 %t, %a
  %d = add i32 %c, %b
  ret i32 %d
}

; CHECK: function _stores([[VAR_m:\$[a-z]+]],[[VAR_n:\$[a-z]+]],[[VAR_o:\$[a-z]+]]) {
; CHECK:  [[VAR_m:\$[a-z]+]] = [[VAR_m:\$[a-z]+]]|0;
; CHECK:  [[VAR_n:\$[a-z]+]] = [[VAR_n:\$[a-z]+]]|0;
; CHECK:  [[VAR_o:\$[a-z]+]] = +[[VAR_o:\$[a-z]+]];
; CHECK:  HEAP32[4] = [[VAR_n:\$[a-z]+]];
; CHECK:  HEAPF64[1] = [[VAR_o:\$[a-z]+]];
; CHECK:  HEAP8[20] = [[VAR_m:\$[a-z]+]];
define void @stores(i8 %m, i32 %n, double %o) {
  store i32 %n, i32* @A
  store double %o, double* @B
  store i8 %m, i8* @C
  ret void
}

; CHECK: allocate([205,204,204,204,204,76,55,64,133,26,0,0,2], "i8", ALLOC_NONE, Runtime.GLOBAL_BASE);
@A = global i32 6789
@B = global double 23.3
@C = global i8 2
