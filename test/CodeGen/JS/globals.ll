; RUN: llc -march=js %s -o - | FileCheck %s

; Test simple global variable codegen.

; CHECK: function _loads() {
; CHECK:  [[VAR_t:\$[a-z]+]] = HEAP32[2]|0;
; CHECK:  [[VAR_s:\$[a-z]+]] = +HEAPF64[2];
; CHECK:  [[VAR_u:\$[a-z]+]] = HEAP8[24]|0;
; CHECK:  [[VAR_a:\$[a-z]+]] = ~~(([[VAR_s:\$[a-z]+]]))>>>0;
; CHECK:  [[VAR_b:\$[a-z]+]] = [[VAR_u:\$[a-z]+]] << 24 >> 24;
; CHECK:  [[VAR_c:\$[a-z]+]] = (([[VAR_t:\$[a-z]+]]) + ([[VAR_a:\$[a-z]+]]))|0;
; CHECK:  [[VAR_d:\$[a-z]+]] = (([[VAR_c:\$[a-z]+]]) + ([[VAR_b:\$[a-z]+]]))|0;
; CHECK:  return [[VAR_d:\$[a-z]+]]|0;
define i32 @loads() {
  %t = load i32* @A
  %s = load double* @B
  %u = load i8* @C
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
; CHECK:  HEAP32[2] = [[VAR_n:\$[a-z]+]];
; CHECK:  HEAPF64[2] = [[VAR_o:\$[a-z]+]];
; CHECK:  HEAP8[24] = [[VAR_m:\$[a-z]+]];
define void @stores(i8 %m, i32 %n, double %o) {
  store i32 %n, i32* @A
  store double %o, double* @B
  store i8 %m, i8* @C
  ret void
}

; CHECK: allocate([133,26,0,0,0,0,0,0,205,204,204,204,204,76,55,64,2,0,0,0,0,0,0,0], "i8", ALLOC_NONE, Runtime.GLOBAL_BASE);
@A = global i32 6789
@B = global double 23.3
@C = global i8 2
