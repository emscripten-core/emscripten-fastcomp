; RUN: pnacl-llc -mtriple=armv7-unknown-nacl -mattr=+neon -sfi-store -sfi-load -filetype=obj %s -o - \
; RUN:  | llvm-objdump -disassemble -triple armv7 - | FileCheck %s

define void @vst1lanei8(i8* %A, <8 x i8>* %B) nounwind {
  %tmp1 = load <8 x i8>* %B
  %tmp2 = extractelement <8 x i8> %tmp1, i32 3
  store i8 %tmp2, i8* %A, align 8
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vst1.8 {d{{[0-9]+}}[3]}, [r0]
  ret void
}

define void @vst1lanei16(i16* %A, <4 x i16>* %B) nounwind {
  %tmp1 = load <4 x i16>* %B
  %tmp2 = extractelement <4 x i16> %tmp1, i32 2
  store i16 %tmp2, i16* %A, align 8
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vst1.16 {d{{[0-9]+}}[2]}, [r0:16]
  ret void
}

define void @vst1lanei32(i32* %A, <2 x i32>* %B) nounwind {
  %tmp1 = load <2 x i32>* %B
  %tmp2 = extractelement <2 x i32> %tmp1, i32 1
  store i32 %tmp2, i32* %A, align 8
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vst1.32 {d{{[0-9]+}}[1]}, [r0:32]
  ret void
}

define void @vst1laneQi8(i8* %A, <16 x i8>* %B) nounwind {
  %tmp1 = load <16 x i8>* %B
; CHECK:         bic r1, r1, #3221225472
; CHECK-NEXT:    vld1.32 {d{{[0-9]+}}, d{{[0-9]+}}}, [r1]
  %tmp2 = extractelement <16 x i8> %tmp1, i32 9
  store i8 %tmp2, i8* %A, align 8
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vst1.8 {d{{[0-9]+}}[1]}, [r0]
  ret void
}

define void @vst1laneQi16(i16* %A, <8 x i16>* %B) nounwind {
  %tmp1 = load <8 x i16>* %B
; CHECK:         bic r1, r1, #3221225472
; CHECK-NEXT:    vld1.32 {d{{[0-9]+}}, d{{[0-9]+}}}, [r1]
  %tmp2 = extractelement <8 x i16> %tmp1, i32 5
  store i16 %tmp2, i16* %A, align 8
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vst1.16 {d{{[0-9]+}}[1]}, [r0:16]
  ret void
}

define void @vst2lanei8(i8* %A, <8 x i8>* %B) nounwind {
  %tmp1 = load <8 x i8>* %B
  call void @llvm.arm.neon.vst2lane.v8i8(i8* %A, <8 x i8> %tmp1, <8 x i8> %tmp1, i32 1, i32 4)
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vst2.8 {d{{[0-9]+}}[1], d{{[0-9]+}}[1]}, [r0:16]
  ret void
}

define void @vst2lanei16(i16* %A, <4 x i16>* %B) nounwind {
  %tmp0 = bitcast i16* %A to i8*
  %tmp1 = load <4 x i16>* %B
  call void @llvm.arm.neon.vst2lane.v4i16(i8* %tmp0, <4 x i16> %tmp1, <4 x i16> %tmp1, i32 1, i32 8)
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vst2.16 {d{{[0-9]+}}[1], d{{[0-9]+}}[1]}, [r0:32]
  ret void
}

define void @vst2lanei32(i32* %A, <2 x i32>* %B) nounwind {
  %tmp0 = bitcast i32* %A to i8*
  %tmp1 = load <2 x i32>* %B
  call void @llvm.arm.neon.vst2lane.v2i32(i8* %tmp0, <2 x i32> %tmp1, <2 x i32> %tmp1, i32 1, i32 1)
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vst2.32 {d{{[0-9]+}}[1], d{{[0-9]+}}[1]}, [r0]
  ret void
}

define void @vst2laneQi16(i16* %A, <8 x i16>* %B) nounwind {
  %tmp0 = bitcast i16* %A to i8*
  %tmp1 = load <8 x i16>* %B
; CHECK:         bic r1, r1, #3221225472
; CHECK-NEXT:    vld1.32 {d{{[0-9]+}}, d{{[0-9]+}}}, [r1]
  call void @llvm.arm.neon.vst2lane.v8i16(i8* %tmp0, <8 x i16> %tmp1, <8 x i16> %tmp1, i32 5, i32 1)
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vst2.16 {d{{[0-9]+}}[1], d{{[0-9]+}}[1]}, [r0]
  ret void
}

