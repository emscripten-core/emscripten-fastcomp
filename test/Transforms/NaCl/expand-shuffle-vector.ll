; RUN: opt -expand-shufflevector %s -S | FileCheck %s

; Test that shufflevector is expanded to insertelement / extractelement.

define <4 x i32> @test_splat_lo_4xi32(<4 x i32> %lhs, <4 x i32> %rhs) {
  ; CHECK-LABEL: test_splat_lo_4xi32
  ; CHECK-NEXT: %1 = extractelement <4 x i32> %lhs, i32 0
  ; CHECK-NEXT: %2 = extractelement <4 x i32> %lhs, i32 0
  ; CHECK-NEXT: %3 = extractelement <4 x i32> %lhs, i32 0
  ; CHECK-NEXT: %4 = extractelement <4 x i32> %lhs, i32 0
  ; CHECK-NEXT: %5 = insertelement <4 x i32> undef, i32 %1, i32 0
  ; CHECK-NEXT: %6 = insertelement <4 x i32> %5, i32 %2, i32 1
  ; CHECK-NEXT: %7 = insertelement <4 x i32> %6, i32 %3, i32 2
  ; CHECK-NEXT: %8 = insertelement <4 x i32> %7, i32 %4, i32 3
  %res = shufflevector <4 x i32> %lhs, <4 x i32> %rhs, <4 x i32> <i32 0, i32 0, i32 0, i32 0>
  ; CHECK-NEXT: ret <4 x i32> %8
  ret <4 x i32> %res
}

define <4 x i32> @test_splat_hi_4xi32(<4 x i32> %lhs, <4 x i32> %rhs) {
  ; CHECK-LABEL: test_splat_hi_4xi32
  ; CHECK-NEXT: %1 = extractelement <4 x i32> %rhs, i32 0
  ; CHECK-NEXT: %2 = extractelement <4 x i32> %rhs, i32 0
  ; CHECK-NEXT: %3 = extractelement <4 x i32> %rhs, i32 0
  ; CHECK-NEXT: %4 = extractelement <4 x i32> %rhs, i32 0
  ; CHECK-NEXT: %5 = insertelement <4 x i32> undef, i32 %1, i32 0
  ; CHECK-NEXT: %6 = insertelement <4 x i32> %5, i32 %2, i32 1
  ; CHECK-NEXT: %7 = insertelement <4 x i32> %6, i32 %3, i32 2
  ; CHECK-NEXT: %8 = insertelement <4 x i32> %7, i32 %4, i32 3
  %res = shufflevector <4 x i32> %lhs, <4 x i32> %rhs, <4 x i32> <i32 4, i32 4, i32 4, i32 4>
  ; CHECK-NEXT: ret <4 x i32> %8
  ret <4 x i32> %res
}

define <4 x i32> @test_id_lo_4xi32(<4 x i32> %lhs, <4 x i32> %rhs) {
  ; CHECK-LABEL: test_id_lo_4xi32
  ; CHECK-NEXT: %1 = extractelement <4 x i32> %lhs, i32 0
  ; CHECK-NEXT: %2 = extractelement <4 x i32> %lhs, i32 1
  ; CHECK-NEXT: %3 = extractelement <4 x i32> %lhs, i32 2
  ; CHECK-NEXT: %4 = extractelement <4 x i32> %lhs, i32 3
  ; CHECK-NEXT: %5 = insertelement <4 x i32> undef, i32 %1, i32 0
  ; CHECK-NEXT: %6 = insertelement <4 x i32> %5, i32 %2, i32 1
  ; CHECK-NEXT: %7 = insertelement <4 x i32> %6, i32 %3, i32 2
  ; CHECK-NEXT: %8 = insertelement <4 x i32> %7, i32 %4, i32 3
  %res = shufflevector <4 x i32> %lhs, <4 x i32> %rhs, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
  ; CHECK-NEXT: ret <4 x i32> %8
  ret <4 x i32> %res
}

define <4 x i32> @test_id_hi_4xi32(<4 x i32> %lhs, <4 x i32> %rhs) {
  ; CHECK-LABEL: test_id_hi_4xi32
  ; CHECK-NEXT: %1 = extractelement <4 x i32> %rhs, i32 0
  ; CHECK-NEXT: %2 = extractelement <4 x i32> %rhs, i32 1
  ; CHECK-NEXT: %3 = extractelement <4 x i32> %rhs, i32 2
  ; CHECK-NEXT: %4 = extractelement <4 x i32> %rhs, i32 3
  ; CHECK-NEXT: %5 = insertelement <4 x i32> undef, i32 %1, i32 0
  ; CHECK-NEXT: %6 = insertelement <4 x i32> %5, i32 %2, i32 1
  ; CHECK-NEXT: %7 = insertelement <4 x i32> %6, i32 %3, i32 2
  ; CHECK-NEXT: %8 = insertelement <4 x i32> %7, i32 %4, i32 3
  %res = shufflevector <4 x i32> %lhs, <4 x i32> %rhs, <4 x i32> <i32 4, i32 5, i32 6, i32 7>
  ; CHECK-NEXT: ret <4 x i32> %8
  ret <4 x i32> %res
}

