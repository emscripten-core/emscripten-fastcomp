; RUN: pnacl-llc -mtriple=armv7-unknown-nacl -mattr=+neon -sfi-store -sfi-load -filetype=obj %s -o - \
; RUN:  | llvm-objdump -disassemble -triple armv7 - | FileCheck %s

%struct.__neon_int8x8x2_t = type { <8 x i8>, <8 x i8> }
%struct.__neon_int4x16x2_t = type { <4 x i16>, <4 x i16> }
%struct.__neon_int2x32x2_t = type { <2 x i32>, <2 x i32> }

declare %struct.__neon_int8x8x2_t @llvm.arm.neon.vld2lane.v8i8(i8*, <8 x i8>, <8 x i8>, i32, i32) nounwind readonly
declare %struct.__neon_int4x16x2_t @llvm.arm.neon.vld2lane.v4i16(i8*, <4 x i16>, <4 x i16>, i32, i32) nounwind readonly
declare %struct.__neon_int2x32x2_t @llvm.arm.neon.vld2lane.v2i32(i8*, <2 x i32>, <2 x i32>, i32, i32) nounwind readonly

%struct.__neon_int8x8x3_t = type { <8 x i8>, <8 x i8>, <8 x i8> }
%struct.__neon_int16x4x3_t = type { <4 x i16>, <4 x i16>, <4 x i16> }

declare %struct.__neon_int8x8x3_t @llvm.arm.neon.vld3lane.v8i8(i8*, <8 x i8>, <8 x i8>, <8 x i8>, i32, i32) nounwind readonly
declare %struct.__neon_int16x4x3_t @llvm.arm.neon.vld3lane.v4i16(i8*, <4 x i16>, <4 x i16>, <4 x i16>, i32, i32) nounwind readonly

%struct.__neon_int16x4x4_t = type { <4 x i16>, <4 x i16>, <4 x i16>, <4 x i16> }
%struct.__neon_int32x2x4_t = type { <2 x i32>, <2 x i32>, <2 x i32>, <2 x i32> }

declare %struct.__neon_int16x4x4_t @llvm.arm.neon.vld4lane.v4i16(i8*, <4 x i16>, <4 x i16>, <4 x i16>, <4 x i16>, i32, i32) nounwind readonly
declare %struct.__neon_int32x2x4_t @llvm.arm.neon.vld4lane.v2i32(i8*, <2 x i32>, <2 x i32>, <2 x i32>, <2 x i32>, i32, i32) nounwind readonly

define <8 x i8> @vld1dupi8(i32 %foo, i32 %bar,
                           i8* %A) nounwind {
  %tmp1 = load i8* %A, align 8
  %tmp2 = insertelement <8 x i8> undef, i8 %tmp1, i32 0
  %tmp3 = shufflevector <8 x i8> %tmp2, <8 x i8> undef, <8 x i32> zeroinitializer
; CHECK:         bic r2, r2, #3221225472
; CHECK-NEXT:    vld1.8 {{{d[0-9]+\[\]}}}, [r2]
  ret <8 x i8> %tmp3
}

define <4 x i16> @vld1dupi16(i16* %A) nounwind {
  %tmp1 = load i16* %A, align 8
  %tmp2 = insertelement <4 x i16> undef, i16 %tmp1, i32 0
  %tmp3 = shufflevector <4 x i16> %tmp2, <4 x i16> undef, <4 x i32> zeroinitializer
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vld1.16 {{{d[0-9]+\[\]}}}, [r0:16]
  ret <4 x i16> %tmp3
}

define <2 x i32> @vld1dupi32(i32* %A) nounwind {
  %tmp1 = load i32* %A, align 8
  %tmp2 = insertelement <2 x i32> undef, i32 %tmp1, i32 0
  %tmp3 = shufflevector <2 x i32> %tmp2, <2 x i32> undef, <2 x i32> zeroinitializer
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vld1.32 {{{d[0-9]+\[\]}}}, [r0:32]
  ret <2 x i32> %tmp3
}

