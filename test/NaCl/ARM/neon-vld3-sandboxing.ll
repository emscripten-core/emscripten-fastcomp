; RUN: llc -mtriple=armv7-unknown-nacl -mattr=+neon -sfi-store -sfi-load -filetype=obj %s -o - \
; RUN:  | llvm-objdump -disassemble -triple armv7 - | FileCheck %s

%struct.__neon_int8x8x3_t = type { <8 x i8>,  <8 x i8>,  <8 x i8> }
%struct.__neon_int16x4x3_t = type { <4 x i16>, <4 x i16>, <4 x i16> }
%struct.__neon_int32x2x3_t = type { <2 x i32>, <2 x i32>, <2 x i32> }
%struct.__neon_float32x2x3_t = type { <2 x float>, <2 x float>, <2 x float> }
%struct.__neon_int64x1x3_t = type { <1 x i64>, <1 x i64>, <1 x i64> }

%struct.__neon_int8x16x3_t = type { <16 x i8>,  <16 x i8>,  <16 x i8> }
%struct.__neon_int16x8x3_t = type { <8 x i16>, <8 x i16>, <8 x i16> }
%struct.__neon_int32x4x3_t = type { <4 x i32>, <4 x i32>, <4 x i32> }
%struct.__neon_float32x4x3_t = type { <4 x float>, <4 x float>, <4 x float> }

declare %struct.__neon_int8x8x3_t @llvm.arm.neon.vld3.v8i8(i8*, i32) nounwind readonly
declare %struct.__neon_int16x4x3_t @llvm.arm.neon.vld3.v4i16(i8*, i32) nounwind readonly
declare %struct.__neon_int32x2x3_t @llvm.arm.neon.vld3.v2i32(i8*, i32) nounwind readonly
declare %struct.__neon_float32x2x3_t @llvm.arm.neon.vld3.v2f32(i8*, i32) nounwind readonly
declare %struct.__neon_int64x1x3_t @llvm.arm.neon.vld3.v1i64(i8*, i32) nounwind readonly

declare %struct.__neon_int8x16x3_t @llvm.arm.neon.vld3.v16i8(i8*, i32) nounwind readonly
declare %struct.__neon_int16x8x3_t @llvm.arm.neon.vld3.v8i16(i8*, i32) nounwind readonly
declare %struct.__neon_int32x4x3_t @llvm.arm.neon.vld3.v4i32(i8*, i32) nounwind readonly
declare %struct.__neon_float32x4x3_t @llvm.arm.neon.vld3.v4f32(i8*, i32) nounwind readonly

define <8 x i8> @vld3i8(i32 %foobar, i32 %ba, i8* %A) nounwind {
  %tmp1 = call %struct.__neon_int8x8x3_t @llvm.arm.neon.vld3.v8i8(i8* %A, i32 32)
  %tmp2 = extractvalue %struct.__neon_int8x8x3_t %tmp1, 0
  %tmp3 = extractvalue %struct.__neon_int8x8x3_t %tmp1, 2
  %tmp4 = add <8 x i8> %tmp2, %tmp3
; CHECK:         bic r2, r2, #3221225472
; CHECK-NEXT:    vld3.8 {{{d[0-9]+}}, {{d[0-9]+}}, {{d[0-9]+}}}, [r2:64]
  ret <8 x i8> %tmp4
}

define <4 x i16> @vld3i16(i16* %A) nounwind {
  %tmp0 = bitcast i16* %A to i8*
  %tmp1 = call %struct.__neon_int16x4x3_t @llvm.arm.neon.vld3.v4i16(i8* %tmp0, i32 1)
  %tmp2 = extractvalue %struct.__neon_int16x4x3_t %tmp1, 0
  %tmp3 = extractvalue %struct.__neon_int16x4x3_t %tmp1, 2
  %tmp4 = add <4 x i16> %tmp2, %tmp3
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vld3.16 {{{d[0-9]+}}, {{d[0-9]+}}, {{d[0-9]+}}}, [r0]
  ret <4 x i16> %tmp4
}

