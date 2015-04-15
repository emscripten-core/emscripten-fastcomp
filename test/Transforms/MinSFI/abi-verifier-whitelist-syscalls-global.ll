; RUN: not opt %s -verify-pnaclabi-module -pnaclabi-allow-minsfi-syscalls -S \
; RUN:   2>&1 | FileCheck %s

target datalayout = "p:32:32:32"
target triple = "le32-unknown-nacl"

define i32 @_start(i32 %args) {
  ret i32 0
}

@__minsfi_syscall_dummy = global [4 x i8] c"DATA"

; CHECK: __minsfi_syscall_dummy is not a valid external symbol
