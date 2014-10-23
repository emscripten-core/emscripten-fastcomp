; RUN: opt %s -minsfi-strip-tls -S | FileCheck %s

target datalayout = "p:32:32:32"
target triple = "le32-unknown-nacl"

@internal_shared = internal global i32 111
@internal_thread_local = internal thread_local global i32 222 

@external_shared = external global i64
@external_thread_local = external thread_local global i64 

@other_attr = thread_local addrspace(5) constant float 1.000000e+00, 
              section "foo", align 4
@tls_model = thread_local(initialexec) global i32 555

; CHECK: @internal_shared = internal global i32 111
; CHECK: @internal_thread_local = internal global i32 222 

; CHECK: @external_shared = external global i64
; CHECK: @external_thread_local = external global i64

; CHECK: @other_attr = addrspace(5) constant float 1.000000e+00, section "foo", align 4
; CHECK: @tls_model = global i32 555
