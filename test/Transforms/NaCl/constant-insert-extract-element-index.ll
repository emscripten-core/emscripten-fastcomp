; RUN: opt -constant-insert-extract-element-index %s -S | FileCheck %s

; The datalayout is needed to determine the alignment of the load/stores.
target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:32"


; The following insert/extract elements are all indexed with an in-range
; constant, they should remain unchanged.

define void @test_16xi1_unchanged(<16 x i1> %in) {
  ; CHECK-LABEL: test_16xi1_unchanged
  ; CHECK-NOT: alloca
  ; CHECK: extractelement
  %e.0 = extractelement <16 x i1> %in, i32 0
  %e.1 = extractelement <16 x i1> %in, i32 1
  %e.2 = extractelement <16 x i1> %in, i32 2
  %e.3 = extractelement <16 x i1> %in, i32 3
  %e.4 = extractelement <16 x i1> %in, i32 4
  %e.5 = extractelement <16 x i1> %in, i32 5
  %e.6 = extractelement <16 x i1> %in, i32 6
  %e.7 = extractelement <16 x i1> %in, i32 7
  %e.8 = extractelement <16 x i1> %in, i32 8
  %e.9 = extractelement <16 x i1> %in, i32 9
  %e.10 = extractelement <16 x i1> %in, i32 10
  %e.11 = extractelement <16 x i1> %in, i32 11
  %e.12 = extractelement <16 x i1> %in, i32 12
  %e.13 = extractelement <16 x i1> %in, i32 13
  %e.14 = extractelement <16 x i1> %in, i32 14
  %e.15 = extractelement <16 x i1> %in, i32 15
  ; CHECK-NOT: alloca
  ; CHECK: insertelement
  %i.0 = insertelement <16 x i1> %in, i1 true, i32 0
  %i.1 = insertelement <16 x i1> %in, i1 true, i32 1
  %i.2 = insertelement <16 x i1> %in, i1 true, i32 2
  %i.3 = insertelement <16 x i1> %in, i1 true, i32 3
  %i.4 = insertelement <16 x i1> %in, i1 true, i32 4
  %i.5 = insertelement <16 x i1> %in, i1 true, i32 5
  %i.6 = insertelement <16 x i1> %in, i1 true, i32 6
  %i.7 = insertelement <16 x i1> %in, i1 true, i32 7
  %i.8 = insertelement <16 x i1> %in, i1 true, i32 8
  %i.9 = insertelement <16 x i1> %in, i1 true, i32 9
  %i.10 = insertelement <16 x i1> %in, i1 true, i32 10
  %i.11 = insertelement <16 x i1> %in, i1 true, i32 11
  %i.12 = insertelement <16 x i1> %in, i1 true, i32 12
  %i.13 = insertelement <16 x i1> %in, i1 true, i32 13
  %i.14 = insertelement <16 x i1> %in, i1 true, i32 14
  %i.15 = insertelement <16 x i1> %in, i1 true, i32 15
  ; CHECK-NOT: alloca
  ret void
}

define void @test_8xi1_unchanged(<8 x i1> %in) {
  ; CHECK-LABEL: test_8xi1_unchanged
  ; CHECK-NOT: alloca
  ; CHECK: extractelement
  %e.0 = extractelement <8 x i1> %in, i32 0
  %e.1 = extractelement <8 x i1> %in, i32 1
  %e.2 = extractelement <8 x i1> %in, i32 2
  %e.3 = extractelement <8 x i1> %in, i32 3
  %e.4 = extractelement <8 x i1> %in, i32 4
  %e.5 = extractelement <8 x i1> %in, i32 5
  %e.6 = extractelement <8 x i1> %in, i32 6
  %e.7 = extractelement <8 x i1> %in, i32 7
  ; CHECK-NOT: alloca
  ; CHECK: insertelement
  %i.0 = insertelement <8 x i1> %in, i1 true, i32 0
  %i.1 = insertelement <8 x i1> %in, i1 true, i32 1
  %i.2 = insertelement <8 x i1> %in, i1 true, i32 2
  %i.3 = insertelement <8 x i1> %in, i1 true, i32 3
  %i.4 = insertelement <8 x i1> %in, i1 true, i32 4
  %i.5 = insertelement <8 x i1> %in, i1 true, i32 5
  %i.6 = insertelement <8 x i1> %in, i1 true, i32 6
  %i.7 = insertelement <8 x i1> %in, i1 true, i32 7
  ; CHECK-NOT: alloca
  ret void
}

