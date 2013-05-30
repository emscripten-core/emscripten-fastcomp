; RUN: pnacl-abicheck < %s | FileCheck %s

define void @arg_i1(i1 %bad) {
  ret void
}
; CHECK: Function arg_i1 has disallowed type:

define void @arg_i16(i32 %allowed, i16 %bad) {
  ret void
}
; CHECK: Function arg_i16 has disallowed type:

define i1 @return_i1() {
  ret i1 0
}
; CHECK: Function return_i1 has disallowed type:

define i8 @return_i8() {
  ret i8 0
}
; CHECK: Function return_i8 has disallowed type:


; Direct calls currently do not produce errors because the functions
; are deemed to have already been flagged.
; CHECK-NOT: disallowed
define void @bad_direct_calls() {
  call void @arg_i1(i1 0)
  call void @arg_i16(i32 0, i16 0)
  %result1 = call i1 @return_i1()
  %result2 = call i8 @return_i8()
  ret void
}

define void @bad_indirect_calls(i32 %ptr) {
  %func1 = inttoptr i32 %ptr to void (i8)*
  call void %func1(i8 0)
; CHECK: Function bad_indirect_calls has instruction with disallowed type: void (i8)*

  %func2 = inttoptr i32 %ptr to i16 ()*
  %result3 = call i16 %func2()
; CHECK: Function bad_indirect_calls has instruction with disallowed type: i16 ()*

  ret void
}
