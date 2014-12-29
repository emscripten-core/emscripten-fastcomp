; RUN: not opt %s -minsfi-rename-entry-point -S 2>&1 | FileCheck %s

target datalayout = "p:32:32:32"
target triple = "le32-unknown-nacl"

@_start_minsfi = constant i32 1234

define i32 @_start() {
  ret i32 0
}

; CHECK: RenameEntryPoint: The module already contains a value named '_start_minsfi'
