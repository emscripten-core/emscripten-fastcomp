; RUN: opt < %s -simplify-allocas -S | FileCheck %s

target datalayout = "p:32:32:32"

%struct = type { i32, i32 }

declare void @receive_alloca(%struct* %ptr)
declare void @receive_vector_alloca(<4 x i32>* %ptr)

define void @alloca_fixed() {
  %buf = alloca %struct, align 128
  call void @receive_alloca(%struct* %buf)
  ret void
}
; CHECK-LABEL: define void @alloca_fixed() {
; CHECK-NEXT:    %buf = alloca i8, i32 8, align 128
; CHECK-NEXT:    %buf.bc = bitcast i8* %buf to %struct*
; CHECK-NEXT:    call void @receive_alloca(%struct* %buf.bc)

; When the size passed to alloca is a constant, it should be a
; constant in the output too.
define void @alloca_fixed_array() {
  %buf = alloca %struct, i32 100
  call void @receive_alloca(%struct* %buf)
  ret void
}
; CHECK-LABEL: define void @alloca_fixed_array() {
; CHECK-NEXT:    %buf = alloca i8, i32 800, align 8
; CHECK-NEXT:    %buf.bc = bitcast i8* %buf to %struct*
; CHECK-NEXT:    call void @receive_alloca(%struct* %buf.bc)

define void @alloca_fixed_vector() {
  %buf = alloca <4 x i32>, align 128
  call void @receive_vector_alloca(<4 x i32>* %buf)
  ret void
}
; CHECK-LABEL: define void @alloca_fixed_vector() {
; CHECK-NEXT: %buf = alloca i8, i32 16, align 128
; CHECK-NEXT: %buf.bc = bitcast i8* %buf to <4 x i32>*
; CHECK-NEXT: call void @receive_vector_alloca(<4 x i32>* %buf.bc)

define void @alloca_variable(i32 %size) {
  %buf = alloca %struct, i32 %size
  call void @receive_alloca(%struct* %buf)
  ret void
}
; CHECK-LABEL: define void @alloca_variable(i32 %size) {
; CHECK-NEXT:    %buf.alloca_mul = mul i32 8, %size
; CHECK-NEXT:    %buf = alloca i8, i32 %buf.alloca_mul
; CHECK-NEXT:    %buf.bc = bitcast i8* %buf to %struct*
; CHECK-NEXT:    call void @receive_alloca(%struct* %buf.bc)

define void @alloca_alignment_i32() {
  %buf = alloca i32
  ret void
}
; CHECK-LABEL: void @alloca_alignment_i32() {
; CHECK-NEXT:    alloca i8, i32 4, align 4

define void @alloca_alignment_double() {
  %buf = alloca double
  ret void
}
; CHECK-LABEL: void @alloca_alignment_double() {
; CHECK-NEXT:    alloca i8, i32 8, align 8

define void @alloca_lower_alignment() {
  %buf = alloca i32, align 1
  ret void
}
; CHECK-LABEL: void @alloca_lower_alignment() {
; CHECK-NEXT:    alloca i8, i32 4, align 1

define void @alloca_array_trunc() {
  %a = alloca i32, i64 1024
  unreachable
}
; CHECK-LABEL: define void @alloca_array_trunc()
; CHECK-NEXT:    %a = alloca i8, i32 4096

define void @alloca_array_zext() {
  %a = alloca i32, i8 128
  unreachable
}
; CHECK-LABEL: define void @alloca_array_zext()
; CHECK-NEXT:    %a = alloca i8, i32 512

define void @dyn_alloca_array_trunc(i64 %a) {
  %b = alloca i32, i64 %a
  unreachable
}
; CHECK-LABEL: define void @dyn_alloca_array_trunc(i64 %a)
; CHECK-NEXT:    trunc i64 %a to i32
; CHECK-NEXT:    mul i32 4,
; CHECK-NEXT:    alloca i8, i32

define void @dyn_alloca_array_zext(i8 %a) {
  %b = alloca i32, i8 %a
  unreachable
}
; CHECK-LABEL: define void @dyn_alloca_array_zext(i8 %a)
; CHECK-NEXT:    zext i8 %a to i32
; CHECK-NEXT:    mul i32 4,
; CHECK-NEXT:    alloca i8, i32

define void @dyn_inst_alloca_array(i32 %a) {
  %b = add i32 1, %a
  %c = alloca i32, i32 %b
  unreachable
}
; CHECK-LABEL: define void @dyn_inst_alloca_array(i32 %a)
; CHECK-NEXT:    %b = add i32 1, %a
; CHECK-NEXT:    mul i32 4, %b
; CHECK-NEXT:    %c = alloca i8, i32

define void @dyn_inst_alloca_array_trunc(i64 %a) {
  %b = add i64 1, %a
  %c = alloca i32, i64 %b
  unreachable
}
; CHECK-LABEL: define void @dyn_inst_alloca_array_trunc(i64 %a)
; CHECK-NEXT:    %b = add i64 1, %a
; CHECK-NEXT:    trunc i64 %b to i32
; CHECK-NEXT:    mul i32 4,
; CHECK-NEXT:    %c = alloca i8, i32

define void @dyn_inst_alloca_array_zext(i8 %a) {
  %b = add i8 1, %a
  %c = alloca i32, i8 %b
  unreachable
}
; CHECK-LABEL: define void @dyn_inst_alloca_array_zext(i8 %a)
; CHECK-NEXT:    %b = add i8 1, %a
; CHECK-NEXT:    zext i8 %b to i32
; CHECK-NEXT:    mul i32 4,
; CHECK-NEXT:    %c = alloca i8, i32
