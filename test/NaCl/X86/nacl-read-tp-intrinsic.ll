; RUN: pnacl-llc -mtriple=i386-unknown-nacl -filetype=asm %s -o - \
; RUN:   | FileCheck -check-prefix=X32 %s

; RUN: pnacl-llc -mtriple=i386-unknown-nacl -filetype=asm -mtls-use-call %s -o - \
; RUN:   | FileCheck -check-prefix=USE_CALL %s

; RUN: pnacl-llc -mtriple=x86_64-unknown-nacl -filetype=asm %s -o - \
; RUN:   | FileCheck -check-prefix=USE_CALL %s

; "-mtls-use-call" should not make any difference on x86-64.
; RUN: pnacl-llc -mtriple=x86_64-unknown-nacl -filetype=asm -mtls-use-call %s -o - \
; RUN:   | FileCheck -check-prefix=USE_CALL %s


declare i8* @llvm.nacl.read.tp()

define i8* @get_thread_pointer() {
  %tp = call i8* @llvm.nacl.read.tp()
  ret i8* %tp
}

; X32: get_thread_pointer:
; X32: movl %gs:0, %eax

; USE_CALL: get_thread_pointer:
; USE_CALL: naclcall __nacl_read_tp


; Make sure that we do not generate:
;   movl $1000, %eax
;   addl %gs:0, %eax
; The x86-32 NaCl validator only accepts %gs with "mov", not with
; "add".  Note that we had to use a large immediate to trigger the bug
; and generate the code above.
define i8* @get_thread_pointer_add() {
  %tp = call i8* @llvm.nacl.read.tp()
  %result = getelementptr i8* %tp, i32 1000
  ret i8* %result
}

; X32: get_thread_pointer_add:
; X32: movl %gs:0, %eax
; X32: addl $1000, %eax