define <16 x i8> @vld1dupQi8(i8* %A) nounwind {
  %tmp1 = load i8* %A, align 8
  %tmp2 = insertelement <16 x i8> undef, i8 %tmp1, i32 0
  %tmp3 = shufflevector <16 x i8> %tmp2, <16 x i8> undef, <16 x i32> zeroinitializer
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vld1.8 {{{d[0-9]+\[\]}}, {{d[0-9]+\[\]}}}, [r0]
  ret <16 x i8> %tmp3
}

define <8 x i8> @vld2dupi8(i8* %A) nounwind {
  %tmp0 = tail call %struct.__neon_int8x8x2_t @llvm.arm.neon.vld2lane.v8i8(i8* %A, <8 x i8> undef, <8 x i8> undef, i32 0, i32 1)
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vld2.8 {{{d[0-9]+\[\]}}, {{d[0-9]+\[\]}}}, [r0]
  %tmp1 = extractvalue %struct.__neon_int8x8x2_t %tmp0, 0
  %tmp2 = shufflevector <8 x i8> %tmp1, <8 x i8> undef, <8 x i32> zeroinitializer
  %tmp3 = extractvalue %struct.__neon_int8x8x2_t %tmp0, 1
  %tmp4 = shufflevector <8 x i8> %tmp3, <8 x i8> undef, <8 x i32> zeroinitializer
  %tmp5 = add <8 x i8> %tmp2, %tmp4
  ret <8 x i8> %tmp5
}

define <4 x i16> @vld2dupi16(i8* %A) nounwind {
  %tmp0 = tail call %struct.__neon_int4x16x2_t @llvm.arm.neon.vld2lane.v4i16(i8* %A, <4 x i16> undef, <4 x i16> undef, i32 0, i32 2)
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vld2.16 {{{d[0-9]+\[\]}}, {{d[0-9]+\[\]}}}, [r0]
  %tmp1 = extractvalue %struct.__neon_int4x16x2_t %tmp0, 0
  %tmp2 = shufflevector <4 x i16> %tmp1, <4 x i16> undef, <4 x i32> zeroinitializer
  %tmp3 = extractvalue %struct.__neon_int4x16x2_t %tmp0, 1
  %tmp4 = shufflevector <4 x i16> %tmp3, <4 x i16> undef, <4 x i32> zeroinitializer
  %tmp5 = add <4 x i16> %tmp2, %tmp4
  ret <4 x i16> %tmp5
}

define <2 x i32> @vld2dupi32(i8* %A) nounwind {
  %tmp0 = tail call %struct.__neon_int2x32x2_t @llvm.arm.neon.vld2lane.v2i32(i8* %A, <2 x i32> undef, <2 x i32> undef, i32 0, i32 16)
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vld2.32 {{{d[0-9]+\[\]}}, {{d[0-9]+\[\]}}}, [r0:64]
  %tmp1 = extractvalue %struct.__neon_int2x32x2_t %tmp0, 0
  %tmp2 = shufflevector <2 x i32> %tmp1, <2 x i32> undef, <2 x i32> zeroinitializer
  %tmp3 = extractvalue %struct.__neon_int2x32x2_t %tmp0, 1
  %tmp4 = shufflevector <2 x i32> %tmp3, <2 x i32> undef, <2 x i32> zeroinitializer
  %tmp5 = add <2 x i32> %tmp2, %tmp4
  ret <2 x i32> %tmp5
}

define <4 x i16> @vld3dupi16(i8* %A) nounwind {
  %tmp0 = tail call %struct.__neon_int16x4x3_t @llvm.arm.neon.vld3lane.v4i16(i8* %A, <4 x i16> undef, <4 x i16> undef, <4 x i16> undef, i32 0, i32 8)
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vld3.16 {{{d[0-9]+\[\]}}, {{d[0-9]+\[\]}}, {{d[0-9]+\[\]}}}, [r0]
  %tmp1 = extractvalue %struct.__neon_int16x4x3_t %tmp0, 0
  %tmp2 = shufflevector <4 x i16> %tmp1, <4 x i16> undef, <4 x i32> zeroinitializer
  %tmp3 = extractvalue %struct.__neon_int16x4x3_t %tmp0, 1
  %tmp4 = shufflevector <4 x i16> %tmp3, <4 x i16> undef, <4 x i32> zeroinitializer
  %tmp5 = extractvalue %struct.__neon_int16x4x3_t %tmp0, 2
  %tmp6 = shufflevector <4 x i16> %tmp5, <4 x i16> undef, <4 x i32> zeroinitializer
  %tmp7 = add <4 x i16> %tmp2, %tmp4
  %tmp8 = add <4 x i16> %tmp7, %tmp6
  ret <4 x i16> %tmp8
}

