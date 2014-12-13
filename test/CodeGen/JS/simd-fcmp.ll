; RUN: llc < %s | FileCheck %s

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

; CHECK: function _test_ueq($a,$b) {
; CHECK: $a = SIMD_float32x4($a);
; CHECK: $b = SIMD_float32x4($b);
; CHECK: SIMD_float32x4_or(SIMD_float32x4_or(SIMD_float32x4_notEqual($a, $a), SIMD_float32x4_notEqual($b, $b)), SIMD_float32x4_equal($a, $b));
; CHECK: return (SIMD_int32x4($c));
; CHECK:}
define <4 x i1> @test_ueq(<4 x float> %a, <4 x float> %b) {
    %c = fcmp ueq <4 x float> %a, %b
    ret <4 x i1> %c
}

; CHECK: function _test_ord($a,$b) {
; CHECK: $a = SIMD_float32x4($a);
; CHECK: $b = SIMD_float32x4($b);
; CHECK: SIMD_float32x4_or(SIMD_float32x4_or(SIMD_float32x4_notEqual($a, $a), SIMD_float32x4_notEqual($b, $b)), SIMD_float32x4_equal($a, $b));
; CHECK: return (SIMD_int32x4($c));
; CHECK:}
define <4 x i1> @test_ord(<4 x float> %a, <4 x float> %b) {
    %c = fcmp ueq <4 x float> %a, %b
    ret <4 x i1> %c
}

; CHECK:function _test_uno($a,$b) {
; CHECK: $a = SIMD_float32x4($a);
; CHECK: $b = SIMD_float32x4($b);
; CHECK: SIMD_float32x4_or(SIMD_float32x4_or(SIMD_float32x4_notEqual($a, $a), SIMD_float32x4_notEqual($b, $b)), SIMD_float32x4_equal($a, $b));
; CHECK: return (SIMD_int32x4($c));
; CHECK:}
define <4 x i1> @test_uno(<4 x float> %a, <4 x float> %b) {
    %c = fcmp ueq <4 x float> %a, %b
    ret <4 x i1> %c
}
