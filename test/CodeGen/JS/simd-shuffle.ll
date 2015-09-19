; RUN: llc < %s | FileCheck %s

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

; CHECK: function _splat_int32x4($a,$b) {
; CHECK:  $a = SIMD_Int32x4_check($a);
; CHECK:  $b = SIMD_Int32x4_check($b);
; CHECK:  var $sel = SIMD_Int32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Int32x4_swizzle($a, 0, 0, 0, 0);
; CHECK:  return (SIMD_Int32x4_check($sel));
; CHECK: }
define <4 x i32> @splat_int32x4(<4 x i32> %a, <4 x i32> %b) nounwind {
entry:
  %sel = shufflevector <4 x i32> %a, <4 x i32> %b, <4 x i32><i32 0, i32 0, i32 0, i32 0>
  ret <4 x i32> %sel
}

; CHECK: function _swizzle_int32x4($a,$b) {
; CHECK:  $a = SIMD_Int32x4_check($a);
; CHECK:  $b = SIMD_Int32x4_check($b);
; CHECK:  var $sel = SIMD_Int32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Int32x4_swizzle($a, 0, 3, 1, 2);
; CHECK:  return (SIMD_Int32x4_check($sel));
; CHECK: }
define <4 x i32> @swizzle_int32x4(<4 x i32> %a, <4 x i32> %b) nounwind {
entry:
  %sel = shufflevector <4 x i32> %a, <4 x i32> %b, <4 x i32><i32 0, i32 3, i32 1, i32 2>
  ret <4 x i32> %sel
}

; CHECK: function _swizzlehi_int32x4($a,$b) {
; CHECK:  $a = SIMD_Int32x4_check($a);
; CHECK:  $b = SIMD_Int32x4_check($b);
; CHECK:  var $sel = SIMD_Int32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Int32x4_swizzle($b, 2, 1, 3, 0);
; CHECK:  return (SIMD_Int32x4_check($sel));
; CHECK: }
define <4 x i32> @swizzlehi_int32x4(<4 x i32> %a, <4 x i32> %b) nounwind {
entry:
  %sel = shufflevector <4 x i32> %a, <4 x i32> %b, <4 x i32><i32 6, i32 5, i32 7, i32 4>
  ret <4 x i32> %sel
}

; CHECK: function _shuffleXY_float32x4to3($a,$b) {
; CHECK:  $a = SIMD_Float32x4_check($a);
; CHECK:  $b = SIMD_Float32x4_check($b);
; CHECK:  var $sel = SIMD_Float32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Float32x4_shuffle($a, $b, 7, 0, 0, 0);
; CHECK:  return (SIMD_Float32x4_check($sel));
; CHECK: }
define <3 x float> @shuffleXY_float32x4to3(<4 x float> %a, <4 x float> %b) nounwind {
entry:
  %sel = shufflevector <4 x float> %a, <4 x float> %b, <3 x i32><i32 7, i32 0, i32 undef>
  ret <3 x float> %sel
}

; CHECK: function _shuffle_int32x4($a,$b) {
; CHECK:  $a = SIMD_Int32x4_check($a);
; CHECK:  $b = SIMD_Int32x4_check($b);
; CHECK:  var $sel = SIMD_Int32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Int32x4_shuffle($a, $b, 7, 0, 5, 3);
; CHECK:  return (SIMD_Int32x4_check($sel));
; CHECK: }
define <4 x i32> @shuffle_int32x4(<4 x i32> %a, <4 x i32> %b) nounwind {
entry:
  %sel = shufflevector <4 x i32> %a, <4 x i32> %b, <4 x i32><i32 7, i32 0, i32 5, i32 3>
  ret <4 x i32> %sel
}

; CHECK: function _shuffleXY_int32x4($a,$b) {
; CHECK:  $a = SIMD_Int32x4_check($a);
; CHECK:  $b = SIMD_Int32x4_check($b);
; CHECK:  var $sel = SIMD_Int32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Int32x4_shuffle($a, $b, 7, 0, 0, 0);
; CHECK:  return (SIMD_Int32x4_check($sel));
; CHECK: }
define <4 x i32> @shuffleXY_int32x4(<4 x i32> %a, <4 x i32> %b) nounwind {
entry:
  %sel = shufflevector <4 x i32> %a, <4 x i32> %b, <4 x i32><i32 7, i32 0, i32 undef, i32 undef>
  ret <4 x i32> %sel
}

