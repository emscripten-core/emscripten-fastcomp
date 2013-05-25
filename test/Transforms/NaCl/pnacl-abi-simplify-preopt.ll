; RUN: opt %s -pnacl-abi-simplify-preopt -S | FileCheck %s

; "-pnacl-abi-simplify-preopt" runs various passes which are tested
; thoroughly in other *.ll files.  This file is a smoke test to check
; that "-pnacl-abi-simplify-preopt" runs what it's supposed to run.

declare void @ext_func()


define void @invoke_func() {
  invoke void @ext_func() to label %cont unwind label %lpad
cont:
  ret void
lpad:
  %lp = landingpad { i8*, i32 } personality i8* null cleanup
  ret void
}
; CHECK-NOT: invoke void @ext_func()
; CHECK-NOT: landingpad


define void @varargs_func(...) {
  ret void
}
; CHECK-NOT: @varargs_func(...)


@llvm.global_ctors = appending global [0 x { i32, void ()* }] zeroinitializer
; CHECK-NOT: @llvm.global_ctors

@tls_var = thread_local global i32 0
; CHECK-NOT: thread_local

@alias = alias i32* @tls_var
; CHECK-NOT: @alias

@weak_ref = extern_weak global i8*
; CHECK-NOT: extern_weak
