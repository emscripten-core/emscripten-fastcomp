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