; CHECK: function _splat_int32x3($a,$b) {
; CHECK:  $a = SIMD_Int32x4_check($a);
; CHECK:  $b = SIMD_Int32x4_check($b);
; CHECK:  var $sel = SIMD_Int32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Int32x4_swizzle($a, 0, 0, 0, 0);
; CHECK:  return (SIMD_Int32x4_check($sel));
; CHECK: }
define <3 x i32> @splat_int32x3(<3 x i32> %a, <3 x i32> %b) nounwind {
entry:
  %sel = shufflevector <3 x i32> %a, <3 x i32> %b, <3 x i32><i32 0, i32 0, i32 0>
  ret <3 x i32> %sel
}

; CHECK: function _swizzle_int32x3($a,$b) {
; CHECK:  $a = SIMD_Int32x4_check($a);
; CHECK:  $b = SIMD_Int32x4_check($b);
; CHECK:  var $sel = SIMD_Int32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Int32x4_swizzle($a, 0, 2, 1, 0);
; CHECK:  return (SIMD_Int32x4_check($sel));
; CHECK: }
define <3 x i32> @swizzle_int32x3(<3 x i32> %a, <3 x i32> %b) nounwind {
entry:
  %sel = shufflevector <3 x i32> %a, <3 x i32> %b, <3 x i32><i32 0, i32 2, i32 1>
  ret <3 x i32> %sel
}

; CHECK: function _swizzlehi_int32x3($a,$b) {
; CHECK:  $a = SIMD_Int32x4_check($a);
; CHECK:  $b = SIMD_Int32x4_check($b);
; CHECK:  var $sel = SIMD_Int32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Int32x4_swizzle($b, 0, 2, 1, 0);
; CHECK:  return (SIMD_Int32x4_check($sel));
; CHECK: }
define <3 x i32> @swizzlehi_int32x3(<3 x i32> %a, <3 x i32> %b) nounwind {
entry:
  %sel = shufflevector <3 x i32> %a, <3 x i32> %b, <3 x i32><i32 3, i32 5, i32 4>
  ret <3 x i32> %sel
}

; CHECK: function _shuffle_int32x3($a,$b) {
; CHECK:  $a = SIMD_Int32x4_check($a);
; CHECK:  $b = SIMD_Int32x4_check($b);
; CHECK:  var $sel = SIMD_Int32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Int32x4_shuffle($a, $b, 6, 0, 5, 0);
; CHECK:  return (SIMD_Int32x4_check($sel));
; CHECK: }
define <3 x i32> @shuffle_int32x3(<3 x i32> %a, <3 x i32> %b) nounwind {
entry:
  %sel = shufflevector <3 x i32> %a, <3 x i32> %b, <3 x i32><i32 5, i32 0, i32 4>
  ret <3 x i32> %sel
}

; CHECK: function _shuffleXY_int32x3($a,$b) {
; CHECK:  $a = SIMD_Int32x4_check($a);
; CHECK:  $b = SIMD_Int32x4_check($b);
; CHECK:  var $sel = SIMD_Int32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Int32x4_shuffle($a, $b, 6, 0, 0, 0);
; CHECK:  return (SIMD_Int32x4_check($sel));
; CHECK: }
define <3 x i32> @shuffleXY_int32x3(<3 x i32> %a, <3 x i32> %b) nounwind {
entry:
  %sel = shufflevector <3 x i32> %a, <3 x i32> %b, <3 x i32><i32 5, i32 0, i32 undef>
  ret <3 x i32> %sel
}

