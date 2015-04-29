; RUN: opt -globalize-constant-vectors %s -S | FileCheck %s
; RUN: opt -globalize-constant-vectors %s -S | FileCheck -check-prefix=C4xi1 %s
; RUN: opt -globalize-constant-vectors %s -S | FileCheck -check-prefix=C8xi1 %s
; RUN: opt -globalize-constant-vectors %s -S | FileCheck -check-prefix=C16xi1 %s
; RUN: opt -globalize-constant-vectors %s -S | FileCheck -check-prefix=C16xi8 %s
; RUN: opt -globalize-constant-vectors %s -S | FileCheck -check-prefix=C8xi16 %s
; RUN: opt -globalize-constant-vectors %s -S | FileCheck -check-prefix=C4xi32 %s
; RUN: opt -globalize-constant-vectors %s -S | FileCheck -check-prefix=C4xfloat %s
; RUN: opt -globalize-constant-vectors %s -S | FileCheck -check-prefix=Cbranch %s
; RUN: opt -globalize-constant-vectors %s -S | FileCheck -check-prefix=Cduplicate %s
; RUN: opt -globalize-constant-vectors %s -S | FileCheck -check-prefix=Czeroinitializer %s
; RUN: opt -expand-constant-expr -globalize-constant-vectors %s -S | FileCheck -check-prefix=Cnestedconst %s

; Run the test once per function so that each check can look at its
; globals as well as its function.

; The datalayout is needed to determine the alignment of the globals.
target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:32"

; Globals shouldn't get globalized.
; CHECK: @global_should_stay_untouched = internal constant <4 x i32> <i32 1337, i32 0, i32 0, i32 0>
@global_should_stay_untouched = internal constant <4 x i32> <i32 1337, i32 0, i32 0, i32 0>

; Also test a global initializer with nested const-exprs.
; NOTE: Have the global share the same const-expr as an instruction below.
; CHECK: @global_with_nesting = internal global <{ <4 x i32>, <8 x i16> }> <{ <4 x i32> <i32 1, i32 4, i32 10, i32 20>, <8 x i16> <i16 0, i16 1, i16 1, i16 2, i16 3, i16 5, i16 8, i16 13> }>
@global_with_nesting = internal global <{ <4 x i32>, <8 x i16> }> <{ <4 x i32> <i32 1, i32 4, i32 10, i32 20>, <8 x i16> <i16 0, i16 1, i16 1, i16 2, i16 3, i16 5, i16 8, i16 13> }>

