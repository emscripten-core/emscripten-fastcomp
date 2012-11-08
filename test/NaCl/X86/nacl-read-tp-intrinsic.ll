
; RUN: llc -mtriple=i386-unknown-nacl -filetype=obj %s -o - \
; RUN:   | llvm-objdump -disassemble r -triple i386 - \
; RUN:   | FileCheck -check-prefix=X32 %s

; RUN: llc -mtriple=i386-unknown-nacl -filetype=obj -mtls-use-call %s -o - \
; RUN:   | llvm-objdump -disassemble -r -triple i386 - \
; RUN:   | FileCheck -check-prefix=X32_IRT %s

; RUN: llc -mtriple=x86_64-unknown-nacl -filetype=obj %s -o - \
; RUN:   | llvm-objdump -disassemble -r -triple x86_64 - \
; RUN:   | FileCheck -check-prefix=X64 %s

; "-mtls-use-call" should not make any difference on x86-64.
; RUN: llc -mtriple=x86_64-unknown-nacl -filetype=obj -mtls-use-call %s -o - \
; RUN:   | llvm-objdump -disassemble -r -triple x86_64 - \
; RUN:   | FileCheck -check-prefix=X64 %s


declare i8* @llvm.nacl.read.tp()

define i8* @get_thread_pointer() {
  %tp = call i8* @llvm.nacl.read.tp()
  ret i8* %tp
}

; X32: movl %gs:0, %eax

; There appears to be a bug in llvm-objdump which stops it from
; showing the symbol name "__nacl_read_tp" in the relocation output on
; x86-32.
; X32_IRT: call
; X32_IRT-NEXT: R_386_PC32 Unknown

; X64: call
; X64-NEXT: __nacl_read_tp
