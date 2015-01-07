; RUN: not opt %s -minsfi-rename-entry-point -S 2>&1 | FileCheck %s

target datalayout = "p:32:32:32"
target triple = "le32-unknown-nacl"

define i32 @foo() {
  ret i32 0
}

; CHECK: RenameEntryPoint: The module does not contain a function named '_start'