; CHECK: function _splat_int32x3to4($a,$b) {
; CHECK:  $a = SIMD_Int32x4_check($a);
; CHECK:  $b = SIMD_Int32x4_check($b);
; CHECK:  var $sel = SIMD_Int32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Int32x4_swizzle($a, 0, 0, 0, 0);
; CHECK:  return (SIMD_Int32x4_check($sel));
; CHECK: }
define <4 x i32> @splat_int32x3to4(<3 x i32> %a, <3 x i32> %b) nounwind {
entry:
  %sel = shufflevector <3 x i32> %a, <3 x i32> %b, <4 x i32><i32 0, i32 0, i32 0, i32 0>
  ret <4 x i32> %sel
}

; CHECK: function _swizzle_int32x3to4($a,$b) {
; CHECK:  $a = SIMD_Int32x4_check($a);
; CHECK:  $b = SIMD_Int32x4_check($b);
; CHECK:  var $sel = SIMD_Int32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Int32x4_swizzle($a, 0, 2, 1, 2);
; CHECK:  return (SIMD_Int32x4_check($sel));
; CHECK: }
define <4 x i32> @swizzle_int32x3to4(<3 x i32> %a, <3 x i32> %b) nounwind {
entry:
  %sel = shufflevector <3 x i32> %a, <3 x i32> %b, <4 x i32><i32 0, i32 2, i32 1, i32 2>
  ret <4 x i32> %sel
}

; CHECK: function _swizzlehi_int32x3to4($a,$b) {
; CHECK:  $a = SIMD_Int32x4_check($a);
; CHECK:  $b = SIMD_Int32x4_check($b);
; CHECK:  var $sel = SIMD_Int32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Int32x4_swizzle($b, 2, 1, 0, 2);
; CHECK:  return (SIMD_Int32x4_check($sel));
; CHECK: }
define <4 x i32> @swizzlehi_int32x3to4(<3 x i32> %a, <3 x i32> %b) nounwind {
entry:
  %sel = shufflevector <3 x i32> %a, <3 x i32> %b, <4 x i32><i32 5, i32 4, i32 3, i32 5>
  ret <4 x i32> %sel
}

; CHECK: function _shuffle_int32x3to4($a,$b) {
; CHECK:  $a = SIMD_Int32x4_check($a);
; CHECK:  $b = SIMD_Int32x4_check($b);
; CHECK:  var $sel = SIMD_Int32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Int32x4_shuffle($a, $b, 6, 0, 5, 2);
; CHECK:  return (SIMD_Int32x4_check($sel));
; CHECK: }
define <4 x i32> @shuffle_int32x3to4(<3 x i32> %a, <3 x i32> %b) nounwind {
entry:
  %sel = shufflevector <3 x i32> %a, <3 x i32> %b, <4 x i32><i32 5, i32 0, i32 4, i32 2>
  ret <4 x i32> %sel
}

; CHECK: function _shuffleXY_int32x3to4($a,$b) {
; CHECK:  $a = SIMD_Int32x4_check($a);
; CHECK:  $b = SIMD_Int32x4_check($b);
; CHECK:  var $sel = SIMD_Int32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Int32x4_shuffle($a, $b, 6, 0, 0, 0);
; CHECK:  return (SIMD_Int32x4_check($sel));
; CHECK: }
define <4 x i32> @shuffleXY_int32x3to4(<3 x i32> %a, <3 x i32> %b) nounwind {
entry:
  %sel = shufflevector <3 x i32> %a, <3 x i32> %b, <4 x i32><i32 5, i32 0, i32 undef, i32 undef>
  ret <4 x i32> %sel
}

; CHECK: function _splat_int32x4to3($a,$b) {
; CHECK:  $a = SIMD_Int32x4_check($a);
; CHECK:  $b = SIMD_Int32x4_check($b);
; CHECK:  var $sel = SIMD_Int32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Int32x4_swizzle($a, 0, 0, 0, 0);
; CHECK:  return (SIMD_Int32x4_check($sel));
; CHECK: }
define <3 x i32> @splat_int32x4to3(<4 x i32> %a, <4 x i32> %b) nounwind {
entry:
  %sel = shufflevector <4 x i32> %a, <4 x i32> %b, <3 x i32><i32 0, i32 0, i32 0>
  ret <3 x i32> %sel
}

