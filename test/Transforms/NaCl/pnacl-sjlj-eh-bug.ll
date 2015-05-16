; RUN: opt %s -pnacl-sjlj-eh -O2 -S | FileCheck %s

; datalayout must be specified for GVN to work.
target datalayout = "p:32:32:32"

; This must be declared for expanding "invoke" and "landingpad" instructions.
@__pnacl_eh_stack = external thread_local global i8*

declare i1 @might_be_setjmp()
declare void @external_func(i32* %ptr)
declare void @var_is_nonzero()


; Test for a bug in which PNaClSjLjEH would transform
; @invoke_optimize_test() such that the call to @var_is_nonzero()
; could get optimized away by a later optimization pass.  This
; happened because PNaClSjLjEH generated code similar to
; @branch_optimize_test() below.

define void @invoke_optimize_test() {
  %var = alloca i32
  store i32 0, i32* %var

  invoke void @external_func(i32* %var)
      to label %exit unwind label %lpad

lpad:
  landingpad i32 personality i8* null
      catch i8* null
  %value = load i32, i32* %var
  %is_zero = icmp eq i32 %value, 0
  br i1 %is_zero, label %exit, label %do_call

do_call:
  call void @var_is_nonzero()
  ret void

exit:
  ret void
}
; CHECK: define void @invoke_optimize_test()
; CHECK: @var_is_nonzero()


; In @branch_optimize_test(), the optimizer can optimize away the call
; to @var_is_nonzero(), because it can assume that %var always
; contains 0 on the "iffalse" branch.
;
; The passes "-gvn -instcombine" are enough to do this.
;
; The optimizer can do this regardless of whether @might_be_setjmp()
; is setjmp() or a normal function.  It doesn't need to know that
; @might_be_setjmp() might return twice, because storing to %var
; between setjmp() and longjmp() leaves %var pointing to an undefined
; value.

define void @branch_optimize_test() {
  %var = alloca i32
  store i32 0, i32* %var

  %cond = call i1 @might_be_setjmp() returns_twice
  br i1 %cond, label %iftrue, label %iffalse

iftrue:
  call void @external_func(i32* %var)
  ret void

iffalse:
  %value = load i32, i32* %var
  %is_zero = icmp eq i32 %value, 0
  br i1 %is_zero, label %exit, label %do_call

do_call:
  call void @var_is_nonzero()
  ret void

exit:
  ret void
}
; CHECK: define void @branch_optimize_test()
; CHECK-NOT: @var_is_nonzero
