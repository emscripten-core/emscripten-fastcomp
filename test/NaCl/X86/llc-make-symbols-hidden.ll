; RUN: pnacl-llc -mtriple=i686-unknown-nacl -filetype=asm %s -o - \
; RUN:   -relocation-model=pic | FileCheck %s

; This should have "hidden" automatically added to it.
declare void @callee()

define void @caller() {
  tail call void @callee()
  ret void
}
; CHECK-LABEL: caller
; As we automatically make callee hidden, we should see neither an
; access to GOT nor jmp instruction to PLT.
; CHECK-NOT: _GLOBAL_OFFSET_TABLE_
; CHECK-NOT: jmp callee@PLT
; CHECK: jmp callee

; This should have "hidden" automatically added to it.
@tls_var = external thread_local global i32

define i32* @get_tls_addr() {
  ret i32* @tls_var
}
; CHECK-LABEL: get_tls_addr
; There must be no general dynamic TLS.
; CHECK-NOT: @TLSGD
; CHECK: tls_var@TLSLDM
