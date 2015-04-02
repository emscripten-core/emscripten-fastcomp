; RUN: opt %s -cleanup-used-globals-metadata  -S | FileCheck %s

target datalayout = "e-p:32:32-i64:64"
target triple = "le32-unknown-nacl"

@llvm.used = appending global [1 x i8*] [i8* bitcast (void ()* @foo to i8*)], section "llvm.metadata"
; The used list is removed.
; CHECK-NOT: @llvm.used


define internal void @foo() #0 {
  ret void
}
; The global (@foo) is still present.
; CHECK-LABEL: define internal void @foo
