; RUN: opt %s -nacl-promote-ints -S | FileCheck %s

target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:32"

%struct.S0 = type { i24, i32 }
declare i32 @__gxx_personality_v0(...)

declare i13 @ext_fct(i16, i24, i32)
; CHECK-LABEL: declare i16 @ext_fct(i16, i32, i32)

define internal i16 @func(i32 %x, i24 %y, i32 %z) {
  %lo = lshr i24 %y, 8
  %lo.tk = trunc i24 %lo to i16
  ret i16 %lo.tk
}
; CHECK-LABEL: define internal i16 @func(i32 %x, i32 %y, i32 %z)
; CHECK-NEXT: %y.clear = and i32 %y, 16777215
; CHECK-NEXT: %lo = lshr i32 %y.clear, 8
; CHECK-NEXT: %lo.tk = trunc i32 %lo to i16
; CHECK-NEXT: ret i16 %lo.tk


define void @invoke_example(i16 %x, i24 %y, i32 %z) {
entry:
  %tmp2 = invoke i13 @ext_fct(i16 %x, i24 %y, i32 %z)
    to label %Cont unwind label %Cleanup
Cont:
    ret void
Cleanup:
  %exn = landingpad i13 personality i32 (...)* @__gxx_personality_v0
    cleanup
  resume i13 %exn
}
; CHECK-LABEL: define void @invoke_example(i16 %x, i32 %y, i32 %z)
; CHECK-DAG: %tmp2 = invoke i16 @ext_fct(i16 %x, i32 %y, i32 %z)
; CHECK-DAG: %exn = landingpad i16 personality i32 (...)* @__gxx_personality_v0
; CHECK-DAG: resume i16 %exn

define i9 @a_func(i32 %x, i9* %y, i9 %z) {
  ret i9 %z
}
; CHECK-LABEL: define i16 @a_func(i32 %x, i9* %y, i16 %z)
; CHECK-NEXT: ret i16 %z

define i9 @applying_fct(i9* %x, i9 %y) {
  %ret = call i9 @applicator(i9 (i32, i9*, i9)* @a_func, i9* %x, i9 %y)
  ret i9 %ret
}
; CHECK-LABEL: define i16 @applying_fct(i9* %x, i16 %y)
; CHECK-NEXT: call i16 @applicator(i16 (i32, i9*, i16)* @a_func, i9* %x, i16 %y)
; CHECK-NEXT: ret i16

define i9 @applicator(i9 (i32, i9*, i9)* %fct, i9* %ptr, i9 %val) {
  %ret = call i9 %fct(i32 0, i9* %ptr, i9 %val)
; CHECK: call i16 %fct(i32 0, i9* %ptr, i16 %val)
  ret i9 %ret
}

define i9 @plain_call(i9* %ptr, i9 %val) {
  %ret = call i9 @applying_fct(i9* %ptr, i9 %val)
; CHECK: call i16 @applying_fct(i9* %ptr, i16 %val)
  ret i9 %ret
}