define void @test_4xi1_unchanged(<4 x i1> %in) {
  ; CHECK-LABEL: test_4xi1_unchanged
  ; CHECK-NOT: alloca
  ; CHECK: extractelement
  %e.0 = extractelement <4 x i1> %in, i32 0
  %e.1 = extractelement <4 x i1> %in, i32 1
  %e.2 = extractelement <4 x i1> %in, i32 2
  %e.3 = extractelement <4 x i1> %in, i32 3
  ; CHECK-NOT: alloca
  ; CHECK: insertelement
  %i.0 = insertelement <4 x i1> %in, i1 true, i32 0
  %i.1 = insertelement <4 x i1> %in, i1 true, i32 1
  %i.2 = insertelement <4 x i1> %in, i1 true, i32 2
  %i.3 = insertelement <4 x i1> %in, i1 true, i32 3
  ; CHECK-NOT: alloca
  ret void
}

define void @test_16xi8_unchanged(<16 x i8> %in) {
  ; CHECK-LABEL: test_16xi8_unchanged
  ; CHECK-NOT: alloca
  ; CHECK: extractelement
  %e.0 = extractelement <16 x i8> %in, i32 0
  %e.1 = extractelement <16 x i8> %in, i32 1
  %e.2 = extractelement <16 x i8> %in, i32 2
  %e.3 = extractelement <16 x i8> %in, i32 3
  %e.4 = extractelement <16 x i8> %in, i32 4
  %e.5 = extractelement <16 x i8> %in, i32 5
  %e.6 = extractelement <16 x i8> %in, i32 6
  %e.7 = extractelement <16 x i8> %in, i32 7
  %e.8 = extractelement <16 x i8> %in, i32 8
  %e.9 = extractelement <16 x i8> %in, i32 9
  %e.10 = extractelement <16 x i8> %in, i32 10
  %e.11 = extractelement <16 x i8> %in, i32 11
  %e.12 = extractelement <16 x i8> %in, i32 12
  %e.13 = extractelement <16 x i8> %in, i32 13
  %e.14 = extractelement <16 x i8> %in, i32 14
  %e.15 = extractelement <16 x i8> %in, i32 15
  ; CHECK-NOT: alloca
  ; CHECK: insertelement
  %i.0 = insertelement <16 x i8> %in, i8 42, i32 0
  %i.1 = insertelement <16 x i8> %in, i8 42, i32 1
  %i.2 = insertelement <16 x i8> %in, i8 42, i32 2
  %i.3 = insertelement <16 x i8> %in, i8 42, i32 3
  %i.4 = insertelement <16 x i8> %in, i8 42, i32 4
  %i.5 = insertelement <16 x i8> %in, i8 42, i32 5
  %i.6 = insertelement <16 x i8> %in, i8 42, i32 6
  %i.7 = insertelement <16 x i8> %in, i8 42, i32 7
  %i.8 = insertelement <16 x i8> %in, i8 42, i32 8
  %i.9 = insertelement <16 x i8> %in, i8 42, i32 9
  %i.10 = insertelement <16 x i8> %in, i8 42, i32 10
  %i.11 = insertelement <16 x i8> %in, i8 42, i32 11
  %i.12 = insertelement <16 x i8> %in, i8 42, i32 12
  %i.13 = insertelement <16 x i8> %in, i8 42, i32 13
  %i.14 = insertelement <16 x i8> %in, i8 42, i32 14
  %i.15 = insertelement <16 x i8> %in, i8 42, i32 15
  ; CHECK-NOT: alloca
  ret void
}

define void @test_8xi16_unchanged(<8 x i16> %in) {
  ; CHECK-LABEL: test_8xi16_unchanged
  ; CHECK-NOT: alloca
  ; CHECK: extractelement
  %e.0 = extractelement <8 x i16> %in, i32 0
  %e.1 = extractelement <8 x i16> %in, i32 1
  %e.2 = extractelement <8 x i16> %in, i32 2
  %e.3 = extractelement <8 x i16> %in, i32 3
  %e.4 = extractelement <8 x i16> %in, i32 4
  %e.5 = extractelement <8 x i16> %in, i32 5
  %e.6 = extractelement <8 x i16> %in, i32 6
  %e.7 = extractelement <8 x i16> %in, i32 7
  ; CHECK-NOT: alloca
  ; CHECK: insertelement
  %i.0 = insertelement <8 x i16> %in, i16 42, i32 0
  %i.1 = insertelement <8 x i16> %in, i16 42, i32 1
  %i.2 = insertelement <8 x i16> %in, i16 42, i32 2
  %i.3 = insertelement <8 x i16> %in, i16 42, i32 3
  %i.4 = insertelement <8 x i16> %in, i16 42, i32 4
  %i.5 = insertelement <8 x i16> %in, i16 42, i32 5
  %i.6 = insertelement <8 x i16> %in, i16 42, i32 6
  %i.7 = insertelement <8 x i16> %in, i16 42, i32 7
  ; CHECK-NOT: alloca
  ret void
}

