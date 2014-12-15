; RUN: llc < %s | FileCheck %s

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

; CHECK: function _test0($a,$b,$cond) {
; CHECK:  $a = SIMD_int32x4($a);
; CHECK:  $b = SIMD_int32x4($b);
; CHECK:  $cond = SIMD_int32x4($cond);
; CHECK:  $cmp = SIMD_int32x4_select($cond,$a,$b);
; CHECK:  return (SIMD_int32x4($cmp));
; CHECK: }
define <4 x i32> @test0(<4 x i32> %a, <4 x i32> %b, <4 x i1> %cond) nounwind {
entry:
  %cmp = select <4 x i1> %cond, <4 x i32> %a, <4 x i32> %b
  ret <4 x i32> %cmp
}

; CHECK: function _test1($a,$b,$cond) {
; CHECK:  $a = SIMD_float32x4($a);
; CHECK:  $b = SIMD_float32x4($b);
; CHECK:  $cond = SIMD_int32x4($cond);
; CHECK:  $cmp = SIMD_float32x4_select($cond,$a,$b);
; CHECK:  return (SIMD_float32x4($cmp));
; CHECK: }
define <4 x float> @test1(<4 x float> %a, <4 x float> %b, <4 x i1> %cond) nounwind {
entry:
  %cmp = select <4 x i1> %cond, <4 x float> %a, <4 x float> %b
  ret <4 x float> %cmp
}