; CHECK: function _swizzle_int32x4to3($a,$b) {
; CHECK:  $a = SIMD_Int32x4_check($a);
; CHECK:  $b = SIMD_Int32x4_check($b);
; CHECK:  var $sel = SIMD_Int32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Int32x4_swizzle($a, 0, 3, 1, 0);
; CHECK:  return (SIMD_Int32x4_check($sel));
; CHECK: }
define <3 x i32> @swizzle_int32x4to3(<4 x i32> %a, <4 x i32> %b) nounwind {
entry:
  %sel = shufflevector <4 x i32> %a, <4 x i32> %b, <3 x i32><i32 0, i32 3, i32 1>
  ret <3 x i32> %sel
}

; CHECK: function _swizzlehi_int32x4to3($a,$b) {
; CHECK:  $a = SIMD_Int32x4_check($a);
; CHECK:  $b = SIMD_Int32x4_check($b);
; CHECK:  var $sel = SIMD_Int32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Int32x4_swizzle($b, 2, 1, 3, 0);
; CHECK:  return (SIMD_Int32x4_check($sel));
; CHECK: }
define <3 x i32> @swizzlehi_int32x4to3(<4 x i32> %a, <4 x i32> %b) nounwind {
entry:
  %sel = shufflevector <4 x i32> %a, <4 x i32> %b, <3 x i32><i32 6, i32 5, i32 7>
  ret <3 x i32> %sel
}

; CHECK: function _shuffle_int32x4to3($a,$b) {
; CHECK:  $a = SIMD_Int32x4_check($a);
; CHECK:  $b = SIMD_Int32x4_check($b);
; CHECK:  var $sel = SIMD_Int32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Int32x4_shuffle($a, $b, 7, 0, 5, 0);
; CHECK:  return (SIMD_Int32x4_check($sel));
; CHECK: }
define <3 x i32> @shuffle_int32x4to3(<4 x i32> %a, <4 x i32> %b) nounwind {
entry:
  %sel = shufflevector <4 x i32> %a, <4 x i32> %b, <3 x i32><i32 7, i32 0, i32 5>
  ret <3 x i32> %sel
}

; CHECK: function _shuffleXY_int32x4to3($a,$b) {
; CHECK:  $a = SIMD_Int32x4_check($a);
; CHECK:  $b = SIMD_Int32x4_check($b);
; CHECK:  var $sel = SIMD_Int32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Int32x4_shuffle($a, $b, 7, 0, 0, 0);
; CHECK:  return (SIMD_Int32x4_check($sel));
; CHECK: }
define <3 x i32> @shuffleXY_int32x4to3(<4 x i32> %a, <4 x i32> %b) nounwind {
entry:
  %sel = shufflevector <4 x i32> %a, <4 x i32> %b, <3 x i32><i32 7, i32 0, i32 undef>
  ret <3 x i32> %sel
}

; CHECK: function _splat_float32x4($a,$b) {
; CHECK:  $a = SIMD_Float32x4_check($a);
; CHECK:  $b = SIMD_Float32x4_check($b);
; CHECK:  var $sel = SIMD_Float32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Float32x4_swizzle($a, 0, 0, 0, 0);
; CHECK:  return (SIMD_Float32x4_check($sel));
; CHECK: }
define <4 x float> @splat_float32x4(<4 x float> %a, <4 x float> %b) nounwind {
entry:
  %sel = shufflevector <4 x float> %a, <4 x float> %b, <4 x i32><i32 0, i32 0, i32 0, i32 0>
  ret <4 x float> %sel
}

; CHECK: function _swizzle_float32x4($a,$b) {
; CHECK:  $a = SIMD_Float32x4_check($a);
; CHECK:  $b = SIMD_Float32x4_check($b);
; CHECK:  var $sel = SIMD_Float32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Float32x4_swizzle($a, 0, 3, 1, 2);
; CHECK:  return (SIMD_Float32x4_check($sel));
; CHECK: }
define <4 x float> @swizzle_float32x4(<4 x float> %a, <4 x float> %b) nounwind {
entry:
  %sel = shufflevector <4 x float> %a, <4 x float> %b, <4 x i32><i32 0, i32 3, i32 1, i32 2>
  ret <4 x float> %sel
}

