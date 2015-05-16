; RUN: opt < %s -mtriple=asmjs-unknown-emscripten -expand-varargs -S | FileCheck %s

target datalayout = "p:32:32:32"

%va_list = type i8*

declare void @llvm.va_start(i8*)
declare void @llvm.va_end(i8*)
declare void @llvm.va_copy(i8*, i8*)

declare void @emscripten_asm_const_int(...)
declare void @emscripten_asm_const_double(...)
declare void @emscripten_landingpad(...)
declare void @emscripten_resume(...)

define void @test(i32 %arg) {
  call void (...) @emscripten_asm_const_int(i32 %arg)
  call void (...) @emscripten_asm_const_double(i32 %arg)
  call void (...) @emscripten_landingpad(i32 %arg)
  call void (...) @emscripten_resume(i32 %arg)
  ret void
}
; CHECK-LABEL: define void @test(
; CHECK-NEXT: call void (...) @emscripten_asm_const_int(i32 %arg)
; CHECK-NEXT: call void (...) @emscripten_asm_const_double(i32 %arg)
; CHECK-NEXT: call void (...) @emscripten_landingpad(i32 %arg)
; CHECK-NEXT: call void (...) @emscripten_resume(i32 %arg)
; CHECK-NEXT: ret void
