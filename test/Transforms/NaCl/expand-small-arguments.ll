; RUN: opt %s -expand-small-arguments -S | FileCheck %s

@var = global i8 0


define void @small_arg(i8 %val) {
  store i8 %val, i8* @var
  ret void
}
; CHECK: define void @small_arg(i32 %val) {
; CHECK-NEXT: %val.arg_trunc = trunc i32 %val to i8
; CHECK-NEXT: store i8 %val.arg_trunc, i8* @var


define i8 @small_result() {
  %val = load i8* @var
  ret i8 %val
}
; CHECK: define i32 @small_result() {
; CHECK-NEXT: %val = load i8* @var
; CHECK-NEXT: %val.ret_ext = zext i8 %val to i32
; CHECK-NEXT: ret i32 %val.ret_ext

define signext i8 @small_result_signext() {
  %val = load i8* @var
  ret i8 %val
}
; CHECK: define signext i32 @small_result_signext() {
; CHECK-NEXT: %val = load i8* @var
; CHECK-NEXT: %val.ret_ext = sext i8 %val to i32
; CHECK-NEXT: ret i32 %val.ret_ext


define void @call_small_arg() {
  call void @small_arg(i8 100)
  ret void
}
; CHECK: define void @call_small_arg() {
; CHECK-NEXT: %arg_ext = zext i8 100 to i32
; CHECK-NEXT: %.arg_cast = bitcast {{.*}} @small_arg
; CHECK-NEXT: call void %.arg_cast(i32 %arg_ext)

define void @call_small_arg_signext() {
  call void @small_arg(i8 signext 100)
  ret void
}
; CHECK: define void @call_small_arg_signext() {
; CHECK-NEXT: %arg_ext = sext i8 100 to i32
; CHECK-NEXT: %.arg_cast = bitcast {{.*}} @small_arg
; CHECK-NEXT: call void %.arg_cast(i32 signext %arg_ext)


define void @call_small_result() {
  %r = call i8 @small_result()
  store i8 %r, i8* @var
  ret void
}
; CHECK: define void @call_small_result() {
; CHECK-NEXT: %r.arg_cast = bitcast {{.*}} @small_result
; CHECK-NEXT: %r = call i32 %r.arg_cast()
; CHECK-NEXT: %r.ret_trunc = trunc i32 %r to i8
; CHECK-NEXT: store i8 %r.ret_trunc, i8* @var


; Check that various attributes are preserved.
define i1 @attributes(i8 %arg) nounwind {
  %r = tail call fastcc i1 @attributes(i8 %arg) nounwind
  ret i1 %r
}
; CHECK: define i32 @attributes(i32 %arg) [[NOUNWIND:#[0-9]+]] {
; CHECK: tail call fastcc i32 {{.*}} [[NOUNWIND]]


; These arguments and results should be left alone.
define i64 @larger_arguments(i32 %a, i64 %b, i8* %ptr, double %d) {
  %r = call i64 @larger_arguments(i32 %a, i64 %b, i8* %ptr, double %d)
  ret i64 %r
}
; CHECK: define i64 @larger_arguments(i32 %a, i64 %b, i8* %ptr, double %d) {
; CHECK-NEXT: %r = call i64 @larger_arguments(i32 %a, i64 %b, i8* %ptr, double %d)
; CHECK-NEXT: ret i64 %r


; Intrinsics must be left alone since the pass cannot change their types.

declare void @llvm.memset.p0i8.i32(i8* nocapture, i8, i32, i32, i1)
; CHECK: declare void @llvm.memset.p0i8.i32(i8* nocapture, i8, i32, i32, i1)

define void @intrinsic_call(i8* %ptr) {
  call void @llvm.memset.p0i8.i32(i8* %ptr, i8 99, i32 256, i32 1, i1 0)
  ret void
}
; CHECK: define void @intrinsic_call
; CHECK-NEXT: call void @llvm.memset.p0i8.i32(i8* %ptr, i8 99,


; CHECK: attributes [[NOUNWIND]] = { nounwind }
