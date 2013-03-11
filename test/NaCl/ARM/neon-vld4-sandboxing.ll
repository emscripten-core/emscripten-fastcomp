; RUN: llc -mtriple=armv7-unknown-nacl -mattr=+neon -sfi-store -sfi-load -filetype=obj %s -o - \
; RUN:  | llvm-objdump -disassemble -triple armv7 - | FileCheck %s

%struct.__neon_int8x8x4_t = type { <8 x i8>,  <8 x i8>,  <8 x i8>, <8 x i8> }
%struct.__neon_int16x4x4_t = type { <4 x i16>, <4 x i16>, <4 x i16>, <4 x i16> }
%struct.__neon_int32x2x4_t = type { <2 x i32>, <2 x i32>, <2 x i32>, <2 x i32> }
%struct.__neon_float32x2x4_t = type { <2 x float>, <2 x float>, <2 x float>, <2 x float> }
%struct.__neon_int64x1x4_t = type { <1 x i64>, <1 x i64>, <1 x i64>, <1 x i64> }

%struct.__neon_int8x16x4_t = type { <16 x i8>,  <16 x i8>,  <16 x i8>, <16 x i8> }
%struct.__neon_int16x8x4_t = type { <8 x i16>, <8 x i16>, <8 x i16>, <8 x i16> }
%struct.__neon_int32x4x4_t = type { <4 x i32>, <4 x i32>, <4 x i32>, <4 x i32> }
%struct.__neon_float32x4x4_t = type { <4 x float>, <4 x float>, <4 x float>, <4 x float> }

declare %struct.__neon_int8x8x4_t @llvm.arm.neon.vld4.v8i8(i8*, i32) nounwind readonly
declare %struct.__neon_int16x4x4_t @llvm.arm.neon.vld4.v4i16(i8*, i32) nounwind readonly
declare %struct.__neon_int32x2x4_t @llvm.arm.neon.vld4.v2i32(i8*, i32) nounwind readonly
declare %struct.__neon_float32x2x4_t @llvm.arm.neon.vld4.v2f32(i8*, i32) nounwind readonly
declare %struct.__neon_int64x1x4_t @llvm.arm.neon.vld4.v1i64(i8*, i32) nounwind readonly

declare %struct.__neon_int8x16x4_t @llvm.arm.neon.vld4.v16i8(i8*, i32) nounwind readonly
declare %struct.__neon_int16x8x4_t @llvm.arm.neon.vld4.v8i16(i8*, i32) nounwind readonly
declare %struct.__neon_int32x4x4_t @llvm.arm.neon.vld4.v4i32(i8*, i32) nounwind readonly
declare %struct.__neon_float32x4x4_t @llvm.arm.neon.vld4.v4f32(i8*, i32) nounwind readonly

define <8 x i8> @vld4i8(i8* %A) nounwind {
  %tmp1 = call %struct.__neon_int8x8x4_t @llvm.arm.neon.vld4.v8i8(i8* %A, i32 8)
  %tmp2 = extractvalue %struct.__neon_int8x8x4_t %tmp1, 0
  %tmp3 = extractvalue %struct.__neon_int8x8x4_t %tmp1, 2
  %tmp4 = add <8 x i8> %tmp2, %tmp3
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vld4.8 {{{d[0-9]+}}, {{d[0-9]+}}, {{d[0-9]+}}, {{d[0-9]+}}}, [r0:64]
  ret <8 x i8> %tmp4
}

define <4 x i16> @vld4i16(i16* %A) nounwind {
  %tmp0 = bitcast i16* %A to i8*
  %tmp1 = call %struct.__neon_int16x4x4_t @llvm.arm.neon.vld4.v4i16(i8* %tmp0, i32 16)
  %tmp2 = extractvalue %struct.__neon_int16x4x4_t %tmp1, 0
  %tmp3 = extractvalue %struct.__neon_int16x4x4_t %tmp1, 2
  %tmp4 = add <4 x i16> %tmp2, %tmp3
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vld4.16 {{{d[0-9]+}}, {{d[0-9]+}}, {{d[0-9]+}}, {{d[0-9]+}}}, [r0:128]
  ret <4 x i16> %tmp4
}