define void @test_4xi32_unchanged(<4 x i32> %in) {
  ; CHECK-LABEL: test_4xi32_unchanged
  ; CHECK-NOT: alloca
  ; CHECK: extractelement
  %e.0 = extractelement <4 x i32> %in, i32 0
  %e.1 = extractelement <4 x i32> %in, i32 1
  %e.2 = extractelement <4 x i32> %in, i32 2
  %e.3 = extractelement <4 x i32> %in, i32 3
  ; CHECK-NOT: alloca
  ; CHECK: insertelement
  %i.0 = insertelement <4 x i32> %in, i32 42, i32 0
  %i.1 = insertelement <4 x i32> %in, i32 42, i32 1
  %i.2 = insertelement <4 x i32> %in, i32 42, i32 2
  %i.3 = insertelement <4 x i32> %in, i32 42, i32 3
  ; CHECK-NOT: alloca
  ret void
}

define void @test_4xfloat_unchanged(<4 x float> %in) {
  ; CHECK-LABEL: test_4xfloat_unchanged
  ; CHECK-NOT: alloca
  ; CHECK: extractelement
  %e.0 = extractelement <4 x float> %in, i32 0
  %e.1 = extractelement <4 x float> %in, i32 1
  %e.2 = extractelement <4 x float> %in, i32 2
  %e.3 = extractelement <4 x float> %in, i32 3
  ; CHECK-NOT: alloca
  ; CHECK: insertelement
  %i.0 = insertelement <4 x float> %in, float 42.0, i32 0
  %i.1 = insertelement <4 x float> %in, float 42.0, i32 1
  %i.2 = insertelement <4 x float> %in, float 42.0, i32 2
  %i.3 = insertelement <4 x float> %in, float 42.0, i32 3
  ; CHECK-NOT: alloca
  ret void
}


; The following insert/extract elements are all indexed with an
; out-of-range constant, they should get modified so that the constant
; is now in-range.

define <16 x i1> @test_16xi1_out_of_range(<16 x i1> %in) {
  ; CHECK-LABEL: test_16xi1_out_of_range
  ; CHECK-NEXT: extractelement <16 x i1> %in, i32 0
  %e.16 = extractelement <16 x i1> %in, i32 16
  ; CHECK-NEXT: %i.16 = insertelement <16 x i1> %in, i1 %e.16, i32 0
  %i.16 = insertelement <16 x i1> %in, i1 %e.16, i32 16
  ; CHECK-NEXT: ret <16 x i1> %i.16
  ret <16 x i1> %i.16
}

define <8 x i1> @test_8xi1_out_of_range(<8 x i1> %in) {
  ; CHECK-LABEL: test_8xi1_out_of_range
  ; CHECK-NEXT: %e.8 = extractelement <8 x i1> %in, i32 0
  %e.8 = extractelement <8 x i1> %in, i32 8
  ; CHECK-NEXT: %i.8 = insertelement <8 x i1> %in, i1 %e.8, i32 0
  %i.8 = insertelement <8 x i1> %in, i1 %e.8, i32 8
  ; CHECK-NEXT: ret <8 x i1> %i.8
  ret <8 x i1> %i.8
}

define <4 x i1> @test_4xi1_out_of_range(<4 x i1> %in) {
  ; CHECK-LABEL: test_4xi1_out_of_range
  ; CHECK-NEXT: %e.4 = extractelement <4 x i1> %in, i32 0
  %e.4 = extractelement <4 x i1> %in, i32 4
  ; CHECK-NEXT: %i.4 = insertelement <4 x i1> %in, i1 %e.4, i32 0
  %i.4 = insertelement <4 x i1> %in, i1 %e.4, i32 4
  ; CHECK-NEXT: ret <4 x i1> %i.4
  ret <4 x i1> %i.4
}

