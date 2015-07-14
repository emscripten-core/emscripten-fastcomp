; RUN: llc < %s | FileCheck %s

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

; CHECK: function _test0($a) {
; CHECK:  $a = SIMD_Int32x4_check($a);
; CHECK:  $shl = SIMD_Int32x4_shiftLeftByScalar($a, 3);
; CHECK:  return (SIMD_Int32x4_check($shl));
; CHECK: }
define <4 x i32> @test0(<4 x i32> %a) {
entry:
  %shl = shl <4 x i32> %a, <i32 3, i32 3, i32 3, i32 3>
  ret <4 x i32> %shl
}

; CHECK: function _test1($a,$b) {
; CHECK:  $a = SIMD_Int32x4_check($a);
; CHECK:  $b = $b|0;
; CHECK:  SIMD_Int32x4_shiftLeftByScalar($a, $b);
; CHECK:  return (SIMD_Int32x4_check($shl));
; CHECK: }
define <4 x i32> @test1(<4 x i32> %a, i32 %b) {
entry:
  %vecinit = insertelement <4 x i32> undef, i32 %b, i32 0
  %vecinit1 = insertelement <4 x i32> %vecinit, i32 %b, i32 1
  %vecinit2 = insertelement <4 x i32> %vecinit1, i32 %b, i32 2
  %vecinit3 = insertelement <4 x i32> %vecinit2, i32 %b, i32 3
  %shl = shl <4 x i32> %a, %vecinit3
  ret <4 x i32> %shl
}

; CHECK: function _test2($a,$b,$c) {
; CHECK:  $a = SIMD_Int32x4_check($a);
; CHECK:  $b = $b|0;
; CHECK:  $c = $c|0;
; CHECK:  var $shl = SIMD_Int32x4(0,0,0,0), $vecinit3 = SIMD_Int32x4(0,0,0,0), label = 0, sp = 0;
; CHECK:  $vecinit3 = SIMD_Int32x4($b, $b, $c, $b);
; CHECK:  $shl = SIMD_Int32x4((SIMD_Int32x4_extractLane($a,0)|0) << (SIMD_Int32x4_extractLane($vecinit3,0)|0)|0, (SIMD_Int32x4_extractLane($a,1)|0) << (SIMD_Int32x4_extractLane($vecinit3,1)|0)|0, (SIMD_Int32x4_extractLane($a,2)|0) << (SIMD_Int32x4_extractLane($vecinit3,2)|0)|0, (SIMD_Int32x4_extractLane($a,3)|0) << (SIMD_Int32x4_extractLane($vecinit3,3)|0)|0);
; CHECK:  return (SIMD_Int32x4_check($shl));
; CHECK: }
define <4 x i32> @test2(<4 x i32> %a, i32 %b, i32 %c) {
entry:
  %vecinit = insertelement <4 x i32> undef, i32 %b, i32 0
  %vecinit1 = insertelement <4 x i32> %vecinit, i32 %b, i32 1
  %vecinit2 = insertelement <4 x i32> %vecinit1, i32 %c, i32 2
  %vecinit3 = insertelement <4 x i32> %vecinit2, i32 %b, i32 3
  %shl = shl <4 x i32> %a, %vecinit3
  ret <4 x i32> %shl
}

; CHECK: function _test3($a) {
; CHECK:  $a = SIMD_Int32x4_check($a);
; CHECK:  SIMD_Int32x4_shiftRightArithmeticByScalar($a, 3);
; CHECK:  return (SIMD_Int32x4_check($shr));
; CHECK: }
define <4 x i32> @test3(<4 x i32> %a) {
entry:
  %shr = ashr <4 x i32> %a, <i32 3, i32 3, i32 3, i32 3>
  ret <4 x i32> %shr
}

; CHECK: function _test4($a,$b) {
; CHECK:  $a = SIMD_Int32x4_check($a);
; CHECK:  $b = $b|0;
; CHECK:  SIMD_Int32x4_shiftRightArithmeticByScalar($a, $b);
; CHECK:  return (SIMD_Int32x4_check($shr));
; CHECK: }
define <4 x i32> @test4(<4 x i32> %a, i32 %b) {
entry:
  %vecinit = insertelement <4 x i32> undef, i32 %b, i32 0
  %vecinit1 = insertelement <4 x i32> %vecinit, i32 %b, i32 1
  %vecinit2 = insertelement <4 x i32> %vecinit1, i32 %b, i32 2
  %vecinit3 = insertelement <4 x i32> %vecinit2, i32 %b, i32 3
  %shr = ashr <4 x i32> %a, %vecinit3
  ret <4 x i32> %shr
}

