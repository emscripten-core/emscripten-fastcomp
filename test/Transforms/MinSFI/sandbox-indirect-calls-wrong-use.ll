; RUN: not opt %s -minsfi-sandbox-indirect-calls -S 2>&1 | FileCheck %s

target datalayout = "p:32:32:32"
target triple = "le32-unknown-nacl"

declare i32 @foo(i32)

define i32 @bar() {
  %fn = bitcast i32 (i32)* @foo to i32 ()*
  %ret = call i32 %fn()
  ret i32 %ret
}

; CHECK: Invalid reference to function @foo