define <16 x i8> @test_16xi8_out_of_range(<16 x i8> %in) {
  ; CHECK-LABEL: test_16xi8_out_of_range
  ; CHECK-NEXT: %e.16 = extractelement <16 x i8> %in, i32 0
  %e.16 = extractelement <16 x i8> %in, i32 16
  ; CHECK-NEXT: %i.16 = insertelement <16 x i8> %in, i8 %e.16, i32 0
  %i.16 = insertelement <16 x i8> %in, i8 %e.16, i32 16
  ; CHECK-NEXT: ret <16 x i8> %i.16
  ret <16 x i8> %i.16
}

define <8 x i16> @test_8xi16_out_of_range(<8 x i16> %in) {
  ; CHECK-LABEL: test_8xi16_out_of_range
  ; CHECK-NEXT: %e.8 = extractelement <8 x i16> %in, i32 0
  %e.8 = extractelement <8 x i16> %in, i32 8
  ; CHECK-NEXT: %i.8 = insertelement <8 x i16> %in, i16 %e.8, i32 0
  %i.8 = insertelement <8 x i16> %in, i16 %e.8, i32 8
  ; CHECK-NEXT: ret <8 x i16> %i.8
  ret <8 x i16> %i.8
}

define <4 x i32> @test_4xi32_out_of_range(<4 x i32> %in) {
  ; CHECK-LABEL: test_4xi32_out_of_range
  ; CHECK-NEXT: %e.4 = extractelement <4 x i32> %in, i32 0
  %e.4 = extractelement <4 x i32> %in, i32 4
  ; CHECK-NEXT: %i.4 = insertelement <4 x i32> %in, i32 %e.4, i32 0
  %i.4 = insertelement <4 x i32> %in, i32 %e.4, i32 4
  ; CHECK-NEXT: ret <4 x i32> %i.4
  ret <4 x i32> %i.4
}

define <4 x float> @test_4xfloat_out_of_range(<4 x float> %in) {
  ; CHECK-LABEL: test_4xfloat_out_of_range
  ; CHECK-NEXT: %e.4 = extractelement <4 x float> %in, i32 0
  %e.4 = extractelement <4 x float> %in, i32 4
  ; CHECK-NEXT: %i.4 = insertelement <4 x float> %in, float %e.4, i32 0
  %i.4 = insertelement <4 x float> %in, float %e.4, i32 4
  ; CHECK-NEXT: ret <4 x float> %i.4
  ret <4 x float> %i.4
}

define <4 x i32> @test_4xi32_out_of_range_urem(<4 x i32> %in) {
  ; CHECK-LABEL: test_4xi32_out_of_range_urem
  %e.4 = extractelement <4 x i32> %in, i32 4 ; CHECK-NEXT: {{.*}} extractelement {{.*}} i32 0
  %e.5 = extractelement <4 x i32> %in, i32 5 ; CHECK-NEXT: {{.*}} extractelement {{.*}} i32 1
  %e.6 = extractelement <4 x i32> %in, i32 6 ; CHECK-NEXT: {{.*}} extractelement {{.*}} i32 2
  %e.7 = extractelement <4 x i32> %in, i32 7 ; CHECK-NEXT: {{.*}} extractelement {{.*}} i32 3
  %e.8 = extractelement <4 x i32> %in, i32 8 ; CHECK-NEXT: {{.*}} extractelement {{.*}} i32 0
  %i.4 = insertelement <4 x i32> %in, i32 %e.4, i32 4 ; CHECK-NEXT: {{.*}} insertelement {{.*}} i32 0
  %i.5 = insertelement <4 x i32> %in, i32 %e.5, i32 5 ; CHECK-NEXT: {{.*}} insertelement {{.*}} i32 1
  %i.6 = insertelement <4 x i32> %in, i32 %e.6, i32 6 ; CHECK-NEXT: {{.*}} insertelement {{.*}} i32 2
  %i.7 = insertelement <4 x i32> %in, i32 %e.7, i32 7 ; CHECK-NEXT: {{.*}} insertelement {{.*}} i32 3
  %i.8 = insertelement <4 x i32> %in, i32 %e.8, i32 8 ; CHECK-NEXT: {{.*}} insertelement {{.*}} i32 0
  ; CHECK-NEXT: ret <4 x i32> %i.4
  ret <4 x i32> %i.4
}