define <2 x i32> @vld4dupi32(i8* %A) nounwind {
  %tmp0 = tail call %struct.__neon_int32x2x4_t @llvm.arm.neon.vld4lane.v2i32(i8* %A, <2 x i32> undef, <2 x i32> undef, <2 x i32> undef, <2 x i32> undef, i32 0, i32 8)
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vld4.32 {{{d[0-9]+\[\]}}, {{d[0-9]+\[\]}}, {{d[0-9]+\[\]}}, {{d[0-9]+\[\]}}}, [r0:64]
  %tmp1 = extractvalue %struct.__neon_int32x2x4_t %tmp0, 0
  %tmp2 = shufflevector <2 x i32> %tmp1, <2 x i32> undef, <2 x i32> zeroinitializer
  %tmp3 = extractvalue %struct.__neon_int32x2x4_t %tmp0, 1
  %tmp4 = shufflevector <2 x i32> %tmp3, <2 x i32> undef, <2 x i32> zeroinitializer
  %tmp5 = extractvalue %struct.__neon_int32x2x4_t %tmp0, 2
  %tmp6 = shufflevector <2 x i32> %tmp5, <2 x i32> undef, <2 x i32> zeroinitializer
  %tmp7 = extractvalue %struct.__neon_int32x2x4_t %tmp0, 3
  %tmp8 = shufflevector <2 x i32> %tmp7, <2 x i32> undef, <2 x i32> zeroinitializer
  %tmp9 = add <2 x i32> %tmp2, %tmp4
  %tmp10 = add <2 x i32> %tmp6, %tmp8
  %tmp11 = add <2 x i32> %tmp9, %tmp10
  ret <2 x i32> %tmp11
}

;Check for a post-increment updating load.
define <4 x i16> @vld4dupi16_update(i16** %ptr) nounwind {
  %A = load i16** %ptr
  %A2 = bitcast i16* %A to i8*
  %tmp0 = tail call %struct.__neon_int16x4x4_t @llvm.arm.neon.vld4lane.v4i16(i8* %A2, <4 x i16> undef, <4 x i16> undef, <4 x i16> undef, <4 x i16> undef, i32 0, i32 1)
; CHECK:         bic r1, r1, #3221225472
; CHECK-NEXT:    vld4.16 {{{d[0-9]+\[\]}}, {{d[0-9]+\[\]}}, {{d[0-9]+\[\]}}, {{d[0-9]+\[\]}}}, [r1]!
  %tmp1 = extractvalue %struct.__neon_int16x4x4_t %tmp0, 0
  %tmp2 = shufflevector <4 x i16> %tmp1, <4 x i16> undef, <4 x i32> zeroinitializer
  %tmp3 = extractvalue %struct.__neon_int16x4x4_t %tmp0, 1
  %tmp4 = shufflevector <4 x i16> %tmp3, <4 x i16> undef, <4 x i32> zeroinitializer
  %tmp5 = extractvalue %struct.__neon_int16x4x4_t %tmp0, 2
  %tmp6 = shufflevector <4 x i16> %tmp5, <4 x i16> undef, <4 x i32> zeroinitializer
  %tmp7 = extractvalue %struct.__neon_int16x4x4_t %tmp0, 3
  %tmp8 = shufflevector <4 x i16> %tmp7, <4 x i16> undef, <4 x i32> zeroinitializer
  %tmp9 = add <4 x i16> %tmp2, %tmp4
  %tmp10 = add <4 x i16> %tmp6, %tmp8
  %tmp11 = add <4 x i16> %tmp9, %tmp10
  %tmp12 = getelementptr i16* %A, i32 4
  store i16* %tmp12, i16** %ptr
  ret <4 x i16> %tmp11
}