define <2 x i32> @vld3i32(i32* %A) nounwind {
  %tmp0 = bitcast i32* %A to i8*
  %tmp1 = call %struct.__neon_int32x2x3_t @llvm.arm.neon.vld3.v2i32(i8* %tmp0, i32 1)
  %tmp2 = extractvalue %struct.__neon_int32x2x3_t %tmp1, 0
  %tmp3 = extractvalue %struct.__neon_int32x2x3_t %tmp1, 2
  %tmp4 = add <2 x i32> %tmp2, %tmp3
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vld3.32 {{{d[0-9]+}}, {{d[0-9]+}}, {{d[0-9]+}}}, [r0]
  ret <2 x i32> %tmp4
}

define <1 x i64> @vld3i64(i64* %A) nounwind {
  %tmp0 = bitcast i64* %A to i8*
  %tmp1 = call %struct.__neon_int64x1x3_t @llvm.arm.neon.vld3.v1i64(i8* %tmp0, i32 16)
  %tmp2 = extractvalue %struct.__neon_int64x1x3_t %tmp1, 0
  %tmp3 = extractvalue %struct.__neon_int64x1x3_t %tmp1, 2
  %tmp4 = add <1 x i64> %tmp2, %tmp3
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vld1.64 {{{d[0-9]+}}, {{d[0-9]+}}, {{d[0-9]+}}}, [r0:64]
  ret <1 x i64> %tmp4
}

define <16 x i8> @vld3Qi8(i8* %A) nounwind {
  %tmp1 = call %struct.__neon_int8x16x3_t @llvm.arm.neon.vld3.v16i8(i8* %A, i32 32)
  %tmp2 = extractvalue %struct.__neon_int8x16x3_t %tmp1, 0
  %tmp3 = extractvalue %struct.__neon_int8x16x3_t %tmp1, 2
  %tmp4 = add <16 x i8> %tmp2, %tmp3
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vld3.8 {{{d[0-9]+}}, {{d[0-9]+}}, {{d[0-9]+}}}, [r0:64]!
  ret <16 x i8> %tmp4
}

define <4 x i16> @vld3i16_update(i16** %ptr, i32 %inc) nounwind {
  %A = load i16** %ptr
  %tmp0 = bitcast i16* %A to i8*
  %tmp1 = call %struct.__neon_int16x4x3_t @llvm.arm.neon.vld3.v4i16(i8* %tmp0, i32 1)
; CHECK:         bic r2, r2, #3221225472
; CHECK-NEXT:    vld3.16  {{{d[0-9]+}}, {{d[0-9]+}}, {{d[0-9]+}}}, [r2], r1
  %tmp2 = extractvalue %struct.__neon_int16x4x3_t %tmp1, 0
  %tmp3 = extractvalue %struct.__neon_int16x4x3_t %tmp1, 2
  %tmp4 = add <4 x i16> %tmp2, %tmp3
  %tmp5 = getelementptr i16* %A, i32 %inc
  store i16* %tmp5, i16** %ptr
  ret <4 x i16> %tmp4
}

define <4 x i32> @vld3Qi32_update(i32** %ptr) nounwind {
  %A = load i32** %ptr
  %tmp0 = bitcast i32* %A to i8*
  %tmp1 = call %struct.__neon_int32x4x3_t @llvm.arm.neon.vld3.v4i32(i8* %tmp0, i32 1)
; CHECK:         bic r1, r1, #3221225472
; CHECK-NEXT:    vld3.32  {{{d[0-9]+}}, {{d[0-9]+}}, {{d[0-9]+}}}, [r1]!
  %tmp2 = extractvalue %struct.__neon_int32x4x3_t %tmp1, 0
  %tmp3 = extractvalue %struct.__neon_int32x4x3_t %tmp1, 2
  %tmp4 = add <4 x i32> %tmp2, %tmp3
  %tmp5 = getelementptr i32* %A, i32 12
  store i32* %tmp5, i32** %ptr
  ret <4 x i32> %tmp4
}