define <4 x i32> @test_interleave_lo_4xi32(<4 x i32> %lhs, <4 x i32> %rhs) {
  ; CHECK-LABEL: test_interleave_lo_4xi32
  ; CHECK-NEXT: %1 = extractelement <4 x i32> %lhs, i32 0
  ; CHECK-NEXT: %2 = extractelement <4 x i32> %rhs, i32 0
  ; CHECK-NEXT: %3 = extractelement <4 x i32> %lhs, i32 1
  ; CHECK-NEXT: %4 = extractelement <4 x i32> %rhs, i32 1
  ; CHECK-NEXT: %5 = insertelement <4 x i32> undef, i32 %1, i32 0
  ; CHECK-NEXT: %6 = insertelement <4 x i32> %5, i32 %2, i32 1
  ; CHECK-NEXT: %7 = insertelement <4 x i32> %6, i32 %3, i32 2
  ; CHECK-NEXT: %8 = insertelement <4 x i32> %7, i32 %4, i32 3
  %res = shufflevector <4 x i32> %lhs, <4 x i32> %rhs, <4 x i32> <i32 0, i32 4, i32 1, i32 5>
  ; CHECK-NEXT: ret <4 x i32> %8
  ret <4 x i32> %res
}

define <4 x i32> @test_interleave_hi_4xi32(<4 x i32> %lhs, <4 x i32> %rhs) {
  ; CHECK-LABEL: test_interleave_hi_4xi32
  ; CHECK-NEXT: %1 = extractelement <4 x i32> %lhs, i32 1
  ; CHECK-NEXT: %2 = extractelement <4 x i32> %rhs, i32 1
  ; CHECK-NEXT: %3 = extractelement <4 x i32> %lhs, i32 3
  ; CHECK-NEXT: %4 = extractelement <4 x i32> %rhs, i32 3
  ; CHECK-NEXT: %5 = insertelement <4 x i32> undef, i32 %1, i32 0
  ; CHECK-NEXT: %6 = insertelement <4 x i32> %5, i32 %2, i32 1
  ; CHECK-NEXT: %7 = insertelement <4 x i32> %6, i32 %3, i32 2
  ; CHECK-NEXT: %8 = insertelement <4 x i32> %7, i32 %4, i32 3
  %res = shufflevector <4 x i32> %lhs, <4 x i32> %rhs, <4 x i32> <i32 1, i32 5, i32 3, i32 7>
  ; CHECK-NEXT: ret <4 x i32> %8
  ret <4 x i32> %res
}

define <4 x i32> @test_undef_4xi32(<4 x i32> %lhs, <4 x i32> %rhs) {
  ; CHECK-LABEL: test_undef_4xi32
  ; CHECK-NEXT: %1 = insertelement <4 x i32> undef, i32 undef, i32 0
  ; CHECK-NEXT: %2 = insertelement <4 x i32> %1, i32 undef, i32 1
  ; CHECK-NEXT: %3 = insertelement <4 x i32> %2, i32 undef, i32 2
  ; CHECK-NEXT: %4 = insertelement <4 x i32> %3, i32 undef, i32 3
  %res = shufflevector <4 x i32> %lhs, <4 x i32> %rhs, <4 x i32> undef
  ; CHECK-NEXT: ret <4 x i32> %4
  ret <4 x i32> %res
}

define <2 x i32> @test_narrow_4xi32(<4 x i32> %lhs, <4 x i32> %rhs) {
  ; CHECK-LABEL: test_narrow_4xi32
  ; CHECK-NEXT: %1 = extractelement <4 x i32> %lhs, i32 0
  ; CHECK-NEXT: %2 = extractelement <4 x i32> %rhs, i32 0
  ; CHECK-NEXT: %3 = insertelement <2 x i32> undef, i32 %1, i32 0
  ; CHECK-NEXT: %4 = insertelement <2 x i32> %3, i32 %2, i32 1
  %res = shufflevector <4 x i32> %lhs, <4 x i32> %rhs, <2 x i32> <i32 0, i32 4>
  ; CHECK-NEXT: ret <2 x i32> %4
  ret <2 x i32> %res
}

define <8 x i32> @test_widen_4xi32(<4 x i32> %lhs, <4 x i32> %rhs) {
  ; CHECK-LABEL: test_widen_4xi32
  ; CHECK-NEXT: %1 = extractelement <4 x i32> %rhs, i32 3
  ; CHECK-NEXT: %2 = extractelement <4 x i32> %rhs, i32 2
  ; CHECK-NEXT: %3 = extractelement <4 x i32> %rhs, i32 1
  ; CHECK-NEXT: %4 = extractelement <4 x i32> %rhs, i32 0
  ; CHECK-NEXT: %5 = extractelement <4 x i32> %lhs, i32 3
  ; CHECK-NEXT: %6 = extractelement <4 x i32> %lhs, i32 2
  ; CHECK-NEXT: %7 = extractelement <4 x i32> %lhs, i32 1
  ; CHECK-NEXT: %8 = extractelement <4 x i32> %lhs, i32 0
  ; CHECK-NEXT: %9 = insertelement <8 x i32> undef, i32 %1, i32 0
  ; CHECK-NEXT: %10 = insertelement <8 x i32> %9, i32 %2, i32 1
  ; CHECK-NEXT: %11 = insertelement <8 x i32> %10, i32 %3, i32 2
  ; CHECK-NEXT: %12 = insertelement <8 x i32> %11, i32 %4, i32 3
  ; CHECK-NEXT: %13 = insertelement <8 x i32> %12, i32 %5, i32 4
  ; CHECK-NEXT: %14 = insertelement <8 x i32> %13, i32 %6, i32 5
  ; CHECK-NEXT: %15 = insertelement <8 x i32> %14, i32 %7, i32 6
  ; CHECK-NEXT: %16 = insertelement <8 x i32> %15, i32 %8, i32 7
  %res = shufflevector <4 x i32> %lhs, <4 x i32> %rhs, <8 x i32> <i32 7, i32 6, i32 5, i32 4, i32 3, i32 2, i32 1, i32 0>
  ; CHECK-NEXT: ret <8 x i32> %16
  ret <8 x i32> %res
}
