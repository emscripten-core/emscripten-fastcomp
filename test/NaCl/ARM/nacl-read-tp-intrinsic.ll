
; RUN: pnacl-llc -mtriple=armv7-unknown-nacl -sfi-store -filetype=asm %s -o - \
; RUN:   | FileCheck -check-prefix=ARM %s

; RUN: pnacl-llc -mtriple=armv7-unknown-nacl -sfi-store -filetype=asm -mtls-use-call %s -o - \
; RUN:   | FileCheck -check-prefix=ARM_IRT %s


declare i8* @llvm.nacl.read.tp()

define i8* @get_thread_pointer() {
  %tp = call i8* @llvm.nacl.read.tp()
  ret i8* %tp
}

; ARM: get_thread_pointer:
; ARM: ldr r0, [r9]

; ARM_IRT: get_thread_pointer:
; ARM_IRT: bl __aeabi_read_tp
