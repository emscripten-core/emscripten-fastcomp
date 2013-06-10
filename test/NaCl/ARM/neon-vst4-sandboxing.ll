; RUN: pnacl-llc -mtriple=armv7-unknown-nacl -mattr=+neon -sfi-store -sfi-load -filetype=obj %s -o - \
; RUN:  | llvm-objdump -disassemble -triple armv7 - | FileCheck %s

define void @vst4i8(i8* %A, <8 x i8>* %B) nounwind {
  %tmp1 = load <8 x i8>* %B
  call void @llvm.arm.neon.vst4.v8i8(i8* %A, <8 x i8> %tmp1, <8 x i8> %tmp1, <8 x i8> %tmp1, <8 x i8> %tmp1, i32 8)
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vst4.8 {d{{[0-9]+}}, d{{[0-9]+}}, d{{[0-9]+}}, d{{[0-9]+}}}, [r0:64]
  ret void
}

define void @vst4i16(i16* %A, <4 x i16>* %B) nounwind {
  %tmp0 = bitcast i16* %A to i8*
  %tmp1 = load <4 x i16>* %B
  call void @llvm.arm.neon.vst4.v4i16(i8* %tmp0, <4 x i16> %tmp1, <4 x i16> %tmp1, <4 x i16> %tmp1, <4 x i16> %tmp1, i32 16)
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vst4.16 {d{{[0-9]+}}, d{{[0-9]+}}, d{{[0-9]+}}, d{{[0-9]+}}}, [r0:128]
  ret void
}

define void @vst4i32(i32* %A, <2 x i32>* %B) nounwind {
  %tmp0 = bitcast i32* %A to i8*
  %tmp1 = load <2 x i32>* %B
  call void @llvm.arm.neon.vst4.v2i32(i8* %tmp0, <2 x i32> %tmp1, <2 x i32> %tmp1, <2 x i32> %tmp1, <2 x i32> %tmp1, i32 32)
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vst4.32 {d{{[0-9]+}}, d{{[0-9]+}}, d{{[0-9]+}}, d{{[0-9]+}}}, [r0:256]
  ret void
}

;Check for a post-increment updating store.
define void @vst4Qf_update(float** %ptr, <4 x float>* %B) nounwind {
  %A = load float** %ptr
  %tmp0 = bitcast float* %A to i8*
  %tmp1 = load <4 x float>* %B
; CHECK:         bic r1, r1, #3221225472
; CHECK-NEXT:    vld1.32 {d{{[0-9]+}}, d{{[0-9]+}}}, [r1]
  call void @llvm.arm.neon.vst4.v4f32(i8* %tmp0, <4 x float> %tmp1, <4 x float> %tmp1, <4 x float> %tmp1, <4 x float> %tmp1, i32 1)
; CHECK:         bic r1, r1, #3221225472
; CHECK-NEXT:    vst4.32 {d{{[0-9]+}}, d{{[0-9]+}}, d{{[0-9]+}}, d{{[0-9]+}}}, [r1]!
  %tmp2 = getelementptr float* %A, i32 16
  store float* %tmp2, float** %ptr
  ret void
}

declare void @llvm.arm.neon.vst4.v8i8(i8*, <8 x i8>, <8 x i8>, <8 x i8>, <8 x i8>, i32) nounwind
declare void @llvm.arm.neon.vst4.v4i16(i8*, <4 x i16>, <4 x i16>, <4 x i16>, <4 x i16>, i32) nounwind
declare void @llvm.arm.neon.vst4.v2i32(i8*, <2 x i32>, <2 x i32>, <2 x i32>, <2 x i32>, i32) nounwind
declare void @llvm.arm.neon.vst4.v2f32(i8*, <2 x float>, <2 x float>, <2 x float>, <2 x float>, i32) nounwind
declare void @llvm.arm.neon.vst4.v1i64(i8*, <1 x i64>, <1 x i64>, <1 x i64>, <1 x i64>, i32) nounwind

declare void @llvm.arm.neon.vst4.v16i8(i8*, <16 x i8>, <16 x i8>, <16 x i8>, <16 x i8>, i32) nounwind
declare void @llvm.arm.neon.vst4.v8i16(i8*, <8 x i16>, <8 x i16>, <8 x i16>, <8 x i16>, i32) nounwind
declare void @llvm.arm.neon.vst4.v4i32(i8*, <4 x i32>, <4 x i32>, <4 x i32>, <4 x i32>, i32) nounwind
declare void @llvm.arm.neon.vst4.v4f32(i8*, <4 x float>, <4 x float>, <4 x float>, <4 x float>, i32) nounwind

