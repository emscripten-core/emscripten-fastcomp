; RUN: llc < %s | FileCheck %s

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:128-n32-S128"
target triple = "asmjs-unknown-emscripten"

; CHECK:  sp = STACKTOP;
; CHECK:  STACKTOP = STACKTOP + 16|0;
; CHECK:  $0 = sp;
; CHECK:  SIMD_Float32x4_store(HEAPU8, temp_Float32x4_ptr, $p);
; CHECK:  $1 = (($0) + ($i<<2)|0);
; CHECK:  $2 = +HEAPF32[$1>>2];
; CHECK:  STACKTOP = sp;return (+$2);
; CHECK: }
define float @ext(<4 x float> %p, i32 %i) {
  %f = extractelement <4 x float> %p, i32 %i
  ret float %f
}

; CHECK:  sp = STACKTOP;
; CHECK:  STACKTOP = STACKTOP + 16|0;
; CHECK:  $0 = sp;
; CHECK:  SIMD_Float32x4_store(HEAPU8, temp_Float32x4_ptr, $p);
; CHECK:  $1 = (($0) + ($i<<2)|0);
; CHECK:  HEAPF32[$1>>2] = $f;
; CHECK:  $2 = SIMD_Float32x4_load(HEAPU8, $0);
; CHECK:  STACKTOP = sp;return (SIMD_Float32x4_check($2));
; CHECK: }
define <4 x float> @ins(<4 x float> %p, float %f, i32 %i) {
  %v = insertelement <4 x float> %p, float %f, i32 %i
  ret <4 x float> %v
}
