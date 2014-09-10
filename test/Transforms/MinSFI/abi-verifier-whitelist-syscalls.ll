; RUN: opt %s -verify-pnaclabi-module -pnaclabi-allow-minsfi-syscalls -S \
; RUN:   | FileCheck -check-prefix=CHECK-WITHFLAG %s
; RUN: not opt %s -verify-pnaclabi-module -S 2>&1 \
; RUN:   | FileCheck -check-prefix=CHECK-NOFLAG %s

target datalayout = "p:32:32:32"
target triple = "le32-unknown-nacl"

define i32 @_start(i32 %args) {
  ret i32 0
}

declare i32 @__minsfi_syscall_dummy(i32)

; CHECK-NOFLAG: __minsfi_syscall_dummy is declared but not defined
; CHECK-NOFLAG: __minsfi_syscall_dummy is not a valid external symbol

; CHECK-WITHFLAG: declare i32 @__minsfi_syscall_dummy(i32)
