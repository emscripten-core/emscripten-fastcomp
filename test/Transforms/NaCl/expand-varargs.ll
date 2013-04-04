; RUN: opt < %s -expand-varargs -S | FileCheck %s

%va_list = type i8*

declare void @llvm.va_start(i8*)
declare void @llvm.va_end(i8*)
declare void @llvm.va_copy(i8*, i8*)

declare i32 @outside_func(i32 %arg, %va_list* %args)


; Produced by the expansion of @varargs_call1():
; CHECK: %vararg_call = type <{ i64, i32 }>

; Produced by the expansion of @call_with_zero_varargs().
; We have a dummy field to deal with buggy programs:
; CHECK: %vararg_call.0 = type <{ i32 }>


define i32 @varargs_func(i32 %arg, ...) {
  %arglist_alloc = alloca %va_list
  %arglist = bitcast %va_list* %arglist_alloc to i8*

  call void @llvm.va_start(i8* %arglist)
  %result = call i32 @outside_func(i32 %arg, %va_list* %arglist_alloc)
  call void @llvm.va_end(i8* %arglist)
  ret i32 %result
}
; CHECK: define i32 @varargs_func(i32 %arg, i8* %varargs) {
; CHECK-NEXT: %arglist_alloc = alloca i8*
; CHECK-NEXT: %arglist = bitcast i8** %arglist_alloc to i8*
; CHECK-NEXT: %arglist1 = bitcast i8* %arglist to i8**
; CHECK-NEXT: store i8* %varargs, i8** %arglist1
; CHECK-NEXT: %result = call i32 @outside_func(i32 %arg, i8** %arglist_alloc)
; CHECK-NEXT: ret i32 %result


define i32 @varargs_call1() {
  %result = call i32 (i32, ...)* @varargs_func(i32 111, i64 222, i32 333)
  ret i32 %result
}
; CHECK: define i32 @varargs_call1() {
; CHECK-NEXT: %vararg_buffer = alloca %vararg_call
; CHECK-NEXT: %vararg_lifetime_bitcast = bitcast %vararg_call* %vararg_buffer to i8*
; CHECK-NEXT: call void @llvm.lifetime.start(i64 12, i8* %vararg_lifetime_bitcast)
; CHECK-NEXT: %vararg_ptr = getelementptr %vararg_call* %vararg_buffer, i32 0, i32 0
; CHECK-NEXT: store i64 222, i64* %vararg_ptr
; CHECK-NEXT: %vararg_ptr1 = getelementptr %vararg_call* %vararg_buffer, i32 0, i32 1
; CHECK-NEXT: store i32 333, i32* %vararg_ptr1
; CHECK-NEXT: %vararg_func = bitcast i32 (i32, ...)* bitcast (i32 (i32, i8*)* @varargs_func to i32 (i32, ...)*) to i32 (i32, %vararg_call*)*
; CHECK-NEXT: %result = call i32 %vararg_func(i32 111, %vararg_call* %vararg_buffer)
; CHECK-NEXT: call void @llvm.lifetime.end(i64 12, i8* %vararg_lifetime_bitcast)
; CHECK-NEXT: ret i32 %result


; Check that the pass works when there are no variable arguments.
define i32 @call_with_zero_varargs() {
  %result = call i32 (i32, ...)* @varargs_func(i32 111)
  ret i32 %result
}
; CHECK: define i32 @call_with_zero_varargs() {
; CHECK-NEXT: %vararg_buffer = alloca %vararg_call.0
; CHECK: %vararg_func = bitcast i32 (i32, ...)* bitcast (i32 (i32, i8*)* @varargs_func to i32 (i32, ...)*) to i32 (i32, %vararg_call.0*)*
; CHECK-NEXT: %result = call i32 %vararg_func(i32 111, %vararg_call.0* %vararg_buffer)


; Check that "invoke" instructions are expanded out too.
define i32 @varargs_invoke() {
  %result = invoke i32 (i32, ...)* @varargs_func(i32 111, i64 222)
      to label %cont unwind label %lpad
cont:
  ret i32 %result
lpad:
  %lp = landingpad { i8*, i32 } personality i8* null cleanup
  ret i32 0
}
; CHECK: @varargs_invoke
; CHECK: %result = invoke i32 %vararg_func(i32 111, %vararg_call.1* %vararg_buffer)
; CHECK-NEXT: to label %cont unwind label %lpad
; CHECK: cont:
; CHECK-NEXT: call void @llvm.lifetime.end(i64 8, i8* %vararg_lifetime_bitcast)
; CHECK: lpad:
; CHECK: call void @llvm.lifetime.end(i64 8, i8* %vararg_lifetime_bitcast)


define void @varargs_multiple_calls() {
  %call1 = call i32 (i32, ...)* @varargs_func(i32 11, i64 22, i32 33)
  %call2 = call i32 (i32, ...)* @varargs_func(i32 44, i64 55, i32 66)
  ret void
}
; CHECK: @varargs_multiple_calls()
; The added allocas should appear at the start of the function.
; CHECK: %vararg_buffer{{.*}} = alloca %vararg_call{{.*}}
; CHECK: %vararg_buffer{{.*}} = alloca %vararg_call{{.*}}
; CHECK: %call1 = call i32 %vararg_func{{.*}}(i32 11, %vararg_call{{.*}}* %vararg_buffer{{.*}})
; CHECK: %call2 = call i32 %vararg_func{{.*}}(i32 44, %vararg_call{{.*}}* %vararg_buffer{{.*}})


define i32 @va_arg_i32(i8* %arglist) {
  %result = va_arg i8* %arglist, i32
  ret i32 %result
}
; CHECK: define i32 @va_arg_i32(i8* %arglist) {
; CHECK-NEXT: %arglist1 = bitcast i8* %arglist to i32**
; CHECK-NEXT: %arglist_current = load i32** %arglist1
; CHECK-NEXT: %result = load i32* %arglist_current
; CHECK-NEXT: %arglist_next = getelementptr i32* %arglist_current, i32 1
; CHECK-NEXT: store i32* %arglist_next, i32** %arglist1
; CHECK-NEXT: ret i32 %result


define i64 @va_arg_i64(i8* %arglist) {
  %result = va_arg i8* %arglist, i64
  ret i64 %result
}
; CHECK: define i64 @va_arg_i64(i8* %arglist) {
; CHECK-NEXT: %arglist1 = bitcast i8* %arglist to i64**
; CHECK-NEXT: %arglist_current = load i64** %arglist1
; CHECK-NEXT: %result = load i64* %arglist_current
; CHECK-NEXT: %arglist_next = getelementptr i64* %arglist_current, i32 1
; CHECK-NEXT: store i64* %arglist_next, i64** %arglist1
; CHECK-NEXT: ret i64 %result


define void @do_va_copy(i8* %dest, i8* %src) {
  call void @llvm.va_copy(i8* %dest, i8* %src)
  ret void
}
; CHECK: define void @do_va_copy(i8* %dest, i8* %src) {
; CHECK-NEXT: %vacopy_src = bitcast i8* %src to i8**
; CHECK-NEXT: %vacopy_dest = bitcast i8* %dest to i8**
; CHECK-NEXT: %vacopy_currentptr = load i8** %vacopy_src
; CHECK-NEXT: store i8* %vacopy_currentptr, i8** %vacopy_dest
; CHECK-NEXT: ret void
