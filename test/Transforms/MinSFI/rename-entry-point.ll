; RUN: opt %s -minsfi-rename-entry-point -S | FileCheck %s

target datalayout = "p:32:32:32"
target triple = "le32-unknown-nacl"

define i32 @_start() {
  ret i32 0
}

; CHECK-LABEL: define i32 @_start_minsfi() {
; CHECK-NEXT:    ret i32 0
; CHECK-NEXT:  }
