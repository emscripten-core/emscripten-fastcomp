; RUN: llc -emscripten-precise-f32=false < %s | FileCheck %s
; RUN: llc -emscripten-precise-f32=true < %s | FileCheck --check-prefix=CHECK-PRECISE_F32 %s

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

; SIMD_Float32x4_splat needs a float32 input even if we're not in precise-f32 mode.

; CHECK: test(
; CHECK: $d = SIMD_Float32x4_splat(Math_fround($f));
; CHECK-PRECISE_F32: test(
; CHECK-PRECISE_F32: $f = Math_fround($f);
; CHECK-PRECISE_F32: $d = SIMD_Float32x4_splat($f);
define <4 x float> @test(float %f) {
  %a = insertelement <4 x float> undef, float %f, i32 0
  %b = insertelement <4 x float> %a, float %f, i32 1
  %c = insertelement <4 x float> %b, float %f, i32 2
  %d = insertelement <4 x float> %c, float %f, i32 3
  ret <4 x float> %d
}

; CHECK: test_insert(
; CHECK: $a = SIMD_Float32x4_replaceLane($v,0,Math_fround($g));
; CHECK-PRECISE_F32: test_insert(
; CHECK-PRECISE_F32: $g = Math_fround($g);
; CHECK-PRECISE_F32: $a = SIMD_Float32x4_replaceLane($v,0,$g);
define <4 x float> @test_insert(<4 x float> %v, float %g) {
  %a = insertelement <4 x float> %v, float %g, i32 0
  ret <4 x float> %a
}

; CHECK: test_ctor(
; CHECK: $d = SIMD_Float32x4(Math_fround($x), Math_fround($y), Math_fround($z), Math_fround($w));
; CHECK-PRECISE_F32: test_ctor(
; CHECK-PRECISE_F32: $x = Math_fround($x);
; CHECK-PRECISE_F32: $y = Math_fround($y);
; CHECK-PRECISE_F32: $z = Math_fround($z);
; CHECK-PRECISE_F32: $w = Math_fround($w);
; CHECK-PRECISE_F32: $d = SIMD_Float32x4($x, $y, $z, $w);
define <4 x float> @test_ctor(<4 x float> %v, float %x, float %y, float %z, float %w) {
  %a = insertelement <4 x float> undef, float %x, i32 0
  %b = insertelement <4 x float> %a, float %y, i32 1
  %c = insertelement <4 x float> %b, float %z, i32 2
  %d = insertelement <4 x float> %c, float %w, i32 3
  ret <4 x float> %d
}