; CHECK: function _test5($a,$b,$c) {
; CHECK:  $a = SIMD_Int32x4_check($a);
; CHECK:  $b = $b|0;
; CHECK:  $c = $c|0;
; CHECK:  var $shr = SIMD_Int32x4(0,0,0,0), $vecinit3 = SIMD_Int32x4(0,0,0,0), label = 0, sp = 0;
; CHECK:  $vecinit3 = SIMD_Int32x4($b, $c, $b, $b);
; CHECK:  $shr = SIMD_Int32x4((SIMD_Int32x4_extractLane($a,0)|0) >> (SIMD_Int32x4_extractLane($vecinit3,0)|0)|0, (SIMD_Int32x4_extractLane($a,1)|0) >> (SIMD_Int32x4_extractLane($vecinit3,1)|0)|0, (SIMD_Int32x4_extractLane($a,2)|0) >> (SIMD_Int32x4_extractLane($vecinit3,2)|0)|0, (SIMD_Int32x4_extractLane($a,3)|0) >> (SIMD_Int32x4_extractLane($vecinit3,3)|0)|0);
; CHECK:  return (SIMD_Int32x4_check($shr));
; CHECK: }
define <4 x i32> @test5(<4 x i32> %a, i32 %b, i32 %c) {
entry:
  %vecinit = insertelement <4 x i32> undef, i32 %b, i32 0
  %vecinit1 = insertelement <4 x i32> %vecinit, i32 %c, i32 1
  %vecinit2 = insertelement <4 x i32> %vecinit1, i32 %b, i32 2
  %vecinit3 = insertelement <4 x i32> %vecinit2, i32 %b, i32 3
  %shr = ashr <4 x i32> %a, %vecinit3
  ret <4 x i32> %shr
}

; CHECK: function _test6($a) {
; CHECK:  $a = SIMD_Int32x4_check($a);
; CHECK:  SIMD_Int32x4_shiftRightLogicalByScalar($a, 3);
; CHECK:  return (SIMD_Int32x4_check($lshr));
; CHECK: }
define <4 x i32> @test6(<4 x i32> %a) {
entry:
  %lshr = lshr <4 x i32> %a, <i32 3, i32 3, i32 3, i32 3>
  ret <4 x i32> %lshr
}

; CHECK: function _test7($a,$b) {
; CHECK:  $a = SIMD_Int32x4_check($a);
; CHECK:  $b = $b|0;
; CHECK:  $lshr = SIMD_Int32x4_shiftRightLogicalByScalar($a, $b);
; CHECK:  return (SIMD_Int32x4_check($lshr));
; CHECK: }
define <4 x i32> @test7(<4 x i32> %a, i32 %b) {
entry:
  %vecinit = insertelement <4 x i32> undef, i32 %b, i32 0
  %vecinit1 = insertelement <4 x i32> %vecinit, i32 %b, i32 1
  %vecinit2 = insertelement <4 x i32> %vecinit1, i32 %b, i32 2
  %vecinit3 = insertelement <4 x i32> %vecinit2, i32 %b, i32 3
  %lshr = lshr <4 x i32> %a, %vecinit3
  ret <4 x i32> %lshr
}

; CHECK: function _test8($a,$b,$c) {
; CHECK:  $a = SIMD_Int32x4_check($a);
; CHECK:  $b = $b|0;
; CHECK:  $c = $c|0;
; CHECK:  var $lshr = SIMD_Int32x4(0,0,0,0), $vecinit3 = SIMD_Int32x4(0,0,0,0), label = 0, sp = 0;
; CHECK:  $vecinit3 = SIMD_Int32x4($b, $b, $b, $c);
; CHECK:  $lshr = SIMD_Int32x4((SIMD_Int32x4_extractLane($a,0)|0) >>> (SIMD_Int32x4_extractLane($vecinit3,0)|0)|0, (SIMD_Int32x4_extractLane($a,1)|0) >>> (SIMD_Int32x4_extractLane($vecinit3,1)|0)|0, (SIMD_Int32x4_extractLane($a,2)|0) >>> (SIMD_Int32x4_extractLane($vecinit3,2)|0)|0, (SIMD_Int32x4_extractLane($a,3)|0) >>> (SIMD_Int32x4_extractLane($vecinit3,3)|0)|0);
; CHECK:  return (SIMD_Int32x4_check($lshr));
; CHECK: }
define <4 x i32> @test8(<4 x i32> %a, i32 %b, i32 %c) {
entry:
  %vecinit = insertelement <4 x i32> undef, i32 %b, i32 0
  %vecinit1 = insertelement <4 x i32> %vecinit, i32 %b, i32 1
  %vecinit2 = insertelement <4 x i32> %vecinit1, i32 %b, i32 2
  %vecinit3 = insertelement <4 x i32> %vecinit2, i32 %c, i32 3
  %lshr = lshr <4 x i32> %a, %vecinit3
  ret <4 x i32> %lshr
}
