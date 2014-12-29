; RUN: not opt %s -minsfi-expand-allocas -S 2>&1 | FileCheck %s

target datalayout = "p:32:32:32"
target triple = "le32-unknown-nacl"

define i32 @foo(i32 %arg1) {
  ret i32 0
}

; CHECK: ExpandAllocas: Module does not have an entry function
