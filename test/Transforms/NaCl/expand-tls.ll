; RUN: opt < %s -nacl-expand-tls -S | FileCheck %s

; All thread-local variables should be removed
; RUN: opt < %s -nacl-expand-tls -S | FileCheck %s -check-prefix=NO_TLS

; NO_TLS-NOT: thread_local

@tvar1 = thread_local global i64 123
@tvar2 = thread_local global i32 456


; CHECK: %tls_init_template = type <{ i64, i32 }>
; CHECK: %tls_struct = type <{ %tls_init_template, %tls_bss_template }>
; CHECK: %tls_bss_template = type <{ [4 x i8] }>


; CHECK: @__tls_template_start = internal constant %tls_init_template <{ i64 123, i32 456 }>

; CHECK: @__tls_template_alignment = internal constant i32 8


define i64* @get_tvar1() {
  ret i64* @tvar1
}
; CHECK: define i64* @get_tvar1()
; CHECK: %tls_raw = call i8* @llvm.nacl.read.tp()
; CHECK: %tls_struct = bitcast i8* %tls_raw to %tls_struct*
; CHECK: %field = getelementptr %tls_struct, %tls_struct* %tls_struct, i32 -1, i32 0, i32 0
; CHECK: ret i64* %field


define i32* @get_tvar2() {
  ret i32* @tvar2
}
; Much the same as for get_tvar1.
; CHECK: define i32* @get_tvar2()
; CHECK: %field = getelementptr %tls_struct, %tls_struct* %tls_struct, i32 -1, i32 0, i32 1


; Check that we define global variables for TLS templates

@__tls_template_start = external global i8
@__tls_template_tdata_end = external global i8
@__tls_template_end = external global i8

define i8* @get_tls_template_start() {
  ret i8* @__tls_template_start
}
; CHECK: define i8* @get_tls_template_start()
; CHECK: ret i8* bitcast (%tls_init_template* @__tls_template_start to i8*)

define i8* @get_tls_template_tdata_end() {
  ret i8* @__tls_template_tdata_end
}
; CHECK: define i8* @get_tls_template_tdata_end()
; CHECK: ret i8* bitcast (%tls_init_template* getelementptr inbounds (%tls_init_template, %tls_init_template* @__tls_template_start, i32 1) to i8*)

define i8* @get_tls_template_end() {
  ret i8* @__tls_template_end
}
; CHECK: define i8* @get_tls_template_end()
; CHECK: ret i8* bitcast (%tls_struct* getelementptr (%tls_struct, %tls_struct* bitcast (%tls_init_template* @__tls_template_start to %tls_struct*), i32 1) to i8*)


; Check that we define the TLS layout functions

declare i32 @__nacl_tp_tls_offset(i32)
declare i32 @__nacl_tp_tdb_offset(i32)

define i32 @test_get_tp_tls_offset(i32 %tls_size) {
  %offset = call i32 @__nacl_tp_tls_offset(i32 %tls_size)
  ret i32 %offset
}
; Uses of the intrinsic are replaced with uses of a regular function.
; CHECK: define i32 @test_get_tp_tls_offset
; CHECK: call i32 @nacl_tp_tls_offset
; NO_TLS-NOT: __nacl_tp_tls_offset

define i32 @test_get_tp_tdb_offset(i32 %tdb_size) {
  %offset = call i32 @__nacl_tp_tdb_offset(i32 %tdb_size)
  ret i32 %offset
}
; Uses of the intrinsic are replaced with uses of a regular function.
; CHECK: define i32 @test_get_tp_tdb_offset
; CHECK: call i32 @nacl_tp_tdb_offset
; NO_TLS-NOT: __nacl_tp_tdb_offset
