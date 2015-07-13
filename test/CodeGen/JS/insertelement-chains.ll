; RUN: llc -emscripten-precise-f32 < %s | FileCheck %s

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

; Basic constructor.

; CHECK: function _test0($x,$y,$z,$w) {
; CHECK:   $d = SIMD_Float32x4($x, $y, $z, $w)
; CHECK: }
define <4 x float> @test0(float %x, float %y, float %z, float %w) {
    %a = insertelement <4 x float> undef, float %x, i32 0
    %b = insertelement <4 x float> %a, float %y, i32 1
    %c = insertelement <4 x float> %b, float %z, i32 2
    %d = insertelement <4 x float> %c, float %w, i32 3
    ret <4 x float> %d
}

; Same as test0 but elements inserted in a different order.

; CHECK: function _test1($x,$y,$z,$w) {
; CHECK:   $d = SIMD_Float32x4($x, $y, $z, $w)
; CHECK: }
define <4 x float> @test1(float %x, float %y, float %z, float %w) {
    %a = insertelement <4 x float> undef, float %w, i32 3
    %b = insertelement <4 x float> %a, float %y, i32 1
    %c = insertelement <4 x float> %b, float %z, i32 2
    %d = insertelement <4 x float> %c, float %x, i32 0
    ret <4 x float> %d
}

; Overwriting elements.

; CHECK: function _test2($x,$y,$z,$w) {
; CHECK:   $h = SIMD_Float32x4($x, $y, $z, $w)
; CHECK: }
define <4 x float> @test2(float %x, float %y, float %z, float %w) {
    %a = insertelement <4 x float> undef, float %z, i32 0
    %b = insertelement <4 x float> %a, float %x, i32 0
    %c = insertelement <4 x float> %b, float %w, i32 1
    %d = insertelement <4 x float> %c, float %y, i32 1
    %e = insertelement <4 x float> %d, float %x, i32 2
    %f = insertelement <4 x float> %e, float %z, i32 2
    %g = insertelement <4 x float> %f, float %y, i32 3
    %h = insertelement <4 x float> %g, float %w, i32 3
    ret <4 x float> %h
}

; Basic splat testcase.

; CHECK: function _test3($x) {
; CHECK:   $d = SIMD_Float32x4_splat($x)
; CHECK: }
define <4 x float> @test3(float %x) {
    %a = insertelement <4 x float> undef, float %x, i32 0
    %b = insertelement <4 x float> %a, float %x, i32 1
    %c = insertelement <4 x float> %b, float %x, i32 2
    %d = insertelement <4 x float> %c, float %x, i32 3
    ret <4 x float> %d
}

; Same as test3 but elements inserted in a different order.

; CHECK: function _test4($x) {
; CHECK:   $d = SIMD_Float32x4_splat($x)
; CHECK: }
define <4 x float> @test4(float %x) {
    %a = insertelement <4 x float> undef, float %x, i32 3
    %b = insertelement <4 x float> %a, float %x, i32 1
    %c = insertelement <4 x float> %b, float %x, i32 2
    %d = insertelement <4 x float> %c, float %x, i32 0
    ret <4 x float> %d
}

; Insert chain.

; CHECK: function _test5($x,$y,$z,$w) {
; CHECK:   $f = SIMD_Float32x4_replaceLane(SIMD_Float32x4_replaceLane(SIMD_Float32x4_replaceLane(SIMD_Float32x4_splat(Math_fround(0)),0,$x),1,$y),2,$z)
; CHECK: }
define <4 x float> @test5(float %x, float %y, float %z, float %w) {
    %a = insertelement <4 x float> undef, float %z, i32 0
    %b = insertelement <4 x float> %a, float %x, i32 0
    %c = insertelement <4 x float> %b, float %w, i32 1
    %d = insertelement <4 x float> %c, float %y, i32 1
    %e = insertelement <4 x float> %d, float %x, i32 2
    %f = insertelement <4 x float> %e, float %z, i32 2
    ret <4 x float> %f
}

; Splat via insert+shuffle.

; CHECK: function _test6($x) {
; CHECK:   $b = SIMD_Float32x4_splat($x)
; CHECK: }
define <4 x float> @test6(float %x) {
    %a = insertelement <4 x float> undef, float %x, i32 0
    %b = shufflevector <4 x float> %a, <4 x float> undef, <4 x i32> zeroinitializer
    ret <4 x float> %b
}