define <2 x i32> @vld4i32(i32* %A) nounwind {
  %tmp0 = bitcast i32* %A to i8*
  %tmp1 = call %struct.__neon_int32x2x4_t @llvm.arm.neon.vld4.v2i32(i8* %tmp0, i32 32)
  %tmp2 = extractvalue %struct.__neon_int32x2x4_t %tmp1, 0
  %tmp3 = extractvalue %struct.__neon_int32x2x4_t %tmp1, 2
  %tmp4 = add <2 x i32> %tmp2, %tmp3
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vld4.32 {{{d[0-9]+}}, {{d[0-9]+}}, {{d[0-9]+}}, {{d[0-9]+}}}, [r0:256]
  ret <2 x i32> %tmp4
}

define <1 x i64> @vld4i64(i64* %A) nounwind {
  %tmp0 = bitcast i64* %A to i8*
  %tmp1 = call %struct.__neon_int64x1x4_t @llvm.arm.neon.vld4.v1i64(i8* %tmp0, i32 64)
  %tmp2 = extractvalue %struct.__neon_int64x1x4_t %tmp1, 0
  %tmp3 = extractvalue %struct.__neon_int64x1x4_t %tmp1, 2
  %tmp4 = add <1 x i64> %tmp2, %tmp3
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vld1.64 {{{d[0-9]+}}, {{d[0-9]+}}, {{d[0-9]+}}, {{d[0-9]+}}}, [r0:256]
  ret <1 x i64> %tmp4
}

define <16 x i8> @vld4Qi8(i8* %A) nounwind {
  %tmp1 = call %struct.__neon_int8x16x4_t @llvm.arm.neon.vld4.v16i8(i8* %A, i32 64)
  %tmp2 = extractvalue %struct.__neon_int8x16x4_t %tmp1, 0
  %tmp3 = extractvalue %struct.__neon_int8x16x4_t %tmp1, 2
  %tmp4 = add <16 x i8> %tmp2, %tmp3
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vld4.8 {{{d[0-9]+}}, {{d[0-9]+}}, {{d[0-9]+}}, {{d[0-9]+}}}, [r0:256]!
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vld4.8 {{{d[0-9]+}}, {{d[0-9]+}}, {{d[0-9]+}}, {{d[0-9]+}}}, [r0:256]
  ret <16 x i8> %tmp4
}

define <8 x i8> @vld4i8_update(i8** %ptr, i32 %inc) nounwind {
  %A = load i8** %ptr
  %tmp1 = call %struct.__neon_int8x8x4_t @llvm.arm.neon.vld4.v8i8(i8* %A, i32 16)
; CHECK:         bic r2, r2, #3221225472
; CHECK-NEXT:    vld4.8 {{{d[0-9]+}}, {{d[0-9]+}}, {{d[0-9]+}}, {{d[0-9]+}}}, [r2:128], r1
  %tmp2 = extractvalue %struct.__neon_int8x8x4_t %tmp1, 0
  %tmp3 = extractvalue %struct.__neon_int8x8x4_t %tmp1, 2
  %tmp4 = add <8 x i8> %tmp2, %tmp3
  %tmp5 = getelementptr i8* %A, i32 %inc
  store i8* %tmp5, i8** %ptr
  ret <8 x i8> %tmp4
}

define <8 x i16> @vld4Qi16_update(i16** %ptr) nounwind {
  %A = load i16** %ptr
  %tmp0 = bitcast i16* %A to i8*
  %tmp1 = call %struct.__neon_int16x8x4_t @llvm.arm.neon.vld4.v8i16(i8* %tmp0, i32 8)
; CHECK:         bic r1, r1, #3221225472
; CHECK-NEXT:    vld4.16 {{{d[0-9]+}}, {{d[0-9]+}}, {{d[0-9]+}}, {{d[0-9]+}}}, [r1:64]!
  %tmp2 = extractvalue %struct.__neon_int16x8x4_t %tmp1, 0
  %tmp3 = extractvalue %struct.__neon_int16x8x4_t %tmp1, 2
  %tmp4 = add <8 x i16> %tmp2, %tmp3
  %tmp5 = getelementptr i16* %A, i32 32
  store i16* %tmp5, i16** %ptr
  ret <8 x i16> %tmp4
}