; 4xi1 vectors should get globalized.
define void @test4xi1(<4 x i1> %in) {
  %ft0 = and <4 x i1> %in, <i1 false, i1 true, i1 false, i1 true>
  %ft1 = and <4 x i1> <i1 true, i1 false, i1 true, i1 false>, %in
  ret void
}
; C4xi1: @[[C1:[_a-z0-9]+]] = internal unnamed_addr constant <4 x i1> <i1 false, i1 true, i1 false, i1 true>, align 4
; C4xi1: @[[C2:[_a-z0-9]+]] = internal unnamed_addr constant <4 x i1> <i1 true, i1 false, i1 true, i1 false>, align 4
; C4xi1: define void @test4xi1(<4 x i1> %in) {
; C4xi1-NEXT: %[[M1:[_a-z0-9]+]] = load <4 x i1>, <4 x i1>* @[[C1]], align 4
; C4xi1-NEXT: %[[M2:[_a-z0-9]+]] = load <4 x i1>, <4 x i1>* @[[C2]], align 4
; C4xi1-NEXT: %ft0 = and <4 x i1> %in, %[[M1]]
; C4xi1-NEXT: %ft1 = and <4 x i1> %[[M2]], %in
; C4xi1-NEXT: ret void

; 8xi1 vectors should get globalized.
define void @test8xi1(<8 x i1> %in) {
  %ft0 = and <8 x i1> %in, <i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true>
  %ft1 = and <8 x i1> <i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false>, %in
  ret void
}
; C8xi1: @[[C1:[_a-z0-9]+]] = internal unnamed_addr constant <8 x i1> <i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true>, align 8
; C8xi1: @[[C2:[_a-z0-9]+]] = internal unnamed_addr constant <8 x i1> <i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false>, align 8
; C8xi1: define void @test8xi1(<8 x i1> %in) {
; C8xi1-NEXT: %[[M1:[_a-z0-9]+]] = load <8 x i1>, <8 x i1>* @[[C1]], align 8
; C8xi1-NEXT: %[[M2:[_a-z0-9]+]] = load <8 x i1>, <8 x i1>* @[[C2]], align 8
; C8xi1-NEXT: %ft0 = and <8 x i1> %in, %[[M1]]
; C8xi1-NEXT: %ft1 = and <8 x i1> %[[M2]], %in
; C8xi1-NEXT: ret void

; 16xi1 vectors should get globalized.
define void @test16xi1(<16 x i1> %in) {
  %ft0 = and <16 x i1> %in, <i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true>
  %ft1 = and <16 x i1> <i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false>, %in
  ret void
}
; C16xi1: @[[C1:[_a-z0-9]+]] = internal unnamed_addr constant <16 x i1> <i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true>, align 16
; C16xi1: @[[C2:[_a-z0-9]+]] = internal unnamed_addr constant <16 x i1> <i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false, i1 true, i1 false>, align 16
; C16xi1: define void @test16xi1(<16 x i1> %in) {
; C16xi1-NEXT: %[[M1:[_a-z0-9]+]] = load <16 x i1>, <16 x i1>* @[[C1]], align 16
; C16xi1-NEXT: %[[M2:[_a-z0-9]+]] = load <16 x i1>, <16 x i1>* @[[C2]], align 16
; C16xi1-NEXT: %ft0 = and <16 x i1> %in, %[[M1]]
; C16xi1-NEXT: %ft1 = and <16 x i1> %[[M2]], %in
; C16xi1-NEXT: ret void

; 16xi8 vectors should get globalized.
define void @test16xi8(<16 x i8> %in) {
  %nonsquares = add <16 x i8> %in, <i8 2, i8 3, i8 5, i8 6, i8 7, i8 8, i8 10, i8 11, i8 12, i8 13, i8 14, i8 15, i8 17, i8 18, i8 19, i8 20>
  %sort = add <16 x i8> <i8 0, i8 1, i8 3, i8 5, i8 9, i8 11, i8 14, i8 17, i8 25, i8 27, i8 30, i8 33, i8 38, i8 41, i8 45, i8 49>, %in
  ret void
}
; C16xi8: @[[C1:[_a-z0-9]+]] = internal unnamed_addr constant <16 x i8> <i8 2, i8 3, i8 5, i8 6, i8 7, i8 8, i8 10, i8 11, i8 12, i8 13, i8 14, i8 15, i8 17, i8 18, i8 19, i8 20>, align 4
; C16xi8: @[[C2:[_a-z0-9]+]] = internal unnamed_addr constant <16 x i8> <i8 0, i8 1, i8 3, i8 5, i8 9, i8 11, i8 14, i8 17, i8 25, i8 27, i8 30, i8 33, i8 38, i8 41, i8 45, i8 49>, align 4
; C16xi8: define void @test16xi8(<16 x i8> %in) {
; C16xi8-NEXT: %[[M1:[_a-z0-9]+]] = load <16 x i8>, <16 x i8>* @[[C1]], align 4
; C16xi8-NEXT: %[[M2:[_a-z0-9]+]] = load <16 x i8>, <16 x i8>* @[[C2]], align 4
; C16xi8-NEXT: %nonsquares = add <16 x i8> %in, %[[M1]]
; C16xi8-NEXT: %sort = add <16 x i8> %[[M2]], %in
; C16xi8-NEXT: ret void

; 8xi16 vectors should get globalized.
define void @test8xi16(<8 x i16> %in) {
  %fib = add <8 x i16> %in, <i16 0, i16 1, i16 1, i16 2, i16 3, i16 5, i16 8, i16 13>
  %answer = add <8 x i16> <i16 42, i16 42, i16 42, i16 42, i16 42, i16 42, i16 42, i16 42>, %in
  ret void
}
; C8xi16: @[[C1:[_a-z0-9]+]] = internal unnamed_addr constant <8 x i16> <i16 0, i16 1, i16 1, i16 2, i16 3, i16 5, i16 8, i16 13>, align 4
; C8xi16: @[[C2:[_a-z0-9]+]] = internal unnamed_addr constant <8 x i16> <i16 42, i16 42, i16 42, i16 42, i16 42, i16 42, i16 42, i16 42>, align 4
; C8xi16: define void @test8xi16(<8 x i16> %in) {
; C8xi16-NEXT: %[[M1:[_a-z0-9]+]] = load <8 x i16>, <8 x i16>* @[[C1]], align 4
; C8xi16-NEXT: %[[M2:[_a-z0-9]+]] = load <8 x i16>, <8 x i16>* @[[C2]], align 4
; C8xi16-NEXT: %fib = add <8 x i16> %in, %[[M1]]
; C8xi16-NEXT: %answer = add <8 x i16> %[[M2]], %in
; C8xi16-NEXT: ret void

; 4xi32 vectors should get globalized.
define void @test4xi32(<4 x i32> %in) {
  %tetrahedral = add <4 x i32> %in, <i32 1, i32 4, i32 10, i32 20>
  %serauqs = add <4 x i32> <i32 1, i32 4, i32 9, i32 61>, %in
  ret void
}
; C4xi32: @[[C1:[_a-z0-9]+]] = internal unnamed_addr constant <4 x i32> <i32 1, i32 4, i32 10, i32 20>, align 4
; C4xi32: @[[C2:[_a-z0-9]+]] = internal unnamed_addr constant <4 x i32> <i32 1, i32 4, i32 9, i32 61>, align 4
; C4xi32: define void @test4xi32(<4 x i32> %in) {
; C4xi32-NEXT: %[[M1:[_a-z0-9]+]] = load <4 x i32>, <4 x i32>* @[[C1]], align 4
; C4xi32-NEXT: %[[M2:[_a-z0-9]+]] = load <4 x i32>, <4 x i32>* @[[C2]], align 4
; C4xi32-NEXT: %tetrahedral = add <4 x i32> %in, %[[M1]]
; C4xi32-NEXT: %serauqs = add <4 x i32> %[[M2]], %in
; C4xi32-NEXT: ret void

; 4xfloat vectors should get globalized.
define void @test4xfloat(<4 x float> %in) {
  %polyhex = fadd <4 x float> %in, <float 1., float 1., float 3., float 7.>
  %poset = fadd <4 x float> <float 1., float 1., float 3., float 19.>, %in
  ret void
}
; C4xfloat: @[[C1:[_a-z0-9]+]] = internal unnamed_addr constant <4 x float> <float 1.000000e+00, float 1.000000e+00, float 3.000000e+00, float 7.000000e+00>, align 4
; C4xfloat: @[[C2:[_a-z0-9]+]] = internal unnamed_addr constant <4 x float> <float 1.000000e+00, float 1.000000e+00, float 3.000000e+00, float 1.900000e+01>, align 4
; C4xfloat: define void @test4xfloat(<4 x float> %in) {
; C4xfloat-NEXT: %[[M1:[_a-z0-9]+]] = load <4 x float>, <4 x float>* @[[C1]], align 4
; C4xfloat-NEXT: %[[M2:[_a-z0-9]+]] = load <4 x float>, <4 x float>* @[[C2]], align 4
; C4xfloat-NEXT: %polyhex = fadd <4 x float> %in, %[[M1]]
; C4xfloat-NEXT: %poset = fadd <4 x float> %[[M2]], %in
; C4xfloat-NEXT: ret void

; Globalized constant loads have to dominate their use.
define void @testbranch(i1 %cond, <4 x i32> %in) {
  br i1 %cond, label %lhs, label %rhs
lhs:
  %from_lhs = add <4 x i32> %in, <i32 1, i32 1, i32 2, i32 2>
  br label %done
rhs:
  %from_rhs = add <4 x i32> <i32 2, i32 2, i32 1, i32 1>, %in
  br label %done
done:
  %merged = phi <4 x i32> [ %from_lhs, %lhs ], [ %from_rhs, %rhs ]
  ret void
}
; Cbranch: @[[C1:[_a-z0-9]+]] = internal unnamed_addr constant <4 x i32> <i32 1, i32 1, i32 2, i32 2>, align 4
; Cbranch: @[[C2:[_a-z0-9]+]] = internal unnamed_addr constant <4 x i32> <i32 2, i32 2, i32 1, i32 1>, align 4
; Cbranch: define void @testbranch(i1 %cond, <4 x i32> %in) {
; Cbranch-NEXT: %[[M1:[_a-z0-9]+]] = load <4 x i32>, <4 x i32>* @[[C1]], align 4
; Cbranch-NEXT: %[[M2:[_a-z0-9]+]] = load <4 x i32>, <4 x i32>* @[[C2]], align 4
; Cbranch-NEXT: br i1 %cond, label %lhs, label %rhs
; Cbranch: lhs:
; Cbranch-NEXT: %from_lhs = add <4 x i32> %in, %[[M1]]
; Cbranch-NEXT: br label %done
; Cbranch: rhs:
; Cbranch-NEXT: %from_rhs = add <4 x i32> %[[M2]], %in
; Cbranch-NEXT: br label %done
; Cbranch: done:
; Cbranch-NEXT: %merged = phi <4 x i32> [ %from_lhs, %lhs ], [ %from_rhs, %rhs ]
; Cbranch-NEXT: ret void

; Globalizing redundant constants between functions should materialize
; them in each function, but there should only be a single global.
define void @testduplicate1() {
  %foo = add <4 x i32> <i32 1, i32 1, i32 1, i32 1>, undef
  ret void
}
define void @testduplicate2() {
  %foo = add <4 x i32> <i32 1, i32 1, i32 1, i32 1>, undef
  ret void
}
; Cduplicate: @[[C1:[_a-z0-9]+]] = internal unnamed_addr constant <4 x i32> <i32 1, i32 1, i32 1, i32 1>, align 4
; Cduplicate: define void @testduplicate1() {
; Cduplicate-NEXT: %[[M1:[_a-z0-9]+]] = load <4 x i32>, <4 x i32>* @[[C1]], align 4
; Cduplicate-NEXT: %foo = add <4 x i32> %[[M1]], undef
; Cduplicate-NEXT: ret void
; Cduplicate: define void @testduplicate2() {
; Cduplicate-NEXT: %[[M1:[_a-z0-9]+]] = load <4 x i32>, <4 x i32>* @[[C1]], align 4
; Cduplicate-NEXT: %foo = add <4 x i32> %[[M1]], undef
; Cduplicate-NEXT: ret void

; zeroinitializer vectors should get globalized.
define void @testzeroinitializer(<4 x float> %in) {
  %id = fadd <4 x float> %in, <float 0.0, float 0.0, float 0.0, float 0.0>
  ret void
}
; Czeroinitializer: @[[C1:[_a-z0-9]+]] = internal unnamed_addr constant <4 x float> zeroinitializer, align 4
; Czeroinitializer: define void @testzeroinitializer(<4 x float> %in) {
; Czeroinitializer-NEXT: %[[M1:[_a-z0-9]+]] = load <4 x float>, <4 x float>* @[[C1]], align 4
; Czeroinitializer-NEXT: %id = fadd <4 x float> %in, %[[M1]]
; Czeroinitializer-NEXT: ret void

; Nested constant exprs are handled by running -expand-constant-expr first.
define i64 @test_nested_const(i64 %x) {
  %foo = add i64 bitcast (<8 x i8><i8 10, i8 20, i8 30, i8 40, i8 50, i8 60, i8 70, i8 80> to i64), %x
  ret i64 %foo
}
; Cnestedconst: @[[C1:[_a-z0-9]+]] = internal unnamed_addr constant <8 x i8> <i8 10, i8 20, i8 30, i8 40, i8 50, i8 60, i8 70, i8 80>, align 8
; Cnestedconst: define i64 @test_nested_const(i64 %x) {
; Cnestedconst-NEXT: %[[M1:[_a-z0-9]+]] = load <8 x i8>, <8 x i8>* @[[C1]], align 8
; Cnestedconst-NEXT: %[[X1:[_a-z0-9]+]] = bitcast <8 x i8> %[[M1]] to i64
; Cnestedconst-NEXT: add i64 %[[X1]], %x
; Cnestedconst-NEXT: ret i64 %foo