; CHECK: function _swizzlehi_float32x4($a,$b) {
; CHECK:  $a = SIMD_Float32x4_check($a);
; CHECK:  $b = SIMD_Float32x4_check($b);
; CHECK:  var $sel = SIMD_Float32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Float32x4_swizzle($b, 2, 1, 3, 0);
; CHECK:  return (SIMD_Float32x4_check($sel));
; CHECK: }
define <4 x float> @swizzlehi_float32x4(<4 x float> %a, <4 x float> %b) nounwind {
entry:
  %sel = shufflevector <4 x float> %a, <4 x float> %b, <4 x i32><i32 6, i32 5, i32 7, i32 4>
  ret <4 x float> %sel
}

; CHECK: function _shuffle_float32x4($a,$b) {
; CHECK:  $a = SIMD_Float32x4_check($a);
; CHECK:  $b = SIMD_Float32x4_check($b);
; CHECK:  var $sel = SIMD_Float32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Float32x4_shuffle($a, $b, 7, 0, 5, 3);
; CHECK:  return (SIMD_Float32x4_check($sel));
; CHECK: }
define <4 x float> @shuffle_float32x4(<4 x float> %a, <4 x float> %b) nounwind {
entry:
  %sel = shufflevector <4 x float> %a, <4 x float> %b, <4 x i32><i32 7, i32 0, i32 5, i32 3>
  ret <4 x float> %sel
}

; CHECK: function _shuffleXY_float32x4($a,$b) {
; CHECK:  $a = SIMD_Float32x4_check($a);
; CHECK:  $b = SIMD_Float32x4_check($b);
; CHECK:  var $sel = SIMD_Float32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Float32x4_shuffle($a, $b, 7, 0, 0, 0);
; CHECK:  return (SIMD_Float32x4_check($sel));
; CHECK: }
define <4 x float> @shuffleXY_float32x4(<4 x float> %a, <4 x float> %b) nounwind {
entry:
  %sel = shufflevector <4 x float> %a, <4 x float> %b, <4 x i32><i32 7, i32 0, i32 undef, i32 undef>
  ret <4 x float> %sel
}

; CHECK: function _splat_float32x3($a,$b) {
; CHECK:  $a = SIMD_Float32x4_check($a);
; CHECK:  $b = SIMD_Float32x4_check($b);
; CHECK:  var $sel = SIMD_Float32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Float32x4_swizzle($a, 0, 0, 0, 0);
; CHECK:  return (SIMD_Float32x4_check($sel));
; CHECK: }
define <3 x float> @splat_float32x3(<3 x float> %a, <3 x float> %b) nounwind {
entry:
  %sel = shufflevector <3 x float> %a, <3 x float> %b, <3 x i32><i32 0, i32 0, i32 0>
  ret <3 x float> %sel
}

; CHECK: function _swizzle_float32x3($a,$b) {
; CHECK:  $a = SIMD_Float32x4_check($a);
; CHECK:  $b = SIMD_Float32x4_check($b);
; CHECK:  var $sel = SIMD_Float32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Float32x4_swizzle($a, 0, 2, 1, 0);
; CHECK:  return (SIMD_Float32x4_check($sel));
; CHECK: }
define <3 x float> @swizzle_float32x3(<3 x float> %a, <3 x float> %b) nounwind {
entry:
  %sel = shufflevector <3 x float> %a, <3 x float> %b, <3 x i32><i32 0, i32 2, i32 1>
  ret <3 x float> %sel
}

