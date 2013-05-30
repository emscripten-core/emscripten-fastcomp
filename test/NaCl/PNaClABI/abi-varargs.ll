; RUN: pnacl-abicheck < %s | FileCheck %s

define void @varargs_func(i32 %arg, ...) {
  ret void
}
; CHECK: Function varargs_func has disallowed type: void (i32, ...)

define void @call_varargs_func(i32 %ptr) {
  %ptr2 = inttoptr i32 %ptr to void (i32, ...)*
  call void (i32, ...)* %ptr2(i32 123)
  ret void
}
; CHECK: Function call_varargs_func has instruction with disallowed type: void (i32, ...)*
