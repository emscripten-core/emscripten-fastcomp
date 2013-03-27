; RUN: opt < %s -expand-varargs -S | FileCheck %s

declare i32 @varargs_func(i32 %arg, ...)


%MyStruct = type { i64, i64 }

; CHECK: %vararg_call = type <{ i64, %MyStruct }>

; Test passing a struct by value.
define i32 @varargs_call_struct(%MyStruct* %ptr) {
  %result = call i32 (i32, ...)* @varargs_func(i32 111, i64 222, %MyStruct* byval %ptr)
  ret i32 %result
}
; CHECK: define i32 @varargs_call_struct(%MyStruct* %ptr) {
; CHECK: %vararg_struct_copy = load %MyStruct* %ptr
; CHECK: %vararg_ptr1 = getelementptr %vararg_call* %vararg_buffer, i32 0, i32 1
; CHECK: store %MyStruct %vararg_struct_copy, %MyStruct* %vararg_ptr1