define void @vst2laneQi32(i32* %A, <4 x i32>* %B) nounwind {
  %tmp0 = bitcast i32* %A to i8*
  %tmp1 = load <4 x i32>* %B
; CHECK:         bic r1, r1, #3221225472
; CHECK-NEXT:    vld1.32 {d{{[0-9]+}}, d{{[0-9]+}}}, [r1]
  call void @llvm.arm.neon.vst2lane.v4i32(i8* %tmp0, <4 x i32> %tmp1, <4 x i32> %tmp1, i32 2, i32 16)
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vst2.32 {d{{[0-9]+}}[0], d{{[0-9]+}}[0]}, [r0:64]
  ret void
}

define void @vst3lanei8(i8* %A, <8 x i8>* %B) nounwind {
  %tmp1 = load <8 x i8>* %B
  call void @llvm.arm.neon.vst3lane.v8i8(i8* %A, <8 x i8> %tmp1, <8 x i8> %tmp1, <8 x i8> %tmp1, i32 1, i32 1)
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vst3.8 {d{{[0-9]+}}[1], d{{[0-9]+}}[1], d{{[0-9]+}}[1]}, [r0]
  ret void
}

define void @vst3lanei16(i16* %A, <4 x i16>* %B) nounwind {
  %tmp0 = bitcast i16* %A to i8*
  %tmp1 = load <4 x i16>* %B
  call void @llvm.arm.neon.vst3lane.v4i16(i8* %tmp0, <4 x i16> %tmp1, <4 x i16> %tmp1, <4 x i16> %tmp1, i32 1, i32 8)
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vst3.16 {d{{[0-9]+}}[1], d{{[0-9]+}}[1], d{{[0-9]+}}[1]}, [r0]
  ret void
}

define void @vst3lanei32(i32* %A, <2 x i32>* %B) nounwind {
  %tmp0 = bitcast i32* %A to i8*
  %tmp1 = load <2 x i32>* %B
  call void @llvm.arm.neon.vst3lane.v2i32(i8* %tmp0, <2 x i32> %tmp1, <2 x i32> %tmp1, <2 x i32> %tmp1, i32 1, i32 1)
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vst3.32 {d{{[0-9]+}}[1], d{{[0-9]+}}[1], d{{[0-9]+}}[1]}, [r0]
  ret void
}

define void @vst4lanei8(i8* %A, <8 x i8>* %B) nounwind {
  %tmp1 = load <8 x i8>* %B
  call void @llvm.arm.neon.vst4lane.v8i8(i8* %A, <8 x i8> %tmp1, <8 x i8> %tmp1, <8 x i8> %tmp1, <8 x i8> %tmp1, i32 1, i32 8)
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vst4.8 {d{{[0-9]+}}[1], d{{[0-9]+}}[1], d{{[0-9]+}}[1], d{{[0-9]+}}[1]}, [r0:32]
  ret void
}

define void @vst4lanei16(i16* %A, <4 x i16>* %B) nounwind {
  %tmp0 = bitcast i16* %A to i8*
  %tmp1 = load <4 x i16>* %B
  call void @llvm.arm.neon.vst4lane.v4i16(i8* %tmp0, <4 x i16> %tmp1, <4 x i16> %tmp1, <4 x i16> %tmp1, <4 x i16> %tmp1, i32 1, i32 1)
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vst4.16 {d{{[0-9]+}}[1], d{{[0-9]+}}[1], d{{[0-9]+}}[1], d{{[0-9]+}}[1]}, [r0]
  ret void
}

define void @vst4lanei32(i32* %A, <2 x i32>* %B) nounwind {
  %tmp0 = bitcast i32* %A to i8*
  %tmp1 = load <2 x i32>* %B
  call void @llvm.arm.neon.vst4lane.v2i32(i8* %tmp0, <2 x i32> %tmp1, <2 x i32> %tmp1, <2 x i32> %tmp1, <2 x i32> %tmp1, i32 1, i32 16)
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vst4.32 {d{{[0-9]+}}[1], d{{[0-9]+}}[1], d{{[0-9]+}}[1], d{{[0-9]+}}[1]}, [r0:128]
  ret void
}

define void @vst4laneQi16(i16* %A, <8 x i16>* %B) nounwind {
  %tmp0 = bitcast i16* %A to i8*
  %tmp1 = load <8 x i16>* %B
; CHECK:         bic r1, r1, #3221225472
; CHECK-NEXT:    vld1.32 {d{{[0-9]+}}, d{{[0-9]+}}}, [r1]
  call void @llvm.arm.neon.vst4lane.v8i16(i8* %tmp0, <8 x i16> %tmp1, <8 x i16> %tmp1, <8 x i16> %tmp1, <8 x i16> %tmp1, i32 7, i32 16)
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vst4.16 {d{{[0-9]+}}[3], d{{[0-9]+}}[3], d{{[0-9]+}}[3], d{{[0-9]+}}[3]}, [r0:64]
  ret void
}