; The following insert/extract elements are all indexed with a variable,
; they should get modified.

define <16 x i1> @test_16xi1_variable(<16 x i1> %in, i32 %idx) {
  ; CHECK-LABEL: test_16xi1_variable
  ; CHECK-NEXT: %[[EALLOCA:[0-9]+]] = alloca i1, i32 16, align 16
  ; CHECK-NEXT: %[[ECAST:[0-9]+]] = bitcast i1* %[[EALLOCA]] to <16 x i1>*
  ; CHECK-NEXT: store <16 x i1> %in, <16 x i1>* %[[ECAST]], align 16
  ; CHECK-NEXT: %[[EGEP:[0-9]+]] = getelementptr i1, i1* %[[EALLOCA]], i32 %idx
  ; CHECK-NEXT: %[[ELOAD:[0-9]+]] = load i1, i1* %[[EGEP]], align 1
  %e.16 = extractelement <16 x i1> %in, i32 %idx
  ; CHECK-NEXT: %[[IALLOCA:[0-9]+]] = alloca i1, i32 16, align 16
  ; CHECK-NEXT: %[[ICAST:[0-9]+]] = bitcast i1* %[[IALLOCA]] to <16 x i1>*
  ; CHECK-NEXT: store <16 x i1> %in, <16 x i1>* %[[ICAST]], align 16
  ; CHECK-NEXT: %[[IGEP:[0-9]+]] = getelementptr i1, i1* %[[IALLOCA]], i32 %idx
  ; CHECK-NEXT: store i1 %[[ELOAD]], i1* %[[IGEP]], align 1
  ; CHECK-NEXT: %[[ILOAD:[0-9]+]] = load <16 x i1>, <16 x i1>* %[[ICAST]], align 16
  %i.16 = insertelement <16 x i1> %in, i1 %e.16, i32 %idx
  ; CHECK-NEXT: ret <16 x i1> %[[ILOAD]]
  ret <16 x i1> %i.16
}

define <8 x i1> @test_8xi1_variable(<8 x i1> %in, i32 %idx) {
  ; CHECK-LABEL: test_8xi1_variable
  ; CHECK-NEXT: %[[EALLOCA:[0-9]+]] = alloca i1, i32 8, align 8
  ; CHECK-NEXT: %[[ECAST:[0-9]+]] = bitcast i1* %[[EALLOCA]] to <8 x i1>*
  ; CHECK-NEXT: store <8 x i1> %in, <8 x i1>* %[[ECAST]], align 8
  ; CHECK-NEXT: %[[EGEP:[0-9]+]] = getelementptr i1, i1* %[[EALLOCA]], i32 %idx
  ; CHECK-NEXT: %[[ELOAD:[0-9]+]] = load i1, i1* %[[EGEP]], align 1
  %e.8 = extractelement <8 x i1> %in, i32 %idx
  ; CHECK-NEXT: %[[IALLOCA:[0-9]+]] = alloca i1, i32 8, align 8
  ; CHECK-NEXT: %[[ICAST:[0-9]+]] = bitcast i1* %[[IALLOCA]] to <8 x i1>*
  ; CHECK-NEXT: store <8 x i1> %in, <8 x i1>* %[[ICAST]], align 8
  ; CHECK-NEXT: %[[IGEP:[0-9]+]] = getelementptr i1, i1* %[[IALLOCA]], i32 %idx
  ; CHECK-NEXT: store i1 %[[ELOAD]], i1* %[[IGEP]], align 1
  ; CHECK-NEXT: %[[ILOAD:[0-9]+]] = load <8 x i1>, <8 x i1>* %[[ICAST]], align 8
  %i.8 = insertelement <8 x i1> %in, i1 %e.8, i32 %idx
  ; CHECK-NEXT: ret <8 x i1> %[[ILOAD]]
  ret <8 x i1> %i.8
}

