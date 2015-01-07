; RUN: not opt %s -minsfi-sandbox-indirect-calls -S 2>&1 | FileCheck %s

target datalayout = "p:32:32:32"
target triple = "le32-unknown-nacl"

declare void @fn()

declare i32 @foo(void ()*)

define i32 @bar() {
  %ret = call i32 @foo(void ()* @fn)
  ret i32 %ret
}

; CHECK: Invalid reference to function @fn
