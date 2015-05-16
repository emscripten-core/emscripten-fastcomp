; RUN: opt -S -normalize-alignment %s 2>&1 | FileCheck %s

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:32"

; Implicit default alignments are changed to explicit alignments.
define void @default_alignment_attrs(float %f, double %d) {
  load i8, i8* null
  load i32, i32* null
  load float, float* null
  load double, double* null

  store i8 100, i8* null
  store i32 100, i32* null
  store float %f, float* null
  store double %d, double* null
  ret void
}
; CHECK-LABEL: @default_alignment_attrs
; CHECK-NEXT: load i8, i8* null, align 1
; CHECK-NEXT: load i32, i32* null, align 1
; CHECK-NEXT: load float, float* null, align 4
; CHECK-NEXT: load double, double* null, align 8
; CHECK-NEXT: store i8 100, i8* null, align 1
; CHECK-NEXT: store i32 100, i32* null, align 1
; CHECK-NEXT: store float %f, float* null, align 4
; CHECK-NEXT: store double %d, double* null, align 8

define void @reduce_alignment_assumptions() {
  load i32, i32* null, align 4
  load float, float* null, align 2
  load float, float* null, align 4
  load float, float* null, align 8
  load double, double* null, align 2
  load double, double* null, align 8
  load double, double* null, align 16

  ; Higher alignment assumptions must be retained for atomics.
  load atomic i32, i32* null seq_cst, align 4
  load atomic i32, i32* null seq_cst, align 8
  store atomic i32 100, i32* null seq_cst, align 4
  store atomic i32 100, i32* null seq_cst, align 8
  ret void
}
; CHECK-LABEL: @reduce_alignment_assumptions
; CHECK-NEXT: load i32, i32* null, align 1
; CHECK-NEXT: load float, float* null, align 1
; CHECK-NEXT: load float, float* null, align 4
; CHECK-NEXT: load float, float* null, align 4
; CHECK-NEXT: load double, double* null, align 1
; CHECK-NEXT: load double, double* null, align 8
; CHECK-NEXT: load double, double* null, align 8
; CHECK-NEXT: load atomic i32, i32* null seq_cst, align 4
; CHECK-NEXT: load atomic i32, i32* null seq_cst, align 4
; CHECK-NEXT: store atomic i32 100, i32* null seq_cst, align 4
; CHECK-NEXT: store atomic i32 100, i32* null seq_cst, align 4

declare void @llvm.memcpy.p0i8.p0i8.i32(i8*, i8*, i32, i32, i1)
declare void @llvm.memmove.p0i8.p0i8.i32(i8*, i8*, i32, i32, i1)
declare void @llvm.memset.p0i8.i32(i8*, i8, i32, i32, i1)

define void @reduce_memcpy_alignment_assumptions(i8* %ptr) {
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %ptr, i8* %ptr,
                                       i32 20, i32 4, i1 false)
  call void @llvm.memmove.p0i8.p0i8.i32(i8* %ptr, i8* %ptr,
                                        i32 20, i32 4, i1 false)
  call void @llvm.memset.p0i8.i32(i8* %ptr, i8 99,
                                  i32 20, i32 4, i1 false)
  ret void
}
; CHECK-LABEL: @reduce_memcpy_alignment_assumptions
; CHECK-NEXT: call void @llvm.memcpy.{{.*}}  i32 20, i32 1, i1 false)
; CHECK-NEXT: call void @llvm.memmove.{{.*}} i32 20, i32 1, i1 false)
; CHECK-NEXT: call void @llvm.memset.{{.*}}  i32 20, i32 1, i1 false)
