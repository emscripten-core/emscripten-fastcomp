; RUN: opt < %s -nacl-expand-tls -S | FileCheck %s


@tvar_bss1 = thread_local global i64 0
@tvar_bss2 = thread_local global i32 0


; CHECK: %tls_struct = type <{ %tls_init_template, %tls_bss_template }>
; CHECK: %tls_bss_template = type <{ i64, i32, [4 x i8] }>


define i64* @get_tvar_bss1() {
  ret i64* @tvar_bss1
}
; CHECK: define i64* @get_tvar_bss1()
; CHECK: %field = getelementptr %tls_struct, %tls_struct* %tls_struct, i32 -1, i32 1, i32 0
; CHECK: ret i64* %field