define void @vst4laneQi32(i32* %A, <4 x i32>* %B) nounwind {
  %tmp0 = bitcast i32* %A to i8*
  %tmp1 = load <4 x i32>* %B
; CHECK:         bic r1, r1, #3221225472
; CHECK-NEXT:    vld1.32 {d{{[0-9]+}}, d{{[0-9]+}}}, [r1]
  call void @llvm.arm.neon.vst4lane.v4i32(i8* %tmp0, <4 x i32> %tmp1, <4 x i32> %tmp1, <4 x i32> %tmp1, <4 x i32> %tmp1, i32 2, i32 1)
; CHECK:         bic r0, r0, #3221225472
; CHECK-NEXT:    vst4.32 {d{{[0-9]+}}[0], d{{[0-9]+}}[0], d{{[0-9]+}}[0], d{{[0-9]+}}[0]}, [r0]
  ret void
}

declare void @llvm.arm.neon.vst2lane.v8i8(i8*, <8 x i8>, <8 x i8>, i32, i32) nounwind
declare void @llvm.arm.neon.vst2lane.v4i16(i8*, <4 x i16>, <4 x i16>, i32, i32) nounwind
declare void @llvm.arm.neon.vst2lane.v2i32(i8*, <2 x i32>, <2 x i32>, i32, i32) nounwind
declare void @llvm.arm.neon.vst2lane.v2f32(i8*, <2 x float>, <2 x float>, i32, i32) nounwind
declare void @llvm.arm.neon.vst2lane.v8i16(i8*, <8 x i16>, <8 x i16>, i32, i32) nounwind
declare void @llvm.arm.neon.vst2lane.v4i32(i8*, <4 x i32>, <4 x i32>, i32, i32) nounwind
declare void @llvm.arm.neon.vst2lane.v4f32(i8*, <4 x float>, <4 x float>, i32, i32) nounwind
declare void @llvm.arm.neon.vst3lane.v8i8(i8*, <8 x i8>, <8 x i8>, <8 x i8>, i32, i32) nounwind
declare void @llvm.arm.neon.vst3lane.v4i16(i8*, <4 x i16>, <4 x i16>, <4 x i16>, i32, i32) nounwind
declare void @llvm.arm.neon.vst3lane.v2i32(i8*, <2 x i32>, <2 x i32>, <2 x i32>, i32, i32) nounwind
declare void @llvm.arm.neon.vst3lane.v2f32(i8*, <2 x float>, <2 x float>, <2 x float>, i32, i32) nounwind
declare void @llvm.arm.neon.vst3lane.v8i16(i8*, <8 x i16>, <8 x i16>, <8 x i16>, i32, i32) nounwind
declare void @llvm.arm.neon.vst3lane.v4i32(i8*, <4 x i32>, <4 x i32>, <4 x i32>, i32, i32) nounwind
declare void @llvm.arm.neon.vst3lane.v4f32(i8*, <4 x float>, <4 x float>, <4 x float>, i32, i32) nounwind
declare void @llvm.arm.neon.vst4lane.v8i8(i8*, <8 x i8>, <8 x i8>, <8 x i8>, <8 x i8>, i32, i32) nounwind
declare void @llvm.arm.neon.vst4lane.v4i16(i8*, <4 x i16>, <4 x i16>, <4 x i16>, <4 x i16>, i32, i32) nounwind
declare void @llvm.arm.neon.vst4lane.v2i32(i8*, <2 x i32>, <2 x i32>, <2 x i32>, <2 x i32>, i32, i32) nounwind
declare void @llvm.arm.neon.vst4lane.v2f32(i8*, <2 x float>, <2 x float>, <2 x float>, <2 x float>, i32, i32) nounwind
declare void @llvm.arm.neon.vst4lane.v8i16(i8*, <8 x i16>, <8 x i16>, <8 x i16>, <8 x i16>, i32, i32) nounwind
declare void @llvm.arm.neon.vst4lane.v4i32(i8*, <4 x i32>, <4 x i32>, <4 x i32>, <4 x i32>, i32, i32) nounwind
declare void @llvm.arm.neon.vst4lane.v4f32(i8*, <4 x float>, <4 x float>, <4 x float>, <4 x float>, i32, i32) nounwind

;Check for a post-increment updating store with register increment.
define void @vst2lanei16_update(i16** %ptr, <4 x i16>* %B, i32 %inc) nounwind {
; CHECK:         bic r1, r1, #3221225472
  %A = load i16** %ptr
  %tmp0 = bitcast i16* %A to i8*
  %tmp1 = load <4 x i16>* %B
  call void @llvm.arm.neon.vst2lane.v4i16(i8* %tmp0, <4 x i16> %tmp1, <4 x i16> %tmp1, i32 1, i32 2)
; CHECK:         bic r1, r1, #3221225472
; CHECK-NEXT:    vst2.16 {d{{[0-9]+}}[1], d{{[0-9]+}}[1]}, [r1], r2
  %tmp2 = getelementptr i16* %A, i32 %inc
  store i16* %tmp2, i16** %ptr
  ret void
}
