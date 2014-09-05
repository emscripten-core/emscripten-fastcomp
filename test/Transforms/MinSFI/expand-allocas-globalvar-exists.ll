; RUN: opt %s -minsfi-expand-allocas -S | FileCheck %s

target datalayout = "p:32:32:32"
target triple = "le32-unknown-nacl"

@__sfi_stack_ptr = external global i32

; CHECK: @__sfi_stack_ptr1 = internal global i32

define i8* @test_correct_global_var_used() {
  %ptr = alloca i8
  ret i8* %ptr
}

; CHECK-LABEL: define i8* @test_correct_global_var_used() {
; CHECK-NEXT:    %frame_top = load i32* @__sfi_stack_ptr1
; CHECK-NEXT:    %1 = sub i32 %frame_top, 1
; CHECK-NEXT:    %ptr = inttoptr i32 %1 to i8*
; CHECK-NEXT:    ret i8* %ptr
; CHECK-NEXT:  }

define i32 @_start_minsfi(i32 %args) {
  ret i32 0
}
