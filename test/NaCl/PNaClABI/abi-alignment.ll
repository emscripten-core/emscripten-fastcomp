; RUN: pnacl-abicheck < %s | FileCheck %s

; Test the "align" attributes that are allowed on load and store
; instructions.  Note that "cmpxchg" and "atomicrmw" do not take
; "align" attributes, so are not tested here.


declare void @llvm.memcpy.p0i8.p0i8.i32(i8*, i8*, i32, i32, i1)
declare void @llvm.memmove.p0i8.p0i8.i32(i8*, i8*, i32, i32, i1)
declare void @llvm.memset.p0i8.i32(i8*, i8, i32, i32, i1)


define void @allowed_cases(i32 %ptr, float %f, double %d) {
  %ptr.i32 = inttoptr i32 %ptr to i32*
  load i32* %ptr.i32, align 1
  store i32 123, i32* %ptr.i32, align 1

  %ptr.float = inttoptr i32 %ptr to float*
  load float* %ptr.float, align 1
  load float* %ptr.float, align 4
  store float %f, float* %ptr.float, align 1
  store float %f, float* %ptr.float, align 4

  %ptr.double = inttoptr i32 %ptr to double*
  load double* %ptr.double, align 1
  load double* %ptr.double, align 8
  store double %d, double* %ptr.double, align 1
  store double %d, double* %ptr.double, align 8

  ; Stricter alignments are required for atomics.
  load atomic i32* %ptr.i32 seq_cst, align 4
  store atomic i32 123, i32* %ptr.i32 seq_cst, align 4
  load atomic float* %ptr.float seq_cst, align 4
  store atomic float %f, float* %ptr.float seq_cst, align 4
  load atomic double* %ptr.double seq_cst, align 8
  store atomic double %d, double* %ptr.double seq_cst, align 8

  ; memcpy() et el take an alignment parameter, which is allowed to be 1.
  %ptr.p = inttoptr i32 %ptr to i8*
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %ptr.p, i8* %ptr.p,
                                       i32 10, i32 1, i1 false)
  call void @llvm.memmove.p0i8.p0i8.i32(i8* %ptr.p, i8* %ptr.p,
                                        i32 10, i32 1, i1 false)
  call void @llvm.memset.p0i8.i32(i8* %ptr.p, i8 99,
                                  i32 10, i32 1, i1 false)

  ret void
}
; CHECK-NOT: disallowed


define void @rejected_cases(i32 %ptr, float %f, double %d, i32 %align) {
  %ptr.i32 = inttoptr i32 %ptr to i32*
  load i32* %ptr.i32, align 4
  store i32 123, i32* %ptr.i32, align 4
; CHECK: disallowed: bad alignment: {{.*}} load i32{{.*}} align 4
; CHECK-NEXT: disallowed: bad alignment: store i32{{.*}} align 4

  ; Unusual, not-very-useful alignments are rejected.
  %ptr.float = inttoptr i32 %ptr to float*
  load float* %ptr.float, align 2
  load float* %ptr.float, align 8
  store float %f, float* %ptr.float, align 2
  store float %f, float* %ptr.float, align 8
; CHECK-NEXT: disallowed: bad alignment: {{.*}} load float{{.*}} align 2
; CHECK-NEXT: disallowed: bad alignment: {{.*}} load float{{.*}} align 8
; CHECK-NEXT: disallowed: bad alignment: store float{{.*}} align 2
; CHECK-NEXT: disallowed: bad alignment: store float{{.*}} align 8

  %ptr.double = inttoptr i32 %ptr to double*
  load double* %ptr.double, align 2
  load double* %ptr.double, align 4
  store double %d, double* %ptr.double, align 2
  store double %d, double* %ptr.double, align 4
; CHECK-NEXT: disallowed: bad alignment: {{.*}} load double{{.*}} align 2
; CHECK-NEXT: disallowed: bad alignment: {{.*}} load double{{.*}} align 4
; CHECK-NEXT: disallowed: bad alignment: store double{{.*}} align 2
; CHECK-NEXT: disallowed: bad alignment: store double{{.*}} align 4

  ; Too-small alignments for atomics are rejected.
  load atomic i32* %ptr.i32 seq_cst, align 2
  load atomic float* %ptr.float seq_cst, align 2
  load atomic double* %ptr.double seq_cst, align 4
; CHECK-NEXT: disallowed: bad alignment: {{.*}} load atomic i32{{.*}} align 2
; CHECK-NEXT: disallowed: bad alignment: {{.*}} load atomic float{{.*}} align 2
; CHECK-NEXT: disallowed: bad alignment: {{.*}} load atomic double{{.*}} align 4

  ; Too-large alignments for atomics are also rejected.
  load atomic i32* %ptr.i32 seq_cst, align 8
  load atomic float* %ptr.float seq_cst, align 8
  load atomic double* %ptr.double seq_cst, align 16
; CHECK-NEXT: disallowed: bad alignment: {{.*}} load atomic i32{{.*}} align 8
; CHECK-NEXT: disallowed: bad alignment: {{.*}} load atomic float{{.*}} align 8
; CHECK-NEXT: disallowed: bad alignment: {{.*}} load atomic double{{.*}} align 16

  ; Non-pessimistic alignments for memcpy() et al are rejected.
  %ptr.p = inttoptr i32 %ptr to i8*
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %ptr.p, i8* %ptr.p,
                                       i32 10, i32 4, i1 false)
  call void @llvm.memmove.p0i8.p0i8.i32(i8* %ptr.p, i8* %ptr.p,
                                        i32 10, i32 4, i1 false)
  call void @llvm.memset.p0i8.i32(i8* %ptr.p, i8 99,
                                  i32 10, i32 4, i1 false)
; CHECK-NEXT: bad alignment: call void @llvm.memcpy
; CHECK-NEXT: bad alignment: call void @llvm.memmove
; CHECK-NEXT: bad alignment: call void @llvm.memset

  ; Check that the verifier does not crash if the alignment argument
  ; is not a constant.
  call void @llvm.memcpy.p0i8.p0i8.i32(i8* %ptr.p, i8* %ptr.p,
                                       i32 10, i32 %align, i1 false)
  call void @llvm.memmove.p0i8.p0i8.i32(i8* %ptr.p, i8* %ptr.p,
                                        i32 10, i32 %align, i1 false)
  call void @llvm.memset.p0i8.i32(i8* %ptr.p, i8 99,
                                  i32 10, i32 %align, i1 false)
; CHECK-NEXT: bad alignment: call void @llvm.memcpy
; CHECK-NEXT: bad alignment: call void @llvm.memmove
; CHECK-NEXT: bad alignment: call void @llvm.memset

  ret void
}
; CHECK-NOT: disallowed
