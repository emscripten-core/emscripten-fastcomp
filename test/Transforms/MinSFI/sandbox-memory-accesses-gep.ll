; RUN: opt %s -expand-getelementptr -replace-ptrs-with-ints \
; RUN:        -minsfi-sandbox-memory-accesses -S | FileCheck %s

target datalayout = "p:32:32:32"
target triple = "le32-unknown-nacl"

; This test verifies that the pass recognizes the pointer arithmetic pattern
; produced by the ExpandGetElementPtr pass and that it emits a more efficient
; address sandboxing than in the general case.

define i32 @test_load_elementptr([100 x i32]* %foo) {
  %elem = getelementptr inbounds [100 x i32]* %foo, i32 0, i32 97
  %val = load i32* %elem
  ret i32 %val
}

; CHECK-LABEL: define i32 @test_load_elementptr(i32 %foo) {
; CHECK-NEXT:    %mem_base = load i64* @__sfi_memory_base
; CHECK-NEXT:    %1 = zext i32 %foo to i64
; CHECK-NEXT:    %2 = add i64 %mem_base, %1
; CHECK-NEXT:    %3 = add i64 %2, 388
; CHECK-NEXT:    %4 = inttoptr i64 %3 to i32*
; CHECK-NEXT:    %val = load i32* %4
; CHECK-NEXT:    ret i32 %val
; CHECK-NEXT:  }
