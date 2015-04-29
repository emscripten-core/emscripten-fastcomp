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
  %val = load i8, i8* @var
  ret i8 %val
}
; CHECK: define i32 @small_result() {
; CHECK-NEXT: %val = load i8, i8* @var
; CHECK-NEXT: %val.ret_ext = zext i8 %val to i32
; CHECK-NEXT: ret i32 %val.ret_ext

define signext i8 @small_result_signext() {
  %val = load i8, i8* @var
  ret i8 %val
}
; CHECK: define signext i32 @small_result_signext() {
; CHECK-NEXT: %val = load i8, i8* @var
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

define void @invoking_small_arg(i8) {
  invoke void @small_arg(i8 %0)
      to label %cont unwind label %lpad
cont:
  ret void
lpad:
  %lp = landingpad { i8*, i32 } personality i8* null cleanup
  ret void
}
; CHECK-LABEL: define void @invoking_small_arg(i32)
; CHECK-NEXT:    %.arg_trunc = trunc i32 %0 to i8
; CHECK-NEXT:    %arg_ext = zext i8 %.arg_trunc to i32
; CHECK-NEXT:    %.arg_cast = bitcast void (i8)* bitcast (void (i32)* @small_arg to void (i8)*) to void (i32)*
; CHECK-NEXT:    invoke void %.arg_cast(i32 %arg_ext)
; CHECK-NEXT:        to label %cont unwind label %lpad

; CHECK:       cont:
; CHECK-NEXT:    ret void

; CHECK:       lpad:
; CHECK-NEXT:    %lp = landingpad { i8*, i32 } personality i8* null
; CHECK-NEXT:            cleanup
; CHECK-NEXT:    ret void

define fastcc void @invoking_cc() {
  invoke fastcc void @invoking_cc()
      to label %cont unwind label %lpad
cont:
  ret void
lpad:
  %lp = landingpad { i8*, i32 } personality i8* null cleanup
  ret void
}
; CHECK-LABEL: define fastcc void @invoking_cc()
; CHECK-NEXT:    invoke fastcc void @invoking_cc()

define void @invoking_attrs() noinline {
  invoke void @invoking_attrs() noinline
      to label %cont unwind label %lpad
cont:
  ret void
lpad:
  %lp = landingpad { i8*, i32 } personality i8* null cleanup
  ret void
}
; CHECK:       define void @invoking_attrs() [[NOINLINE:#[0-9]+]]
; CHECK:         invoke void @invoking_attrs() [[NOINLINE]]

define void @invoking_critical_edge() {
entry:
  %a = invoke i8 @small_result()
      to label %loop unwind label %lpad
loop:
  %b = phi i8 [ %a, %entry ], [ %c, %loop ]
  %c = add i8 1, %b
  %d = icmp eq i8 %c, 5
  br i1 %d, label %exit, label %loop

exit:
  %aa = phi i8 [ 0, %lpad ], [ %c, %loop ]
  ret void

lpad:
  %lp = landingpad { i8*, i32 } personality i8* null cleanup
  br label %exit
}
; CHECK-LABEL: define void @invoking_critical_edge()
; CHECK:        entry:
; CHECK-NEXT:    %a.arg_cast = bitcast i8 ()* bitcast (i32 ()* @small_result to i8 ()*) to i32 ()*
; CHECK-NEXT:    %a = invoke i32 %a.arg_cast()
; CHECK-NEXT:            to label %entry.loop_crit_edge unwind label %lpad

; CHECK:       entry.loop_crit_edge:
; CHECK-NEXT:    %a.ret_trunc = trunc i32 %a to i8
; CHECK-NEXT:    br label %loop

; CHECK:       loop:
; CHECK-NEXT:    %b = phi i8 [ %a.ret_trunc, %entry.loop_crit_edge ], [ %c, %loop ]
; CHECK-NEXT:    %c = add i8 1, %b
; CHECK-NEXT:    %d = icmp eq i8 %c, 5
; CHECK-NEXT:    br i1 %d, label %exit, label %loop

; CHECK:       exit:
; CHECK-NEXT:    %aa = phi i8 [ 0, %lpad ], [ %c, %loop ]
; CHECK-NEXT:    ret void

; CHECK:       lpad:
; CHECK-NEXT:    %lp = landingpad { i8*, i32 } personality i8* null
; CHECK-NEXT:            cleanup
; CHECK-NEXT:    br label %exit

define i8 @invoking_small_result() {
entry:
  %a = invoke i8 @small_result()
      to label %cont unwind label %lpad
cont:
  ret i8 %a
lpad:
  %lp = landingpad { i8*, i32 } personality i8* null cleanup
  ret i8 123
}
; CHECK-LABEL: define i32 @invoking_small_result()
; CHECK:       entry:
; CHECK-NEXT:    %a.arg_cast = bitcast i8 ()* bitcast (i32 ()* @small_result to i8 ()*) to i32 ()*
; CHECK-NEXT:    %a = invoke i32 %a.arg_cast()
; CHECK-NEXT:        to label %cont unwind label %lpad

; CHECK:       cont:
; CHECK-NEXT:    %a.ret_trunc = trunc i32 %a to i8
; CHECK-NEXT:    %a.ret_trunc.ret_ext = zext i8 %a.ret_trunc to i32
; CHECK-NEXT:    ret i32 %a.ret_trunc.ret_ext

; CHECK:       lpad:
; CHECK-NEXT:    %lp = landingpad { i8*, i32 } personality i8* null
; CHECK-NEXT:            cleanup
; CHECK-NEXT:    %.ret_ext = zext i8 123 to i32
; CHECK-NEXT:    ret i32 %.ret_ext


; CHECK: attributes [[NOUNWIND]] = { nounwind }
; CHECK: attributes [[NOINLINE]] = { noinline }
