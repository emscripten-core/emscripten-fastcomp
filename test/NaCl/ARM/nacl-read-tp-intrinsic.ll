
; RUN: llc -mtriple=armv7-unknown-nacl -sfi-store -filetype=obj %s -o - \
; RUN:   | llvm-objdump -disassemble -r -triple armv7 - \
; RUN:   | FileCheck -check-prefix=ARM %s

; RUN: llc -mtriple=armv7-unknown-nacl -sfi-store -filetype=obj -mtls-use-call %s -o - \
; RUN:   | llvm-objdump -disassemble -r -triple armv7 - \
; RUN:   | FileCheck -check-prefix=ARM_IRT %s


declare i8* @llvm.nacl.read.tp()

define i8* @get_thread_pointer() {
  %tp = call i8* @llvm.nacl.read.tp()
  ret i8* %tp
}

; ARM: ldr r0, [r9]

; ARM_IRT: bl #
; ARM_IRT-NEXT: __aeabi_read_tp