define <4 x i1> @test_4xi1_variable(<4 x i1> %in, i32 %idx) {
  ; CHECK-LABEL: test_4xi1_variable
  ; CHECK-NEXT: %[[EALLOCA:[0-9]+]] = alloca i1, i32 4, align 4
  ; CHECK-NEXT: %[[ECAST:[0-9]+]] = bitcast i1* %[[EALLOCA]] to <4 x i1>*
  ; CHECK-NEXT: store <4 x i1> %in, <4 x i1>* %[[ECAST]], align 4
  ; CHECK-NEXT: %[[EGEP:[0-9]+]] = getelementptr i1, i1* %[[EALLOCA]], i32 %idx
  ; CHECK-NEXT: %[[ELOAD:[0-9]+]] = load i1, i1* %[[EGEP]], align 1
  %e.4 = extractelement <4 x i1> %in, i32 %idx
  ; CHECK-NEXT: %[[IALLOCA:[0-9]+]] = alloca i1, i32 4, align 4
  ; CHECK-NEXT: %[[ICAST:[0-9]+]] = bitcast i1* %[[IALLOCA]] to <4 x i1>*
  ; CHECK-NEXT: store <4 x i1> %in, <4 x i1>* %[[ICAST]], align 4
  ; CHECK-NEXT: %[[IGEP:[0-9]+]] = getelementptr i1, i1* %[[IALLOCA]], i32 %idx
  ; CHECK-NEXT: store i1 %[[ELOAD]], i1* %[[IGEP]], align 1
  ; CHECK-NEXT: %[[ILOAD:[0-9]+]] = load <4 x i1>, <4 x i1>* %[[ICAST]], align 4
  %i.4 = insertelement <4 x i1> %in, i1 %e.4, i32 %idx
  ; CHECK-NEXT: ret <4 x i1> %[[ILOAD]]
  ret <4 x i1> %i.4
}

define <16 x i8> @test_16xi8_variable(<16 x i8> %in, i32 %idx) {
  ; CHECK-LABEL: test_16xi8_variable
  ; CHECK-NEXT: %[[EALLOCA:[0-9]+]] = alloca i8, i32 16, align 4
  ; CHECK-NEXT: %[[ECAST:[0-9]+]] = bitcast i8* %[[EALLOCA]] to <16 x i8>*
  ; CHECK-NEXT: store <16 x i8> %in, <16 x i8>* %[[ECAST]], align 4
  ; CHECK-NEXT: %[[EGEP:[0-9]+]] = getelementptr i8, i8* %[[EALLOCA]], i32 %idx
  ; CHECK-NEXT: %[[ELOAD:[0-9]+]] = load i8, i8* %[[EGEP]], align 1
  %e.16 = extractelement <16 x i8> %in, i32 %idx
  ; CHECK-NEXT: %[[IALLOCA:[0-9]+]] = alloca i8, i32 16, align 4
  ; CHECK-NEXT: %[[ICAST:[0-9]+]] = bitcast i8* %[[IALLOCA]] to <16 x i8>*
  ; CHECK-NEXT: store <16 x i8> %in, <16 x i8>* %[[ICAST]], align 4
  ; CHECK-NEXT: %[[IGEP:[0-9]+]] = getelementptr i8, i8* %[[IALLOCA]], i32 %idx
  ; CHECK-NEXT: store i8 %[[ELOAD]], i8* %[[IGEP]], align 1
  ; CHECK-NEXT: %[[ILOAD:[0-9]+]] = load <16 x i8>, <16 x i8>* %[[ICAST]], align 4
  %i.16 = insertelement <16 x i8> %in, i8 %e.16, i32 %idx
  ; CHECK-NEXT: ret <16 x i8> %[[ILOAD]]
  ret <16 x i8> %i.16
}

define <8 x i16> @test_8xi16_variable(<8 x i16> %in, i32 %idx) {
  ; CHECK-LABEL: test_8xi16_variable
  ; CHECK-NEXT: %[[EALLOCA:[0-9]+]] = alloca i16, i32 8, align 4
  ; CHECK-NEXT: %[[ECAST:[0-9]+]] = bitcast i16* %[[EALLOCA]] to <8 x i16>*
  ; CHECK-NEXT: store <8 x i16> %in, <8 x i16>* %[[ECAST]], align 4
  ; CHECK-NEXT: %[[EGEP:[0-9]+]] = getelementptr i16, i16* %[[EALLOCA]], i32 %idx
  ; CHECK-NEXT: %[[ELOAD:[0-9]+]] = load i16, i16* %[[EGEP]], align 2
  %e.8 = extractelement <8 x i16> %in, i32 %idx
  ; CHECK-NEXT: %[[IALLOCA:[0-9]+]] = alloca i16, i32 8, align 4
  ; CHECK-NEXT: %[[ICAST:[0-9]+]] = bitcast i16* %[[IALLOCA]] to <8 x i16>*
  ; CHECK-NEXT: store <8 x i16> %in, <8 x i16>* %[[ICAST]], align 4
  ; CHECK-NEXT: %[[IGEP:[0-9]+]] = getelementptr i16, i16* %[[IALLOCA]], i32 %idx
  ; CHECK-NEXT: store i16 %[[ELOAD]], i16* %[[IGEP]], align 2
  ; CHECK-NEXT: %[[ILOAD:[0-9]+]] = load <8 x i16>, <8 x i16>* %[[ICAST]], align 4
  %i.8 = insertelement <8 x i16> %in, i16 %e.8, i32 %idx
  ; CHECK-NEXT: ret <8 x i16> %[[ILOAD]]
  ret <8 x i16> %i.8
}

