; RUN: opt -expand-shufflevector %s -S | \
; RUN:   opt -backend-canonicalize -S | FileCheck %s

; Test that shufflevector is re-created after having been expanded to
; insertelement / extractelement: shufflevector isn't part of the stable
; PNaCl ABI but insertelement / extractelement are. Re-creating
; shufflevector allows the backend to generate more efficient code.
;
; TODO(jfb) Narrow and widen aren't tested since the underlying types
;           are currently not supported by the PNaCl ABI.

; The datalayout is needed to fold global constants.
target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:32"

define <4 x i32> @test_splat_lo_4xi32(<4 x i32> %lhs, <4 x i32> %rhs) {
  ; CHECK-LABEL: test_splat_lo_4xi32
  ; CHECK-NEXT: %[[R:[0-9]+]] = shufflevector <4 x i32> %lhs, <4 x i32> undef, <4 x i32> zeroinitializer
  %res = shufflevector <4 x i32> %lhs, <4 x i32> %rhs, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
  ; CHECK-NEXT: ret <4 x i32> %[[R]]
  ret <4 x i32> %res
}

define <4 x i32> @test_splat_hi_4xi32(<4 x i32> %lhs, <4 x i32> %rhs) {
  ; CHECK-LABEL: test_splat_hi_4xi32
  ; CHECK-NEXT: %[[R:[0-9]+]] = shufflevector <4 x i32> %rhs, <4 x i32> undef, <4 x i32> zeroinitializer
  %res = shufflevector <4 x i32> %lhs, <4 x i32> %rhs, <4 x i32> <i32 4, i32 4, i32 4, i32 4>
  ; CHECK-NEXT: ret <4 x i32> %[[R]]
  ret <4 x i32> %res
}

define <4 x i32> @test_id_lo_4xi32(<4 x i32> %lhs, <4 x i32> %rhs) {
  ; CHECK-LABEL: test_id_lo_4xi32
  ; CHECK-NEXT: %[[R:[0-9]+]] = shufflevector <4 x i32> %lhs, <4 x i32> undef, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
  %res = shufflevector <4 x i32> %lhs, <4 x i32> %rhs, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
  ; CHECK-NEXT: ret <4 x i32> %[[R]]
  ret <4 x i32> %res
}

define <4 x i32> @test_id_hi_4xi32(<4 x i32> %lhs, <4 x i32> %rhs) {
  ; CHECK-LABEL: test_id_hi_4xi32
  ; CHECK-NEXT: %[[R:[0-9]+]] = shufflevector <4 x i32> %rhs, <4 x i32> undef, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
  %res = shufflevector <4 x i32> %lhs, <4 x i32> %rhs, <4 x i32> <i32 4, i32 5, i32 6, i32 7>
  ; CHECK-NEXT: ret <4 x i32> %[[R]]
  ret <4 x i32> %res
}

define <4 x i32> @test_interleave_lo_4xi32(<4 x i32> %lhs, <4 x i32> %rhs) {
  ; CHECK-LABEL: test_interleave_lo_4xi32
  ; CHECK-NEXT: %[[R:[0-9]+]] = shufflevector <4 x i32> %lhs, <4 x i32> %rhs, <4 x i32> <i32 0, i32 4, i32 1, i32 5>
  %res = shufflevector <4 x i32> %lhs, <4 x i32> %rhs, <4 x i32> <i32 0, i32 4, i32 1, i32 5>
  ; CHECK-NEXT: ret <4 x i32> %[[R]]
  ret <4 x i32> %res
}

define <4 x i32> @test_interleave_hi_4xi32(<4 x i32> %lhs, <4 x i32> %rhs) {
  ; CHECK-LABEL: test_interleave_hi_4xi32
  ; CHECK-NEXT: %[[R:[0-9]+]] = shufflevector <4 x i32> %lhs, <4 x i32> %rhs, <4 x i32> <i32 1, i32 5, i32 3, i32 7>
  %res = shufflevector <4 x i32> %lhs, <4 x i32> %rhs, <4 x i32> <i32 1, i32 5, i32 3, i32 7>
  ; CHECK-NEXT: ret <4 x i32> %[[R]]
  ret <4 x i32> %res
}

define <4 x i32> @test_with_constant(<4 x i32> %lhs, <4 x i32> %rhs) {
  ; CHECK-LABEL: test_with_constant
  ; CHECK-NEXT: %[[R:[0-9]+]] = shufflevector <4 x i32> %lhs, <4 x i32> <i32 0, i32 -1, i32 undef, i32 undef>, <4 x i32> <i32 4, i32 0, i32 1, i32 5>
  %res = shufflevector <4 x i32> %lhs, <4 x i32> <i32 0, i32 -1, i32 undef, i32 undef>, <4 x i32> <i32 4, i32 0, i32 1, i32 5>
  ; CHECK-NEXT: ret <4 x i32> %[[R]]
  ret <4 x i32> %res
}
