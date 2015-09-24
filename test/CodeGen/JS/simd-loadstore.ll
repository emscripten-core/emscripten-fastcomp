; RUN: llc < %s | FileCheck %s

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

; CHECK: function _fx1($p) {
; CHECK:  $p = $p|0;
; CHECK:  var $s = SIMD_Float32x4(0,0,0,0), $t = SIMD_Float32x4(0,0,0,0), label = 0, sp = 0, temp_Float32x4_ptr = 0;
; CHECK:  $t = SIMD_Float32x4_load1(HEAPU8, $p);
; CHECK:  $s = SIMD_Float32x4_add($t,SIMD_Float32x4_splat(Math_fround(+0.5)));
; CHECK:  temp_Float32x4_ptr = $p;SIMD_Float32x4_store1(HEAPU8, temp_Float32x4_ptr, $s);
; CHECK:  return;
; CHECK: }
define void @fx1(i8* %p) {
    %q = bitcast i8* %p to <1 x float>*
    %t = load <1 x float>, <1 x float>* %q
    %s = fadd <1 x float> %t, <float 0.5>
    store <1 x float> %s, <1 x float>* %q
    ret void
}

; CHECK: function _fx2($p) {
; CHECK:  $p = $p|0;
; CHECK:  $s = SIMD_Float32x4(0,0,0,0), $t = SIMD_Float32x4(0,0,0,0), label = 0, sp = 0, temp_Float32x4_ptr = 0;
; CHECK:  $t = SIMD_Float32x4_load2(HEAPU8, $p);
; CHECK:  $s = SIMD_Float32x4_add($t,SIMD_Float32x4(Math_fround(+3.5),Math_fround(+7.5),Math_fround(+0),Math_fround(+0)));
; CHECK:  temp_Float32x4_ptr = $p;SIMD_Float32x4_store2(HEAPU8, temp_Float32x4_ptr, $s);
; CHECK:  return;
; CHECK: }
define void @fx2(i8* %p) {
    %q = bitcast i8* %p to <2 x float>*
    %t = load <2 x float>, <2 x float>* %q
    %s = fadd <2 x float> %t, <float 3.5, float 7.5>
    store <2 x float> %s, <2 x float>* %q
    ret void
}

; CHECK: function _fx3($p) {
; CHECK:  $p = $p|0;
; CHECK:  var $s = SIMD_Float32x4(0,0,0,0), $t = SIMD_Float32x4(0,0,0,0), label = 0, sp = 0, temp_Float32x4_ptr = 0;
; CHECK:  $t = SIMD_Float32x4_load3(HEAPU8, $p);
; CHECK:  $s = SIMD_Float32x4_add($t,SIMD_Float32x4(Math_fround(+1.5),Math_fround(+4.5),Math_fround(+6.5),Math_fround(+0)));
; CHECK:  temp_Float32x4_ptr = $p;SIMD_Float32x4_store3(HEAPU8, temp_Float32x4_ptr, $s);
; CHECK:  return;
; CHECK: }
define void @fx3(i8* %p) {
    %q = bitcast i8* %p to <3 x float>*
    %t = load <3 x float>, <3 x float>* %q
    %s = fadd <3 x float> %t, <float 1.5, float 4.5, float 6.5>
    store <3 x float> %s, <3 x float>* %q
    ret void
}

; CHECK: function _fx4($p) {
; CHECK:  $p = $p|0;
; CHECK:  var $s = SIMD_Float32x4(0,0,0,0), $t = SIMD_Float32x4(0,0,0,0), label = 0, sp = 0, temp_Float32x4_ptr = 0;
; CHECK:  $t = SIMD_Float32x4_load(HEAPU8, $p);
; CHECK:  $s = SIMD_Float32x4_add($t,SIMD_Float32x4(Math_fround(+9.5),Math_fround(+5.5),Math_fround(+1.5),Math_fround(+-3.5)));
; CHECK:  temp_Float32x4_ptr = $p;SIMD_Float32x4_store(HEAPU8, temp_Float32x4_ptr, $s);
; CHECK:  return;
; CHECK: }
define void @fx4(i8* %p) {
    %q = bitcast i8* %p to <4 x float>*
    %t = load <4 x float>, <4 x float>* %q
    %s = fadd <4 x float> %t, <float 9.5, float 5.5, float 1.5, float -3.5>
    store <4 x float> %s, <4 x float>* %q
    ret void
}
