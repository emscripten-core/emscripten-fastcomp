; RUN: llc < %s | FileCheck %s

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

; CHECK: SIMD_int32x4(($a.x|0) / ($b.x|0)|0, ($a.y|0) / ($b.y|0)|0, ($a.z|0) / ($b.z|0)|0, ($a.w|0) / ($b.w|0)|0);
define <4 x i32> @signed_div(<4 x i32> %a, <4 x i32> %b) {
  %c = sdiv <4 x i32> %a, %b
  ret <4 x i32> %c
}

; CHECK: SIMD_int32x4(($a.x>>>0) / ($b.x>>>0)>>>0, ($a.y>>>0) / ($b.y>>>0)>>>0, ($a.z>>>0) / ($b.z>>>0)>>>0, ($a.w>>>0) / ($b.w>>>0)>>>0);
define <4 x i32> @un_div(<4 x i32> %a, <4 x i32> %b) {
  %c = udiv <4 x i32> %a, %b
  ret <4 x i32> %c
}

; CHECK: SIMD_int32x4(($a.x|0) / ($b.x|0)|0, ($a.y|0) / ($b.y|0)|0, ($a.z|0) / ($b.z|0)|0, ($a.w|0) / ($b.w|0)|0);
define <4 x i32> @signed_rem(<4 x i32> %a, <4 x i32> %b) {
  %c = srem <4 x i32> %a, %b
  ret <4 x i32> %c
}

; CHECK: SIMD_int32x4(($a.x>>>0) / ($b.x>>>0)>>>0, ($a.y>>>0) / ($b.y>>>0)>>>0, ($a.z>>>0) / ($b.z>>>0)>>>0, ($a.w>>>0) / ($b.w>>>0)>>>0);
define <4 x i32> @un_rem(<4 x i32> %a, <4 x i32> %b) {
  %c = urem <4 x i32> %a, %b
  ret <4 x i32> %c
}