; CHECK: function _swizzlehi_float32x3($a,$b) {
; CHECK:  $a = SIMD_Float32x4_check($a);
; CHECK:  $b = SIMD_Float32x4_check($b);
; CHECK:  var $sel = SIMD_Float32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Float32x4_swizzle($b, 0, 2, 1, 0);
; CHECK:  return (SIMD_Float32x4_check($sel));
; CHECK: }
define <3 x float> @swizzlehi_float32x3(<3 x float> %a, <3 x float> %b) nounwind {
entry:
  %sel = shufflevector <3 x float> %a, <3 x float> %b, <3 x i32><i32 3, i32 5, i32 4>
  ret <3 x float> %sel
}

; CHECK: function _shuffle_float32x3($a,$b) {
; CHECK:  $a = SIMD_Float32x4_check($a);
; CHECK:  $b = SIMD_Float32x4_check($b);
; CHECK:  var $sel = SIMD_Float32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Float32x4_shuffle($a, $b, 6, 0, 5, 0);
; CHECK:  return (SIMD_Float32x4_check($sel));
; CHECK: }
define <3 x float> @shuffle_float32x3(<3 x float> %a, <3 x float> %b) nounwind {
entry:
  %sel = shufflevector <3 x float> %a, <3 x float> %b, <3 x i32><i32 5, i32 0, i32 4>
  ret <3 x float> %sel
}

; CHECK: function _shuffleXY_float32x3($a,$b) {
; CHECK:  $a = SIMD_Float32x4_check($a);
; CHECK:  $b = SIMD_Float32x4_check($b);
; CHECK:  var $sel = SIMD_Float32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Float32x4_shuffle($a, $b, 6, 0, 0, 0);
; CHECK:  return (SIMD_Float32x4_check($sel));
; CHECK: }
define <3 x float> @shuffleXY_float32x3(<3 x float> %a, <3 x float> %b) nounwind {
entry:
  %sel = shufflevector <3 x float> %a, <3 x float> %b, <3 x i32><i32 5, i32 0, i32 undef>
  ret <3 x float> %sel
}

; CHECK: function _splat_float32x3to4($a,$b) {
; CHECK:  $a = SIMD_Float32x4_check($a);
; CHECK:  $b = SIMD_Float32x4_check($b);
; CHECK:  var $sel = SIMD_Float32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Float32x4_swizzle($a, 0, 0, 0, 0);
; CHECK:  return (SIMD_Float32x4_check($sel));
; CHECK: }
define <4 x float> @splat_float32x3to4(<3 x float> %a, <3 x float> %b) nounwind {
entry:
  %sel = shufflevector <3 x float> %a, <3 x float> %b, <4 x i32><i32 0, i32 0, i32 0, i32 0>
  ret <4 x float> %sel
}

; CHECK: function _swizzle_float32x3to4($a,$b) {
; CHECK:  $a = SIMD_Float32x4_check($a);
; CHECK:  $b = SIMD_Float32x4_check($b);
; CHECK:  var $sel = SIMD_Float32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Float32x4_swizzle($a, 0, 2, 1, 2);
; CHECK:  return (SIMD_Float32x4_check($sel));
; CHECK: }
define <4 x float> @swizzle_float32x3to4(<3 x float> %a, <3 x float> %b) nounwind {
entry:
  %sel = shufflevector <3 x float> %a, <3 x float> %b, <4 x i32><i32 0, i32 2, i32 1, i32 2>
  ret <4 x float> %sel
}

; CHECK: function _swizzlehi_float32x3to4($a,$b) {
; CHECK:  $a = SIMD_Float32x4_check($a);
; CHECK:  $b = SIMD_Float32x4_check($b);
; CHECK:  var $sel = SIMD_Float32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Float32x4_swizzle($b, 2, 1, 0, 2);
; CHECK:  return (SIMD_Float32x4_check($sel));
; CHECK: }
define <4 x float> @swizzlehi_float32x3to4(<3 x float> %a, <3 x float> %b) nounwind {
entry:
  %sel = shufflevector <3 x float> %a, <3 x float> %b, <4 x i32><i32 5, i32 4, i32 3, i32 5>
  ret <4 x float> %sel
}

