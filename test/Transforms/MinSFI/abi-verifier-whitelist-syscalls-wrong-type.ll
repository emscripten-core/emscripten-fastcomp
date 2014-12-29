; RUN: not opt %s -verify-pnaclabi-module -pnaclabi-allow-minsfi-syscalls -S \
; RUN:   2>&1 | FileCheck %s

target datalayout = "p:32:32:32"
target triple = "le32-unknown-nacl"

define void @_start() {
  ret void
}

declare i32 @__minsfi_syscall_good()
; CHECK-NOT: __minsfi_syscall_good is not a valid external symbol

declare void @__minsfi_syscall_bad_void()
; CHECK: __minsfi_syscall_bad_void is not a valid external symbol

declare i64 @__minsfi_syscall_bad_i64()
; CHECK: __minsfi_syscall_bad_i64 is not a valid external symbol

declare float @__minsfi_syscall_bad_float()
; CHECK: __minsfi_syscall_bad_float is not a valid external symbol
