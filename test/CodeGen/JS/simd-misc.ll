; RUN: llc < %s | FileCheck %s

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

declare <4 x float> @emscripten_float32x4_reciprocalApproximation(<4 x float>)
declare <4 x float> @emscripten_float32x4_reciprocalSqrtApproximation(<4 x float>)

; CHECK: function _test_rcp($a) {
; CHECK: $a = SIMD_Float32x4_check($a);
; CHECK: SIMD_Float32x4_reciprocalApproximation
; CHECK:}
define <4 x float> @test_rcp(<4 x float> %a) {
    %c = call <4 x float> @emscripten_float32x4_reciprocalApproximation(<4 x float> %a)
    ret <4 x float> %c
}

; CHECK: function _test_rsqrt($a) {
; CHECK: $a = SIMD_Float32x4_check($a);
; CHECK: SIMD_Float32x4_reciprocalSqrtApproximation
; CHECK:}
define <4 x float> @test_rsqrt(<4 x float> %a) {
    %c = call <4 x float> @emscripten_float32x4_reciprocalSqrtApproximation(<4 x float> %a)
    ret <4 x float> %c
}

; CHECK: function _sext_vec($a) {
; CHECK:  $b = SIMD_Int32x4_select($a, SIMD_Int32x4_splat(-1), SIMD_Int32x4_splat(0));
; CHECK: }
define <4 x i32> @sext_vec(<4 x i1> %a) {
    %b = sext <4 x i1> %a to <4 x i32>
    ret <4 x i32> %b
}

; CHECK: function _zext_vec($a) {
; CHECK:  $b = SIMD_Int32x4_select($a, SIMD_Int32x4_splat(1), SIMD_Int32x4_splat(0));
; CHECK: }
define <4 x i32> @zext_vec(<4 x i1> %a) {
    %b = zext <4 x i1> %a to <4 x i32>
    ret <4 x i32> %b
}
