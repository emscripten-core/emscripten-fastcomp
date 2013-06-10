; RUN: pnacl-llc -mtriple=armv7-unknown-nacl -mattr=+neon -sfi-load -sfi-store -filetype=obj %s -o - \
; RUN:  | llvm-objdump -disassemble -triple armv7 - | FileCheck %s

define <8 x i8> @vld1i8(i8* %A) nounwind {
  %tmp1 = call <8 x i8> @llvm.arm.neon.vld1.v8i8(i8* %A, i32 16)
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vld1.8 {{{d[0-9]+}}}, [r0:64]
  ret <8 x i8> %tmp1
}

define <4 x i16> @vld1i16(i16* %A) nounwind {
  %tmp0 = bitcast i16* %A to i8*
  %tmp1 = call <4 x i16> @llvm.arm.neon.vld1.v4i16(i8* %tmp0, i32 1)
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vld1.16 {{{d[0-9]+}}}, [r0]
  ret <4 x i16> %tmp1
}

define <2 x i32> @vld1i32(i32* %A) nounwind {
  %tmp0 = bitcast i32* %A to i8*
  %tmp1 = call <2 x i32> @llvm.arm.neon.vld1.v2i32(i8* %tmp0, i32 1)
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vld1.32 {{{d[0-9]+}}}, [r0]
  ret <2 x i32> %tmp1
}

; Insert useless arguments here just for the sake of moving
; %A further down the rN chain (testing how sandboxing detects
; the correct register and not just the default r0)
define <1 x i64> @vld1i64(i32 %foo, i32 %bar, i32 %baz,
                          i64* %A) nounwind {
  %tmp0 = bitcast i64* %A to i8*
  %tmp1 = call <1 x i64> @llvm.arm.neon.vld1.v1i64(i8* %tmp0, i32 1)
; CHECK:         bic r3, r3, #3221225472
; CHECK-NEXT:    vld1.64 {{{d[0-9]+}}}, [r3]
  ret <1 x i64> %tmp1
}

define <16 x i8> @vld1Qi8(i8* %A) nounwind {
  %tmp1 = call <16 x i8> @llvm.arm.neon.vld1.v16i8(i8* %A, i32 8)
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vld1.8 {{{d[0-9]+}}, {{d[0-9]+}}}, [r0:64]
  ret <16 x i8> %tmp1
}

define <8 x i16> @vld1Qi16(i16* %A) nounwind {
  %tmp0 = bitcast i16* %A to i8*
  %tmp1 = call <8 x i16> @llvm.arm.neon.vld1.v8i16(i8* %tmp0, i32 32)
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vld1.16 {{{d[0-9]+}}, {{d[0-9]+}}}, [r0:128]
  ret <8 x i16> %tmp1
}

define <4 x i32> @vld1Qi32(i32* %A) nounwind {
  %tmp0 = bitcast i32* %A to i8*
  %tmp1 = call <4 x i32> @llvm.arm.neon.vld1.v4i32(i8* %tmp0, i32 1)
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vld1.32 {{{d[0-9]+}}, {{d[0-9]+}}}, [r0]
  ret <4 x i32> %tmp1
}

define <2 x i64> @vld1Qi64(i64* %A) nounwind {
  %tmp0 = bitcast i64* %A to i8*
  %tmp1 = call <2 x i64> @llvm.arm.neon.vld1.v2i64(i8* %tmp0, i32 1)
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vld1.64 {{{d[0-9]+}}, {{d[0-9]+}}}, [r0]
  ret <2 x i64> %tmp1
}

declare <8 x i8>  @llvm.arm.neon.vld1.v8i8(i8*, i32) nounwind readonly
declare <4 x i16> @llvm.arm.neon.vld1.v4i16(i8*, i32) nounwind readonly
declare <2 x i32> @llvm.arm.neon.vld1.v2i32(i8*, i32) nounwind readonly
declare <2 x float> @llvm.arm.neon.vld1.v2f32(i8*, i32) nounwind readonly
declare <1 x i64> @llvm.arm.neon.vld1.v1i64(i8*, i32) nounwind readonly

declare <16 x i8> @llvm.arm.neon.vld1.v16i8(i8*, i32) nounwind readonly
declare <8 x i16> @llvm.arm.neon.vld1.v8i16(i8*, i32) nounwind readonly
declare <4 x i32> @llvm.arm.neon.vld1.v4i32(i8*, i32) nounwind readonly
declare <4 x float> @llvm.arm.neon.vld1.v4f32(i8*, i32) nounwind readonly
declare <2 x i64> @llvm.arm.neon.vld1.v2i64(i8*, i32) nounwind readonly

define <16 x i8> @vld1Qi8_update(i8** %ptr) nounwind {
  %A = load i8** %ptr
  %tmp1 = call <16 x i8> @llvm.arm.neon.vld1.v16i8(i8* %A, i32 8)
; CHECK:         bic r1, r1, #3221225472
; CHECK-NEXT:    vld1.8 {{{d[0-9]+}}, {{d[0-9]+}}}, [r1:64]!
  %tmp2 = getelementptr i8* %A, i32 16
  store i8* %tmp2, i8** %ptr
  ret <16 x i8> %tmp1
}

define <4 x i16> @vld1i16_update(i16** %ptr) nounwind {
  %A = load i16** %ptr
  %tmp0 = bitcast i16* %A to i8*
  %tmp1 = call <4 x i16> @llvm.arm.neon.vld1.v4i16(i8* %tmp0, i32 1)
; CHECK:         bic r1, r1, #3221225472
; CHECK-NEXT:    vld1.16 {{{d[0-9]+}}}, [r1]!
  %tmp2 = getelementptr i16* %A, i32 4
         store i16* %tmp2, i16** %ptr
  ret <4 x i16> %tmp1
}

define <2 x i32> @vld1i32_update(i32** %ptr, i32 %inc) nounwind {
  %A = load i32** %ptr
  %tmp0 = bitcast i32* %A to i8*
  %tmp1 = call <2 x i32> @llvm.arm.neon.vld1.v2i32(i8* %tmp0, i32 1)
; CHECK:         bic r2, r2, #3221225472
; CHECK-NEXT:    vld1.32 {{{d[0-9]+}}}, [r2], r1
  %tmp2 = getelementptr i32* %A, i32 %inc
  store i32* %tmp2, i32** %ptr
  ret <2 x i32> %tmp1
}

