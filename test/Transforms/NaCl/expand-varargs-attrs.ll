; RUN: opt < %s -expand-varargs -S | FileCheck %s

declare i32 @varargs_func(i32 %arg, ...)


; Check that attributes such as "byval" are preserved on fixed arguments.

%MyStruct = type { i64, i64 }

define void @func_with_arg_attrs(%MyStruct* byval, ...) {
  ret void
}
; CHECK-LABEL: define void @func_with_arg_attrs(%MyStruct* byval, i8* noalias %varargs) {


declare void @take_struct_arg(%MyStruct* byval %s, ...)

define void @call_with_arg_attrs(%MyStruct* %s) {
  call void (%MyStruct*, ...) @take_struct_arg(%MyStruct* byval %s)
  ret void
}
; CHECK-LABEL: @call_with_arg_attrs(
; CHECK: call void bitcast (void (%MyStruct*, i8*)* @take_struct_arg to void (%MyStruct*, { i32 }*)*)(%MyStruct* byval %s, { i32 }* %vararg_buffer)


; The "byval" attribute here should be dropped.
define i32 @pass_struct_via_vararg1(%MyStruct* %s) {
  %result = call i32 (i32, ...) @varargs_func(i32 111, %MyStruct* byval %s)
  ret i32 %result
}
; CHECK-LABEL: @pass_struct_via_vararg1(
; CHECK: %result = call i32 bitcast (i32 (i32, i8*)* @varargs_func to i32 (i32, { %MyStruct }*)*)(i32 111, { %MyStruct }* %vararg_buffer)


; The "byval" attribute here should be dropped.
define i32 @pass_struct_via_vararg2(%MyStruct* %s) {
  %result = call i32 (i32, ...) @varargs_func(i32 111, i32 2, %MyStruct* byval %s)
  ret i32 %result
}
; CHECK-LABEL: @pass_struct_via_vararg2(
; CHECK: %result = call i32 bitcast (i32 (i32, i8*)* @varargs_func to i32 (i32, { i32, %MyStruct }*)*)(i32 111, { i32, %MyStruct }* %vararg_buffer)


; Check that return attributes such as "signext" are preserved.
define i32 @call_with_return_attr() {
  %result = call signext i32 (i32, ...) @varargs_func(i32 111, i64 222)
  ret i32 %result
}
; CHECK-LABEL: @call_with_return_attr(
; CHECK: %result = call signext i32 bitcast (i32 (i32, i8*)* @varargs_func to i32 (i32, { i64 }*)*)(i32 111, { i64 }* %vararg_buffer)


; Check that the "readonly" function attribute is preserved.
define i32 @call_readonly() {
  %result = call i32 (i32, ...) @varargs_func(i32 111, i64 222) readonly
  ret i32 %result
}
; CHECK-LABEL: @call_readonly(
; CHECK: %result = call i32 bitcast (i32 (i32, i8*)* @varargs_func to i32 (i32, { i64 }*)*)(i32 111, { i64 }* %vararg_buffer) #1


; Check that the "tail" attribute gets removed, because the callee
; reads space alloca'd by the caller.
define i32 @tail_call() {
  %result = tail call i32 (i32, ...) @varargs_func(i32 111, i64 222)
  ret i32 %result
}
; CHECK-LABEL: @tail_call(
; CHECK: %result = call i32 bitcast (i32 (i32, i8*)* @varargs_func to i32 (i32, { i64 }*)*)(i32 111, { i64 }* %vararg_buffer)


; CHECK: attributes #1 = { readonly }
