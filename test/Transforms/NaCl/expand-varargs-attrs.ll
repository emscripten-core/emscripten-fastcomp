; RUN: opt < %s -expand-varargs -S | FileCheck %s

declare i32 @varargs_func(i32 %arg, ...)


; Check that attributes such as "byval" are preserved on fixed arguments.

%MyStruct = type { i64, i64 }

define void @func_with_arg_attrs(%MyStruct* byval, ...) {
  ret void
}
; CHECK: define void @func_with_arg_attrs(%MyStruct* byval, i8* %varargs) {


declare void @take_struct_arg(%MyStruct* byval %s, ...)

define void @call_with_arg_attrs(%MyStruct* %s) {
  call void (%MyStruct*, ...)* @take_struct_arg(%MyStruct* byval %s)
  ret void
}
; CHECK: define void @call_with_arg_attrs(%MyStruct* %s) {
; CHECK: call void %vararg_func(%MyStruct* byval %s, {}* undef)


; The "byval" attribute here should be dropped.
define i32 @pass_struct_via_vararg1(%MyStruct* %s) {
  %result = call i32 (i32, ...)* @varargs_func(i32 111, %MyStruct* byval %s)
  ret i32 %result
}
; CHECK: define i32 @pass_struct_via_vararg1(%MyStruct* %s) {
; CHECK: %result = call i32 %vararg_func(i32 111, %{{.*}}* %vararg_buffer)


; The "byval" attribute here should be dropped.
define i32 @pass_struct_via_vararg2(%MyStruct* %s) {
  %result = call i32 (i32, ...)* @varargs_func(i32 111, i32 2, %MyStruct* byval %s)
  ret i32 %result
}
; CHECK: define i32 @pass_struct_via_vararg2(%MyStruct* %s) {
; CHECK: %result = call i32 %vararg_func(i32 111, %{{.*}}* %vararg_buffer)


; Check that return attributes such as "signext" are preserved.
define i32 @call_with_return_attr() {
  %result = call signext i32 (i32, ...)* @varargs_func(i32 111, i64 222)
  ret i32 %result
}
; CHECK: define i32 @call_with_return_attr() {
; CHECK: %result = call signext i32 %vararg_func(i32 111


; Check that the "readonly" function attribute is preserved.
define i32 @call_readonly() {
  %result = call i32 (i32, ...)* @varargs_func(i32 111, i64 222) readonly
  ret i32 %result
}
; CHECK: define i32 @call_readonly() {
; CHECK: %result = call i32 %vararg_func(i32 111, {{.*}}) #1


; Check that the "tail" attribute gets removed, because the callee
; reads space alloca'd by the caller.
define i32 @tail_call() {
  %result = tail call i32 (i32, ...)* @varargs_func(i32 111, i64 222)
  ret i32 %result
}
; CHECK: define i32 @tail_call() {
; CHECK: %result = call i32 %vararg_func(i32 111


; CHECK: attributes #1 = { readonly }
