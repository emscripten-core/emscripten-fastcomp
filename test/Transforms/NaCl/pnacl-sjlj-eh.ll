; RUN: opt %s -pnacl-sjlj-eh -S | FileCheck %s

; This must be declared for expanding "invoke" and "landingpad" instructions.
@__pnacl_eh_stack = external thread_local global i8*

; This must be declared for expanding "resume" instructions.
declare void @__pnacl_eh_resume(i32* %exception)

declare i32 @external_func(i64 %arg)
declare void @external_func_void()
declare i32 @my_setjmp()


; CHECK: %ExceptionFrame = type { [1024 x i8], %ExceptionFrame*, i32 }

define i32 @invoke_test(i64 %arg) {
  %result = invoke i32 @external_func(i64 %arg)
      to label %cont unwind label %lpad
cont:
  ret i32 %result
lpad:
  %lp = landingpad { i8*, i32 } personality i8* null cleanup
  ret i32 999
}
; CHECK: define i32 @invoke_test
; CHECK-NEXT: %invoke_result_ptr = alloca i32
; CHECK-NEXT: %invoke_frame = alloca %ExceptionFrame, align 8
; CHECK-NEXT: %exc_info_ptr = getelementptr %ExceptionFrame, %ExceptionFrame* %invoke_frame, i32 0, i32 2
; CHECK-NEXT: %invoke_next = getelementptr %ExceptionFrame, %ExceptionFrame* %invoke_frame, i32 0, i32 1
; CHECK-NEXT: %invoke_jmp_buf = getelementptr %ExceptionFrame, %ExceptionFrame* %invoke_frame, i32 0, i32 0, i32 0
; CHECK-NEXT: %pnacl_eh_stack = bitcast i8** @__pnacl_eh_stack to %ExceptionFrame**
; CHECK-NEXT: %old_eh_stack = load %ExceptionFrame*, %ExceptionFrame** %pnacl_eh_stack
; CHECK-NEXT: store %ExceptionFrame* %old_eh_stack, %ExceptionFrame** %invoke_next
; CHECK-NEXT: store i32 {{[0-9]+}}, i32* %exc_info_ptr
; CHECK-NEXT: store %ExceptionFrame* %invoke_frame, %ExceptionFrame** %pnacl_eh_stack
; CHECK-NEXT: %invoke_is_exc = call i32 @invoke_test_setjmp_caller(i64 %arg, i32 (i64)* @external_func, i8* %invoke_jmp_buf, i32* %invoke_result_ptr)
; CHECK-NEXT: %result = load i32, i32* %invoke_result_ptr
; CHECK-NEXT: store %ExceptionFrame* %old_eh_stack, %ExceptionFrame** %pnacl_eh_stack
; CHECK-NEXT: %invoke_sj_is_zero = icmp eq i32 %invoke_is_exc, 0
; CHECK-NEXT: br i1 %invoke_sj_is_zero, label %cont, label %lpad
; CHECK: cont:
; CHECK-NEXT: ret i32 %result
; CHECK: lpad:
; CHECK-NEXT: %landingpad_ptr = bitcast i8* %invoke_jmp_buf to { i8*, i32 }*
; CHECK-NEXT: %lp = load { i8*, i32 }, { i8*, i32 }* %landingpad_ptr
; CHECK-NEXT: ret i32 999

