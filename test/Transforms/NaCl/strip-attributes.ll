; RUN: opt -S -nacl-strip-attributes %s | FileCheck %s


@var = unnamed_addr global i32 0
; CHECK: @var = global i32 0


define fastcc void @func_attrs(i32 inreg, i32 zeroext)
    unnamed_addr noreturn nounwind readonly align 8 {
  ret void
}
; CHECK: define void @func_attrs(i32, i32) {

define hidden void @hidden_visibility() {
  ret void
}
; CHECK: define void @hidden_visibility() {

define protected void @protected_visibility() {
  ret void
}
; CHECK: define void @protected_visibility() {


define void @call_attrs() {
  call fastcc void @func_attrs(i32 inreg 10, i32 zeroext 20) noreturn nounwind readonly
  ret void
}
; CHECK: define void @call_attrs()
; CHECK: call void @func_attrs(i32 10, i32 20){{$}}


; We currently don't attempt to strip attributes from intrinsic
; declarations because the reader automatically inserts attributes
; based on built-in knowledge of intrinsics, so it is difficult to get
; rid of them here.
declare i8* @llvm.nacl.read.tp()
; CHECK: declare i8* @llvm.nacl.read.tp() #{{[0-9]+}}

define void @arithmetic_attrs() {
  %add = add nsw i32 1, 2
  %shl = shl nuw i32 3, 4
  %lshr = lshr exact i32 2, 1
  ret void
}
; CHECK: define void @arithmetic_attrs() {
; CHECK-NEXT: %add = add i32 1, 2
; CHECK-NEXT: %shl = shl i32 3, 4
; CHECK-NEXT: %lshr = lshr i32 2, 1


; Implicit default alignments are changed to explicit alignments.
define void @default_alignment_attrs(float %f, double %d) {
  load i8* null
  load i32* null
  load float* null
  load double* null

  store i8 100, i8* null
  store i32 100, i32* null
  store float %f, float* null
  store double %d, double* null
  ret void
}
; CHECK: define void @default_alignment_attrs
; CHECK-NEXT: load i8* null, align 1
; CHECK-NEXT: load i32* null, align 1
; CHECK-NEXT: load float* null, align 4
; CHECK-NEXT: load double* null, align 8
; CHECK-NEXT: store i8 100, i8* null, align 1
; CHECK-NEXT: store i32 100, i32* null, align 1
; CHECK-NEXT: store float %f, float* null, align 4
; CHECK-NEXT: store double %d, double* null, align 8

define void @reduce_alignment_assumptions() {
  load i32* null, align 4
  load float* null, align 2
  load float* null, align 4
  load float* null, align 8
  load double* null, align 2
  load double* null, align 8
  load double* null, align 16

  ; Higher alignment assumptions must be retained for atomics.
  load atomic i32* null seq_cst, align 4
  load atomic i32* null seq_cst, align 8
  store atomic i32 100, i32* null seq_cst, align 4
  store atomic i32 100, i32* null seq_cst, align 8
  ret void
}
; CHECK: define void @reduce_alignment_assumptions
; CHECK-NEXT: load i32* null, align 1
; CHECK-NEXT: load float* null, align 1
; CHECK-NEXT: load float* null, align 4
; CHECK-NEXT: load float* null, align 4
; CHECK-NEXT: load double* null, align 1
; CHECK-NEXT: load double* null, align 8
; CHECK-NEXT: load double* null, align 8
; CHECK-NEXT: load atomic i32* null seq_cst, align 4
; CHECK-NEXT: load atomic i32* null seq_cst, align 4
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
; CHECK: define void @reduce_memcpy_alignment_assumptions
; CHECK-NEXT: call void @llvm.memcpy.{{.*}}  i32 20, i32 1, i1 false)
; CHECK-NEXT: call void @llvm.memmove.{{.*}} i32 20, i32 1, i1 false)
; CHECK-NEXT: call void @llvm.memset.{{.*}}  i32 20, i32 1, i1 false)
