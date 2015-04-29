; RUN: opt < %s -expand-varargs -S | FileCheck %s

declare i32 @varargs_func(i32 %arg, ...)


%MyStruct = type { i64, i64 }

; Test passing a struct by value.
define i32 @varargs_call_struct(%MyStruct* %ptr) {
  %result = call i32 (i32, ...) @varargs_func(i32 111, i64 222, %MyStruct* byval %ptr)
  ret i32 %result
}
; CHECK-LABEL: @varargs_call_struct(
; CHECK: %vararg_ptr1 = getelementptr inbounds { i64, %MyStruct }, { i64, %MyStruct }* %vararg_buffer, i32 0, i32 1
; CHECK: %1 = bitcast %MyStruct* %vararg_ptr1 to i8*
; CHECK: %2 = bitcast %MyStruct* %ptr to i8*
; CHECK: call void @llvm.memcpy.p0i8.p0i8.i64(i8* %1, i8* %2, i64 16, i32 1, i1 false)