; CHECK: function _shuffle_float32x3to4($a,$b) {
; CHECK:  $a = SIMD_Float32x4_check($a);
; CHECK:  $b = SIMD_Float32x4_check($b);
; CHECK:  var $sel = SIMD_Float32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Float32x4_shuffle($a, $b, 6, 0, 5, 2);
; CHECK:  return (SIMD_Float32x4_check($sel));
; CHECK: }
define <4 x float> @shuffle_float32x3to4(<3 x float> %a, <3 x float> %b) nounwind {
entry:
  %sel = shufflevector <3 x float> %a, <3 x float> %b, <4 x i32><i32 5, i32 0, i32 4, i32 2>
  ret <4 x float> %sel
}

; CHECK: function _shuffleXY_float32x3to4($a,$b) {
; CHECK:  $a = SIMD_Float32x4_check($a);
; CHECK:  $b = SIMD_Float32x4_check($b);
; CHECK:  var $sel = SIMD_Float32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Float32x4_shuffle($a, $b, 6, 0, 0, 0);
; CHECK:  return (SIMD_Float32x4_check($sel));
; CHECK: }
define <4 x float> @shuffleXY_float32x3to4(<3 x float> %a, <3 x float> %b) nounwind {
entry:
  %sel = shufflevector <3 x float> %a, <3 x float> %b, <4 x i32><i32 5, i32 0, i32 undef, i32 undef>
  ret <4 x float> %sel
}

; CHECK: function _splat_float32x4to3($a,$b) {
; CHECK:  $a = SIMD_Float32x4_check($a);
; CHECK:  $b = SIMD_Float32x4_check($b);
; CHECK:  var $sel = SIMD_Float32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Float32x4_swizzle($a, 0, 0, 0, 0);
; CHECK:  return (SIMD_Float32x4_check($sel));
; CHECK: }
define <3 x float> @splat_float32x4to3(<4 x float> %a, <4 x float> %b) nounwind {
entry:
  %sel = shufflevector <4 x float> %a, <4 x float> %b, <3 x i32><i32 0, i32 0, i32 0>
  ret <3 x float> %sel
}

; CHECK: function _swizzle_float32x4to3($a,$b) {
; CHECK:  $a = SIMD_Float32x4_check($a);
; CHECK:  $b = SIMD_Float32x4_check($b);
; CHECK:  var $sel = SIMD_Float32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Float32x4_swizzle($a, 0, 3, 1, 0);
; CHECK:  return (SIMD_Float32x4_check($sel));
; CHECK: }
define <3 x float> @swizzle_float32x4to3(<4 x float> %a, <4 x float> %b) nounwind {
entry:
  %sel = shufflevector <4 x float> %a, <4 x float> %b, <3 x i32><i32 0, i32 3, i32 1>
  ret <3 x float> %sel
}

; CHECK: function _swizzlehi_float32x4to3($a,$b) {
; CHECK:  $a = SIMD_Float32x4_check($a);
; CHECK:  $b = SIMD_Float32x4_check($b);
; CHECK:  var $sel = SIMD_Float32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Float32x4_swizzle($b, 2, 1, 3, 0);
; CHECK:  return (SIMD_Float32x4_check($sel));
; CHECK: }
define <3 x float> @swizzlehi_float32x4to3(<4 x float> %a, <4 x float> %b) nounwind {
entry:
  %sel = shufflevector <4 x float> %a, <4 x float> %b, <3 x i32><i32 6, i32 5, i32 7>
  ret <3 x float> %sel
}

; CHECK: function _shuffle_float32x4to3($a,$b) {
; CHECK:  $a = SIMD_Float32x4_check($a);
; CHECK:  $b = SIMD_Float32x4_check($b);
; CHECK:  var $sel = SIMD_Float32x4(0,0,0,0)
; CHECK:  $sel = SIMD_Float32x4_shuffle($a, $b, 7, 0, 5, 0);
; CHECK:  return (SIMD_Float32x4_check($sel));
; CHECK: }
define <3 x float> @shuffle_float32x4to3(<4 x float> %a, <4 x float> %b) nounwind {
entry:
  %sel = shufflevector <4 x float> %a, <4 x float> %b, <3 x i32><i32 7, i32 0, i32 5>
  ret <3 x float> %sel
}