define <4 x i32> @test_4xi32_variable(<4 x i32> %in, i32 %idx) {
  ; CHECK-LABEL: test_4xi32_variable
  ; CHECK-NEXT: %[[EALLOCA:[0-9]+]] = alloca i32, i32 4, align 4
  ; CHECK-NEXT: %[[ECAST:[0-9]+]] = bitcast i32* %[[EALLOCA]] to <4 x i32>*
  ; CHECK-NEXT: store <4 x i32> %in, <4 x i32>* %[[ECAST]], align 4
  ; CHECK-NEXT: %[[EGEP:[0-9]+]] = getelementptr i32, i32* %[[EALLOCA]], i32 %idx
  ; CHECK-NEXT: %[[ELOAD:[0-9]+]] = load i32, i32* %[[EGEP]], align 4
  %e.4 = extractelement <4 x i32> %in, i32 %idx
  ; CHECK-NEXT: %[[IALLOCA:[0-9]+]] = alloca i32, i32 4, align 4
  ; CHECK-NEXT: %[[ICAST:[0-9]+]] = bitcast i32* %[[IALLOCA]] to <4 x i32>*
  ; CHECK-NEXT: store <4 x i32> %in, <4 x i32>* %[[ICAST]], align 4
  ; CHECK-NEXT: %[[IGEP:[0-9]+]] = getelementptr i32, i32* %[[IALLOCA]], i32 %idx
  ; CHECK-NEXT: store i32 %[[ELOAD]], i32* %[[IGEP]], align 4
  ; CHECK-NEXT: %[[ILOAD:[0-9]+]] = load <4 x i32>, <4 x i32>* %[[ICAST]], align 4
  %i.4 = insertelement <4 x i32> %in, i32 %e.4, i32 %idx
  ; CHECK-NEXT: ret <4 x i32> %[[ILOAD]]
  ret <4 x i32> %i.4
}

define <4 x float> @test_4xfloat_variable(<4 x float> %in, i32 %idx) {
  ; CHECK-LABEL: test_4xfloat_variable
  ; CHECK-NEXT: %[[EALLOCA:[0-9]+]] = alloca float, i32 4, align 4
  ; CHECK-NEXT: %[[ECAST:[0-9]+]] = bitcast float* %[[EALLOCA]] to <4 x float>*
  ; CHECK-NEXT: store <4 x float> %in, <4 x float>* %[[ECAST]], align 4
  ; CHECK-NEXT: %[[EGEP:[0-9]+]] = getelementptr float, float* %[[EALLOCA]], i32 %idx
  ; CHECK-NEXT: %[[ELOAD:[0-9]+]] = load float, float* %[[EGEP]], align 4
  %e.4 = extractelement <4 x float> %in, i32 %idx
  ; CHECK-NEXT: %[[IALLOCA:[0-9]+]] = alloca float, i32 4, align 4
  ; CHECK-NEXT: %[[ICAST:[0-9]+]] = bitcast float* %[[IALLOCA]] to <4 x float>*
  ; CHECK-NEXT: store <4 x float> %in, <4 x float>* %[[ICAST]], align 4
  ; CHECK-NEXT: %[[IGEP:[0-9]+]] = getelementptr float, float* %[[IALLOCA]], i32 %idx
  ; CHECK-NEXT: store float %[[ELOAD]], float* %[[IGEP]], align 4
  ; CHECK-NEXT: %[[ILOAD:[0-9]+]] = load <4 x float>, <4 x float>* %[[ICAST]], align 4
  %i.4 = insertelement <4 x float> %in, float %e.4, i32 %idx
  ; CHECK-NEXT: ret <4 x float> %[[ILOAD]]
  ret <4 x float> %i.4
}
