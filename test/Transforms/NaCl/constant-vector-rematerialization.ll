; RUN: opt < %s -backend-canonicalize -S | FileCheck %s

; Test that constant vectors that were globalized get rematerialized properly.

; The datalayout is needed to determine the alignment of the globals.
target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:32"

@veci32 = internal constant [4 x i32] [i32 1, i32 2, i32 3, i32 4]
@veci32zero = internal constant [4 x i32] zeroinitializer

@veci8 = internal constant [16 x i8] [i8 255, i8 255, i8 255, i8 0, i8 255, i8 255, i8 0, i8 255, i8 255, i8 0, i8 255, i8 255, i8 0, i8 255, i8 255, i8 255]
@veci8zero = internal constant [16 x i8] zeroinitializer

define <4 x i32> @test_vec_i32() {
  %bc = bitcast [4 x i32]* @veci32 to <4 x i32>*
  %v = load <4 x i32>, <4 x i32>* %bc
  ret <4 x i32> %v
}
; CHECK-LABEL: @test_vec_i32(
; CHECK-NEXT: ret <4 x i32> <i32 1, i32 2, i32 3, i32 4>

define <4 x i32> @test_vec_i32_zero() {
  %bc = bitcast [4 x i32]* @veci32zero to <4 x i32>*
  %v = load <4 x i32>, <4 x i32>* %bc
  ret <4 x i32> %v
}
; CHECK-LABEL: @test_vec_i32_zero(
; CHECK-NEXT: ret <4 x i32> zeroinitializer

define <4 x i32> @test_vec_i8() {
  %bc = bitcast [16 x i8]* @veci8 to <4 x i32>*
  %v = load <4 x i32>, <4 x i32>* %bc
  ret <4 x i32> %v
}
; CHECK-LABEL: @test_vec_i8(
; CHECK-NEXT: ret <4 x i32> <i32 16777215, i32 -16711681, i32 -65281, i32 -256>

define <4 x i32> @test_vec_i8_zero() {
  %bc = bitcast [16 x i8]* @veci8zero to <4 x i32>*
  %v = load <4 x i32>, <4 x i32>* %bc
  ret <4 x i32> %v
}
; CHECK-LABEL: @test_vec_i8_zero(
; CHECK-NEXT: ret <4 x i32> zeroinitializer