; Check definition of helper function:
; CHECK: define internal i32 @invoke_test_setjmp_caller(i64 %arg, i32 (i64)* %func_ptr, i8* %jmp_buf, i32* %result_ptr) {
; CHECK-NEXT: %invoke_sj = call i32 @llvm.nacl.setjmp(i8* %jmp_buf) [[RETURNS_TWICE:#[0-9]+]]
; CHECK-NEXT: %invoke_sj_is_zero = icmp eq i32 %invoke_sj, 0
; CHECK-NEXT: br i1 %invoke_sj_is_zero, label %normal, label %exception
; CHECK: normal:
; CHECK-NEXT: %result = call i32 %func_ptr(i64 %arg)
; CHECK-NEXT: store i32 %result, i32* %result_ptr
; CHECK-NEXT: ret i32 0
; CHECK: exception:
; CHECK-NEXT: ret i32 1


; A landingpad block may be used by multiple "invoke" instructions.
define i32 @shared_landingpad(i64 %arg) {
  %result1 = invoke i32 @external_func(i64 %arg)
      to label %cont1 unwind label %lpad
cont1:
  %result2 = invoke i32 @external_func(i64 %arg)
      to label %cont2 unwind label %lpad
cont2:
  ret i32 %result2
lpad:
  %lp = landingpad { i8*, i32 } personality i8* null cleanup
  ret i32 999
}
; CHECK: define i32 @shared_landingpad
; CHECK: br i1 %invoke_sj_is_zero{{[0-9]*}}, label %cont1, label %lpad
; CHECK: br i1 %invoke_sj_is_zero{{[0-9]*}}, label %cont2, label %lpad


; Check that the pass can handle a landingpad appearing before an invoke.
define i32 @landingpad_before_invoke() {
  ret i32 123

dead_block:
  %lp = landingpad i32 personality i8* null cleanup
  ret i32 %lp
}
; CHECK: define i32 @landingpad_before_invoke
; CHECK: %lp = load i32, i32* %landingpad_ptr


; Test the expansion of the "resume" instruction.
define void @test_resume({ i8*, i32 } %arg) {
  resume { i8*, i32 } %arg
}
; CHECK: define void @test_resume
; CHECK-NEXT: %resume_exc = extractvalue { i8*, i32 } %arg, 0
; CHECK-NEXT: %resume_cast = bitcast i8* %resume_exc to i32*
; CHECK-NEXT: call void @__pnacl_eh_resume(i32* %resume_cast)
; CHECK-NEXT: unreachable


; Check that call attributes are preserved.
define i32 @call_attrs(i64 %arg) {
  %result = invoke fastcc i32 @external_func(i64 inreg %arg) noreturn
      to label %cont unwind label %lpad
cont:
  ret i32 %result
lpad:
  %lp = landingpad { i8*, i32 } personality i8* null cleanup
  ret i32 999
}
; CHECK: define i32 @call_attrs
; CHECK: %result = call fastcc i32 %func_ptr(i64 inreg %arg) [[NORETURN:#[0-9]+]]


; If the PNaClSjLjEH pass needs to insert any instructions into the
; non-exceptional path, check that PHI nodes are updated correctly.
; (An earlier version needed to do this, but the current version
; doesn't.)
define i32 @invoke_with_phi_nodes(i64 %arg) {
entry:
  %result = invoke i32 @external_func(i64 %arg)
      to label %cont unwind label %lpad
cont:
  %cont_phi = phi i32 [ 100, %entry ]
  ret i32 %cont_phi
lpad:
  %lpad_phi = phi i32 [ 200, %entry ]
  %lp = landingpad { i8*, i32 } personality i8* null cleanup
  ret i32 %lpad_phi
}
; CHECK: define i32 @invoke_with_phi_nodes
; CHECK: cont:
; CHECK-NEXT: %cont_phi = phi i32 [ 100, %entry ]
; CHECK-NEXT: ret i32 %cont_phi
; CHECK: lpad:
; CHECK-NEXT: %lpad_phi = phi i32 [ 200, %entry ]
; CHECK: ret i32 %lpad_phi


; Test "void" result type from "invoke".  This requires special
; handling because void* is not a valid type.
define void @invoke_void_result() {
  invoke void @external_func_void() to label %cont unwind label %lpad
cont:
  ret void
lpad:
  landingpad i32 personality i8* null cleanup
  ret void
}
; CHECK: define void @invoke_void_result()
; "%result_ptr" argument is omitted from the helper function:
; CHECK: define internal i32 @invoke_void_result_setjmp_caller(void ()* %func_ptr, i8* %jmp_buf)


; A call to setjmp() cannot be moved into a helper function, so test
; that it isn't moved.
define void @invoke_setjmp() {
  %x = invoke i32 @my_setjmp() returns_twice to label %cont unwind label %lpad
cont:
  ret void
lpad:
  landingpad i32 personality i8* null cleanup
  ret void
}
; CHECK: define void @invoke_setjmp()
; CHECK-NOT: call
; CHECK: %x = call i32 @my_setjmp() [[RETURNS_TWICE]]
; CHECK-NEXT: br label %cont


; CHECK: attributes [[RETURNS_TWICE]] = { returns_twice }
; CHECK: attributes [[NORETURN]] = { noreturn }
