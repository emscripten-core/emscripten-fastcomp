; RUN: opt %s -minsfi-allocate-data-segment -S | FileCheck %s

target datalayout = "p:32:32:32"
target triple = "le32-unknown-nacl"

define void @foo() {
  ret void
}

; CHECK: %__sfi_data_segment = type <{}>
; CHECK: @__sfi_data_segment = constant %__sfi_data_segment zeroinitializer
; CHECK: @__sfi_data_segment_size = constant i32 0 
