; RUN: opt -fix-vector-load-store-alignment %s -S | FileCheck %s

; Test that vector load/store get converted to memory accesses of the
; underlying elements, with proper alignment.

; The datalayout is needed to determine the alignment of the load/stores.
target datalayout = "e-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-p:32:32:32-v128:32:32"

; Load =========================================================================

define <4 x i1> @test_load_4xi1(<4 x i1>* %loc) {
  ; CHECK-LABEL: test_load_4xi1
  ; CHECK-NEXT: %[[BASE:[0-9]+]] = bitcast <4 x i1>* %loc to i1*
  ; CHECK-NEXT: %[[GEP0:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 0
  ; CHECK-NEXT: %[[LD0:[0-9]+]] = load i1* %[[GEP0]], align 4
  ; CHECK-NEXT: %[[INS0:[0-9]+]] = insertelement <4 x i1> undef, i1 %[[LD0]], i32 0
  ; CHECK-NEXT: %[[GEP1:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 1
  ; CHECK-NEXT: %[[LD1:[0-9]+]] = load i1* %[[GEP1]], align 1
  ; CHECK-NEXT: %[[INS1:[0-9]+]] = insertelement <4 x i1> %[[INS0]], i1 %[[LD1]], i32 1
  ; CHECK-NEXT: %[[GEP2:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 2
  ; CHECK-NEXT: %[[LD2:[0-9]+]] = load i1* %[[GEP2]], align 2
  ; CHECK-NEXT: %[[INS2:[0-9]+]] = insertelement <4 x i1> %[[INS1]], i1 %[[LD2]], i32 2
  ; CHECK-NEXT: %[[GEP3:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 3
  ; CHECK-NEXT: %[[LD3:[0-9]+]] = load i1* %[[GEP3]], align 1
  ; CHECK-NEXT: %[[INS3:[0-9]+]] = insertelement <4 x i1> %[[INS2]], i1 %[[LD3]], i32 3
  ; CHECK-NEXT: ret <4 x i1> %[[INS3]]
  %loaded = load <4 x i1>* %loc
  ret <4 x i1> %loaded
}

define <8 x i1> @test_load_8xi1(<8 x i1>* %loc) {
  ; CHECK-LABEL: test_load_8xi1
  ; CHECK-NEXT: %[[BASE:[0-9]+]] = bitcast <8 x i1>* %loc to i1*
  ; CHECK-NEXT: %[[GEP0:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 0
  ; CHECK-NEXT: %[[LD0:[0-9]+]] = load i1* %[[GEP0]], align 8
  ; CHECK-NEXT: %[[INS0:[0-9]+]] = insertelement <8 x i1> undef, i1 %[[LD0]], i32 0
  ; CHECK-NEXT: %[[GEP1:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 1
  ; CHECK-NEXT: %[[LD1:[0-9]+]] = load i1* %[[GEP1]], align 1
  ; CHECK-NEXT: %[[INS1:[0-9]+]] = insertelement <8 x i1> %[[INS0]], i1 %[[LD1]], i32 1
  ; CHECK-NEXT: %[[GEP2:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 2
  ; CHECK-NEXT: %[[LD2:[0-9]+]] = load i1* %[[GEP2]], align 2
  ; CHECK-NEXT: %[[INS2:[0-9]+]] = insertelement <8 x i1> %[[INS1]], i1 %[[LD2]], i32 2
  ; CHECK-NEXT: %[[GEP3:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 3
  ; CHECK-NEXT: %[[LD3:[0-9]+]] = load i1* %[[GEP3]], align 1
  ; CHECK-NEXT: %[[INS3:[0-9]+]] = insertelement <8 x i1> %[[INS2]], i1 %[[LD3]], i32 3
  ; CHECK-NEXT: %[[GEP4:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 4
  ; CHECK-NEXT: %[[LD4:[0-9]+]] = load i1* %[[GEP4]], align 4
  ; CHECK-NEXT: %[[INS4:[0-9]+]] = insertelement <8 x i1> %[[INS3]], i1 %[[LD4]], i32 4
  ; CHECK-NEXT: %[[GEP5:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 5
  ; CHECK-NEXT: %[[LD5:[0-9]+]] = load i1* %[[GEP5]], align 1
  ; CHECK-NEXT: %[[INS5:[0-9]+]] = insertelement <8 x i1> %[[INS4]], i1 %[[LD5]], i32 5
  ; CHECK-NEXT: %[[GEP6:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 6
  ; CHECK-NEXT: %[[LD6:[0-9]+]] = load i1* %[[GEP6]], align 2
  ; CHECK-NEXT: %[[INS6:[0-9]+]] = insertelement <8 x i1> %[[INS5]], i1 %[[LD6]], i32 6
  ; CHECK-NEXT: %[[GEP7:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 7
  ; CHECK-NEXT: %[[LD7:[0-9]+]] = load i1* %[[GEP7]], align 1
  ; CHECK-NEXT: %[[INS7:[0-9]+]] = insertelement <8 x i1> %[[INS6]], i1 %[[LD7]], i32 7
  ; CHECK-NEXT: ret <8 x i1> %[[INS7]]
  %loaded = load <8 x i1>* %loc
  ret <8 x i1> %loaded
}

define <16 x i1> @test_load_16xi1(<16 x i1>* %loc) {
  ; CHECK-LABEL: test_load_16xi1
  ; CHECK-NEXT: %[[BASE:[0-9]+]] = bitcast <16 x i1>* %loc to i1*
  ; CHECK-NEXT: %[[GEP0:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 0
  ; CHECK-NEXT: %[[LD0:[0-9]+]] = load i1* %[[GEP0]], align 16
  ; CHECK-NEXT: %[[INS0:[0-9]+]] = insertelement <16 x i1> undef, i1 %[[LD0]], i32 0
  ; CHECK-NEXT: %[[GEP1:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 1
  ; CHECK-NEXT: %[[LD1:[0-9]+]] = load i1* %[[GEP1]], align 1
  ; CHECK-NEXT: %[[INS1:[0-9]+]] = insertelement <16 x i1> %[[INS0]], i1 %[[LD1]], i32 1
  ; CHECK-NEXT: %[[GEP2:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 2
  ; CHECK-NEXT: %[[LD2:[0-9]+]] = load i1* %[[GEP2]], align 2
  ; CHECK-NEXT: %[[INS2:[0-9]+]] = insertelement <16 x i1> %[[INS1]], i1 %[[LD2]], i32 2
  ; CHECK-NEXT: %[[GEP3:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 3
  ; CHECK-NEXT: %[[LD3:[0-9]+]] = load i1* %[[GEP3]], align 1
  ; CHECK-NEXT: %[[INS3:[0-9]+]] = insertelement <16 x i1> %[[INS2]], i1 %[[LD3]], i32 3
  ; CHECK-NEXT: %[[GEP4:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 4
  ; CHECK-NEXT: %[[LD4:[0-9]+]] = load i1* %[[GEP4]], align 4
  ; CHECK-NEXT: %[[INS4:[0-9]+]] = insertelement <16 x i1> %[[INS3]], i1 %[[LD4]], i32 4
  ; CHECK-NEXT: %[[GEP5:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 5
  ; CHECK-NEXT: %[[LD5:[0-9]+]] = load i1* %[[GEP5]], align 1
  ; CHECK-NEXT: %[[INS5:[0-9]+]] = insertelement <16 x i1> %[[INS4]], i1 %[[LD5]], i32 5
  ; CHECK-NEXT: %[[GEP6:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 6
  ; CHECK-NEXT: %[[LD6:[0-9]+]] = load i1* %[[GEP6]], align 2
  ; CHECK-NEXT: %[[INS6:[0-9]+]] = insertelement <16 x i1> %[[INS5]], i1 %[[LD6]], i32 6
  ; CHECK-NEXT: %[[GEP7:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 7
  ; CHECK-NEXT: %[[LD7:[0-9]+]] = load i1* %[[GEP7]], align 1
  ; CHECK-NEXT: %[[INS7:[0-9]+]] = insertelement <16 x i1> %[[INS6]], i1 %[[LD7]], i32 7
  ; CHECK-NEXT: %[[GEP8:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 8
  ; CHECK-NEXT: %[[LD8:[0-9]+]] = load i1* %[[GEP8]], align 8
  ; CHECK-NEXT: %[[INS8:[0-9]+]] = insertelement <16 x i1> %[[INS7]], i1 %[[LD8]], i32 8
  ; CHECK-NEXT: %[[GEP9:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 9
  ; CHECK-NEXT: %[[LD9:[0-9]+]] = load i1* %[[GEP9]], align 1
  ; CHECK-NEXT: %[[INS9:[0-9]+]] = insertelement <16 x i1> %[[INS8]], i1 %[[LD9]], i32 9
  ; CHECK-NEXT: %[[GEP10:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 10
  ; CHECK-NEXT: %[[LD10:[0-9]+]] = load i1* %[[GEP10]], align 2
  ; CHECK-NEXT: %[[INS10:[0-9]+]] = insertelement <16 x i1> %[[INS9]], i1 %[[LD10]], i32 10
  ; CHECK-NEXT: %[[GEP11:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 11
  ; CHECK-NEXT: %[[LD11:[0-9]+]] = load i1* %[[GEP11]], align 1
  ; CHECK-NEXT: %[[INS11:[0-9]+]] = insertelement <16 x i1> %[[INS10]], i1 %[[LD11]], i32 11
  ; CHECK-NEXT: %[[GEP12:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 12
  ; CHECK-NEXT: %[[LD12:[0-9]+]] = load i1* %[[GEP12]], align 4
  ; CHECK-NEXT: %[[INS12:[0-9]+]] = insertelement <16 x i1> %[[INS11]], i1 %[[LD12]], i32 12
  ; CHECK-NEXT: %[[GEP13:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 13
  ; CHECK-NEXT: %[[LD13:[0-9]+]] = load i1* %[[GEP13]], align 1
  ; CHECK-NEXT: %[[INS13:[0-9]+]] = insertelement <16 x i1> %[[INS12]], i1 %[[LD13]], i32 13
  ; CHECK-NEXT: %[[GEP14:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 14
  ; CHECK-NEXT: %[[LD14:[0-9]+]] = load i1* %[[GEP14]], align 2
  ; CHECK-NEXT: %[[INS14:[0-9]+]] = insertelement <16 x i1> %[[INS13]], i1 %[[LD14]], i32 14
  ; CHECK-NEXT: %[[GEP15:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 15
  ; CHECK-NEXT: %[[LD15:[0-9]+]] = load i1* %[[GEP15]], align 1
  ; CHECK-NEXT: %[[INS15:[0-9]+]] = insertelement <16 x i1> %[[INS14]], i1 %[[LD15]], i32 15
  ; CHECK-NEXT: ret <16 x i1> %[[INS15]]
  %loaded = load <16 x i1>* %loc
  ret <16 x i1> %loaded
}

define <4 x i32> @test_load_4xi32(<4 x i32>* %loc) {
  ; CHECK-LABEL: test_load_4xi32
  ; CHECK-NEXT: %[[BASE:[0-9]+]] = bitcast <4 x i32>* %loc to i32*
  ; CHECK-NEXT: %[[GEP0:[0-9]+]] = getelementptr inbounds i32* %[[BASE]], i32 0
  ; CHECK-NEXT: %[[LD0:[0-9]+]] = load i32* %[[GEP0]], align 4
  ; CHECK-NEXT: %[[INS0:[0-9]+]] = insertelement <4 x i32> undef, i32 %[[LD0]], i32 0
  ; CHECK-NEXT: %[[GEP1:[0-9]+]] = getelementptr inbounds i32* %[[BASE]], i32 1
  ; CHECK-NEXT: %[[LD1:[0-9]+]] = load i32* %[[GEP1]], align 4
  ; CHECK-NEXT: %[[INS1:[0-9]+]] = insertelement <4 x i32> %[[INS0]], i32 %[[LD1]], i32 1
  ; CHECK-NEXT: %[[GEP2:[0-9]+]] = getelementptr inbounds i32* %[[BASE]], i32 2
  ; CHECK-NEXT: %[[LD2:[0-9]+]] = load i32* %[[GEP2]], align 4
  ; CHECK-NEXT: %[[INS2:[0-9]+]] = insertelement <4 x i32> %[[INS1]], i32 %[[LD2]], i32 2
  ; CHECK-NEXT: %[[GEP3:[0-9]+]] = getelementptr inbounds i32* %[[BASE]], i32 3
  ; CHECK-NEXT: %[[LD3:[0-9]+]] = load i32* %[[GEP3]], align 4
  ; CHECK-NEXT: %[[INS3:[0-9]+]] = insertelement <4 x i32> %[[INS2]], i32 %[[LD3]], i32 3
  ; CHECK-NEXT: ret <4 x i32> %[[INS3]]
  %loaded = load <4 x i32>* %loc
  ret <4 x i32> %loaded
}

define <4 x float> @test_load_4xfloat(<4 x float>* %loc) {
  ; CHECK-LABEL: test_load_4xfloat
  ; CHECK-NEXT: %[[BASE:[0-9]+]] = bitcast <4 x float>* %loc to float*
  ; CHECK-NEXT: %[[GEP0:[0-9]+]] = getelementptr inbounds float* %[[BASE]], i32 0
  ; CHECK-NEXT: %[[LD0:[0-9]+]] = load float* %[[GEP0]], align 4
  ; CHECK-NEXT: %[[INS0:[0-9]+]] = insertelement <4 x float> undef, float %[[LD0]], i32 0
  ; CHECK-NEXT: %[[GEP1:[0-9]+]] = getelementptr inbounds float* %[[BASE]], i32 1
  ; CHECK-NEXT: %[[LD1:[0-9]+]] = load float* %[[GEP1]], align 4
  ; CHECK-NEXT: %[[INS1:[0-9]+]] = insertelement <4 x float> %[[INS0]], float %[[LD1]], i32 1
  ; CHECK-NEXT: %[[GEP2:[0-9]+]] = getelementptr inbounds float* %[[BASE]], i32 2
  ; CHECK-NEXT: %[[LD2:[0-9]+]] = load float* %[[GEP2]], align 4
  ; CHECK-NEXT: %[[INS2:[0-9]+]] = insertelement <4 x float> %[[INS1]], float %[[LD2]], i32 2
  ; CHECK-NEXT: %[[GEP3:[0-9]+]] = getelementptr inbounds float* %[[BASE]], i32 3
  ; CHECK-NEXT: %[[LD3:[0-9]+]] = load float* %[[GEP3]], align 4
  ; CHECK-NEXT: %[[INS3:[0-9]+]] = insertelement <4 x float> %[[INS2]], float %[[LD3]], i32 3
  ; CHECK-NEXT: ret <4 x float> %[[INS3]]
  %loaded = load <4 x float>* %loc
  ret <4 x float> %loaded
}

define <8 x i16> @test_load_8xi16(<8 x i16>* %loc) {
  ; CHECK-LABEL: test_load_8xi16
  ; CHECK-NEXT: %[[BASE:[0-9]+]] = bitcast <8 x i16>* %loc to i16*
  ; CHECK-NEXT: %[[GEP0:[0-9]+]] = getelementptr inbounds i16* %[[BASE]], i32 0
  ; CHECK-NEXT: %[[LD0:[0-9]+]] = load i16* %[[GEP0]], align 4
  ; CHECK-NEXT: %[[INS0:[0-9]+]] = insertelement <8 x i16> undef, i16 %[[LD0]], i32 0
  ; CHECK-NEXT: %[[GEP1:[0-9]+]] = getelementptr inbounds i16* %[[BASE]], i32 1
  ; CHECK-NEXT: %[[LD1:[0-9]+]] = load i16* %[[GEP1]], align 2
  ; CHECK-NEXT: %[[INS1:[0-9]+]] = insertelement <8 x i16> %[[INS0]], i16 %[[LD1]], i32 1
  ; CHECK-NEXT: %[[GEP2:[0-9]+]] = getelementptr inbounds i16* %[[BASE]], i32 2
  ; CHECK-NEXT: %[[LD2:[0-9]+]] = load i16* %[[GEP2]], align 4
  ; CHECK-NEXT: %[[INS2:[0-9]+]] = insertelement <8 x i16> %[[INS1]], i16 %[[LD2]], i32 2
  ; CHECK-NEXT: %[[GEP3:[0-9]+]] = getelementptr inbounds i16* %[[BASE]], i32 3
  ; CHECK-NEXT: %[[LD3:[0-9]+]] = load i16* %[[GEP3]], align 2
  ; CHECK-NEXT: %[[INS3:[0-9]+]] = insertelement <8 x i16> %[[INS2]], i16 %[[LD3]], i32 3
  ; CHECK-NEXT: %[[GEP4:[0-9]+]] = getelementptr inbounds i16* %[[BASE]], i32 4
  ; CHECK-NEXT: %[[LD4:[0-9]+]] = load i16* %[[GEP4]], align 4
  ; CHECK-NEXT: %[[INS4:[0-9]+]] = insertelement <8 x i16> %[[INS3]], i16 %[[LD4]], i32 4
  ; CHECK-NEXT: %[[GEP5:[0-9]+]] = getelementptr inbounds i16* %[[BASE]], i32 5
  ; CHECK-NEXT: %[[LD5:[0-9]+]] = load i16* %[[GEP5]], align 2
  ; CHECK-NEXT: %[[INS5:[0-9]+]] = insertelement <8 x i16> %[[INS4]], i16 %[[LD5]], i32 5
  ; CHECK-NEXT: %[[GEP6:[0-9]+]] = getelementptr inbounds i16* %[[BASE]], i32 6
  ; CHECK-NEXT: %[[LD6:[0-9]+]] = load i16* %[[GEP6]], align 4
  ; CHECK-NEXT: %[[INS6:[0-9]+]] = insertelement <8 x i16> %[[INS5]], i16 %[[LD6]], i32 6
  ; CHECK-NEXT: %[[GEP7:[0-9]+]] = getelementptr inbounds i16* %[[BASE]], i32 7
  ; CHECK-NEXT: %[[LD7:[0-9]+]] = load i16* %[[GEP7]], align 2
  ; CHECK-NEXT: %[[INS7:[0-9]+]] = insertelement <8 x i16> %[[INS6]], i16 %[[LD7]], i32 7
  ; CHECK-NEXT: ret <8 x i16> %[[INS7]]
  %loaded = load <8 x i16>* %loc
  ret <8 x i16> %loaded
}

define <16 x i8> @test_load_16xi8(<16 x i8>* %loc) {
  ; CHECK-LABEL: test_load_16xi8
  ; CHECK-NEXT: %[[BASE:[0-9]+]] = bitcast <16 x i8>* %loc to i8*
  ; CHECK-NEXT: %[[GEP0:[0-9]+]] = getelementptr inbounds i8* %[[BASE]], i32 0
  ; CHECK-NEXT: %[[LD0:[0-9]+]] = load i8* %[[GEP0]], align 4
  ; CHECK-NEXT: %[[INS0:[0-9]+]] = insertelement <16 x i8> undef, i8 %[[LD0]], i32 0
  ; CHECK-NEXT: %[[GEP1:[0-9]+]] = getelementptr inbounds i8* %[[BASE]], i32 1
  ; CHECK-NEXT: %[[LD1:[0-9]+]] = load i8* %[[GEP1]], align 1
  ; CHECK-NEXT: %[[INS1:[0-9]+]] = insertelement <16 x i8> %[[INS0]], i8 %[[LD1]], i32 1
  ; CHECK-NEXT: %[[GEP2:[0-9]+]] = getelementptr inbounds i8* %[[BASE]], i32 2
  ; CHECK-NEXT: %[[LD2:[0-9]+]] = load i8* %[[GEP2]], align 2
  ; CHECK-NEXT: %[[INS2:[0-9]+]] = insertelement <16 x i8> %[[INS1]], i8 %[[LD2]], i32 2
  ; CHECK-NEXT: %[[GEP3:[0-9]+]] = getelementptr inbounds i8* %[[BASE]], i32 3
  ; CHECK-NEXT: %[[LD3:[0-9]+]] = load i8* %[[GEP3]], align 1
  ; CHECK-NEXT: %[[INS3:[0-9]+]] = insertelement <16 x i8> %[[INS2]], i8 %[[LD3]], i32 3
  ; CHECK-NEXT: %[[GEP4:[0-9]+]] = getelementptr inbounds i8* %[[BASE]], i32 4
  ; CHECK-NEXT: %[[LD4:[0-9]+]] = load i8* %[[GEP4]], align 4
  ; CHECK-NEXT: %[[INS4:[0-9]+]] = insertelement <16 x i8> %[[INS3]], i8 %[[LD4]], i32 4
  ; CHECK-NEXT: %[[GEP5:[0-9]+]] = getelementptr inbounds i8* %[[BASE]], i32 5
  ; CHECK-NEXT: %[[LD5:[0-9]+]] = load i8* %[[GEP5]], align 1
  ; CHECK-NEXT: %[[INS5:[0-9]+]] = insertelement <16 x i8> %[[INS4]], i8 %[[LD5]], i32 5
  ; CHECK-NEXT: %[[GEP6:[0-9]+]] = getelementptr inbounds i8* %[[BASE]], i32 6
  ; CHECK-NEXT: %[[LD6:[0-9]+]] = load i8* %[[GEP6]], align 2
  ; CHECK-NEXT: %[[INS6:[0-9]+]] = insertelement <16 x i8> %[[INS5]], i8 %[[LD6]], i32 6
  ; CHECK-NEXT: %[[GEP7:[0-9]+]] = getelementptr inbounds i8* %[[BASE]], i32 7
  ; CHECK-NEXT: %[[LD7:[0-9]+]] = load i8* %[[GEP7]], align 1
  ; CHECK-NEXT: %[[INS7:[0-9]+]] = insertelement <16 x i8> %[[INS6]], i8 %[[LD7]], i32 7
  ; CHECK-NEXT: %[[GEP8:[0-9]+]] = getelementptr inbounds i8* %[[BASE]], i32 8
  ; CHECK-NEXT: %[[LD8:[0-9]+]] = load i8* %[[GEP8]], align 4
  ; CHECK-NEXT: %[[INS8:[0-9]+]] = insertelement <16 x i8> %[[INS7]], i8 %[[LD8]], i32 8
  ; CHECK-NEXT: %[[GEP9:[0-9]+]] = getelementptr inbounds i8* %[[BASE]], i32 9
  ; CHECK-NEXT: %[[LD9:[0-9]+]] = load i8* %[[GEP9]], align 1
  ; CHECK-NEXT: %[[INS9:[0-9]+]] = insertelement <16 x i8> %[[INS8]], i8 %[[LD9]], i32 9
  ; CHECK-NEXT: %[[GEP10:[0-9]+]] = getelementptr inbounds i8* %[[BASE]], i32 10
  ; CHECK-NEXT: %[[LD10:[0-9]+]] = load i8* %[[GEP10]], align 2
  ; CHECK-NEXT: %[[INS10:[0-9]+]] = insertelement <16 x i8> %[[INS9]], i8 %[[LD10]], i32 10
  ; CHECK-NEXT: %[[GEP11:[0-9]+]] = getelementptr inbounds i8* %[[BASE]], i32 11
  ; CHECK-NEXT: %[[LD11:[0-9]+]] = load i8* %[[GEP11]], align 1
  ; CHECK-NEXT: %[[INS11:[0-9]+]] = insertelement <16 x i8> %[[INS10]], i8 %[[LD11]], i32 11
  ; CHECK-NEXT: %[[GEP12:[0-9]+]] = getelementptr inbounds i8* %[[BASE]], i32 12
  ; CHECK-NEXT: %[[LD12:[0-9]+]] = load i8* %[[GEP12]], align 4
  ; CHECK-NEXT: %[[INS12:[0-9]+]] = insertelement <16 x i8> %[[INS11]], i8 %[[LD12]], i32 12
  ; CHECK-NEXT: %[[GEP13:[0-9]+]] = getelementptr inbounds i8* %[[BASE]], i32 13
  ; CHECK-NEXT: %[[LD13:[0-9]+]] = load i8* %[[GEP13]], align 1
  ; CHECK-NEXT: %[[INS13:[0-9]+]] = insertelement <16 x i8> %[[INS12]], i8 %[[LD13]], i32 13
  ; CHECK-NEXT: %[[GEP14:[0-9]+]] = getelementptr inbounds i8* %[[BASE]], i32 14
  ; CHECK-NEXT: %[[LD14:[0-9]+]] = load i8* %[[GEP14]], align 2
  ; CHECK-NEXT: %[[INS14:[0-9]+]] = insertelement <16 x i8> %[[INS13]], i8 %[[LD14]], i32 14
  ; CHECK-NEXT: %[[GEP15:[0-9]+]] = getelementptr inbounds i8* %[[BASE]], i32 15
  ; CHECK-NEXT: %[[LD15:[0-9]+]] = load i8* %[[GEP15]], align 1
  ; CHECK-NEXT: %[[INS15:[0-9]+]] = insertelement <16 x i8> %[[INS14]], i8 %[[LD15]], i32 15
  ; CHECK-NEXT: ret <16 x i8> %[[INS15]]
  %loaded = load <16 x i8>* %loc
  ret <16 x i8> %loaded
}


; Store ========================================================================

define void @test_store_4xi1(<4 x i1> %val, <4 x i1>* %loc) {
  ; CHECK-LABEL: test_store_4xi1
  ; CHECK-NEXT: %[[BASE:[0-9]+]] = bitcast <4 x i1>* %loc to i1*
  ; CHECK-NEXT: %[[GEP0:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 0
  ; CHECK-NEXT: %[[EXT0:[0-9]+]] = extractelement <4 x i1> %val, i32 0
  ; CHECK-NEXT: store i1 %[[EXT0]], i1* %[[GEP0]], align 4
  ; CHECK-NEXT: %[[GEP1:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 1
  ; CHECK-NEXT: %[[EXT1:[0-9]+]] = extractelement <4 x i1> %val, i32 1
  ; CHECK-NEXT: store i1 %[[EXT1]], i1* %[[GEP1]], align 1
  ; CHECK-NEXT: %[[GEP2:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 2
  ; CHECK-NEXT: %[[EXT2:[0-9]+]] = extractelement <4 x i1> %val, i32 2
  ; CHECK-NEXT: store i1 %[[EXT2]], i1* %[[GEP2]], align 2
  ; CHECK-NEXT: %[[GEP3:[0-9]+]] = getelementptr inbounds i1* %[[BASE]], i32 3
  ; CHECK-NEXT: %[[EXT3:[0-9]+]] = extractelement <4 x i1> %val, i32 3
  ; CHECK-NEXT: store i1 %[[EXT3]], i1* %[[GEP3]], align 1
  ; CHECK-NEXT: ret void
  store <4 x i1> %val, <4 x i1>* %loc
  ret void
}

define void @test_store_4xi32(<4 x i32> %val, <4 x i32>* %loc) {
  ; CHECK-LABEL: test_store_4xi32
  ; CHECK-NEXT: %[[BASE:[0-9]+]] = bitcast <4 x i32>* %loc to i32*
  ; CHECK-NEXT: %[[GEP0:[0-9]+]] = getelementptr inbounds i32* %[[BASE]], i32 0
  ; CHECK-NEXT: %[[EXT0:[0-9]+]] = extractelement <4 x i32> %val, i32 0
  ; CHECK-NEXT: store i32 %[[EXT0]], i32* %[[GEP0]], align 4
  ; CHECK-NEXT: %[[GEP1:[0-9]+]] = getelementptr inbounds i32* %[[BASE]], i32 1
  ; CHECK-NEXT: %[[EXT1:[0-9]+]] = extractelement <4 x i32> %val, i32 1
  ; CHECK-NEXT: store i32 %[[EXT1]], i32* %[[GEP1]], align 4
  ; CHECK-NEXT: %[[GEP2:[0-9]+]] = getelementptr inbounds i32* %[[BASE]], i32 2
  ; CHECK-NEXT: %[[EXT2:[0-9]+]] = extractelement <4 x i32> %val, i32 2
  ; CHECK-NEXT: store i32 %[[EXT2]], i32* %[[GEP2]], align 4
  ; CHECK-NEXT: %[[GEP3:[0-9]+]] = getelementptr inbounds i32* %[[BASE]], i32 3
  ; CHECK-NEXT: %[[EXT3:[0-9]+]] = extractelement <4 x i32> %val, i32 3
  ; CHECK-NEXT: store i32 %[[EXT3]], i32* %[[GEP3]], align 4
  ; CHECK-NEXT: ret void
  store <4 x i32> %val, <4 x i32>* %loc
  ret void
}

define void @test_store_4xfloat(<4 x float> %val, <4 x float>* %loc) {
  ; CHECK-LABEL: test_store_4xfloat
  ; CHECK-NEXT: %[[BASE:[0-9]+]] = bitcast <4 x float>* %loc to float*
  ; CHECK-NEXT: %[[GEP0:[0-9]+]] = getelementptr inbounds float* %[[BASE]], i32 0
  ; CHECK-NEXT: %[[EXT0:[0-9]+]] = extractelement <4 x float> %val, i32 0
  ; CHECK-NEXT: store float %[[EXT0]], float* %[[GEP0]], align 4
  ; CHECK-NEXT: %[[GEP1:[0-9]+]] = getelementptr inbounds float* %[[BASE]], i32 1
  ; CHECK-NEXT: %[[EXT1:[0-9]+]] = extractelement <4 x float> %val, i32 1
  ; CHECK-NEXT: store float %[[EXT1]], float* %[[GEP1]], align 4
  ; CHECK-NEXT: %[[GEP2:[0-9]+]] = getelementptr inbounds float* %[[BASE]], i32 2
  ; CHECK-NEXT: %[[EXT2:[0-9]+]] = extractelement <4 x float> %val, i32 2
  ; CHECK-NEXT: store float %[[EXT2]], float* %[[GEP2]], align 4
  ; CHECK-NEXT: %[[GEP3:[0-9]+]] = getelementptr inbounds float* %[[BASE]], i32 3
  ; CHECK-NEXT: %[[EXT3:[0-9]+]] = extractelement <4 x float> %val, i32 3
  ; CHECK-NEXT: store float %[[EXT3]], float* %[[GEP3]], align 4
  ; CHECK-NEXT: ret void
  store <4 x float> %val, <4 x float>* %loc
  ret void
}


; Volatile =====================================================================

define <4 x i32> @test_volatile_load_4xi32(<4 x i32>* %loc) {
  ; CHECK-LABEL: test_volatile_load_4xi32
  ; CHECK-NEXT: %[[BASE:[0-9]+]] = bitcast <4 x i32>* %loc to i32*
  ; CHECK-NEXT: %[[GEP0:[0-9]+]] = getelementptr inbounds i32* %[[BASE]], i32 0
  ; CHECK-NEXT: %[[LD0:[0-9]+]] = load volatile i32* %[[GEP0]], align 4
  ; CHECK-NEXT: %[[INS0:[0-9]+]] = insertelement <4 x i32> undef, i32 %[[LD0]], i32 0
  ; CHECK-NEXT: %[[GEP1:[0-9]+]] = getelementptr inbounds i32* %[[BASE]], i32 1
  ; CHECK-NEXT: %[[LD1:[0-9]+]] = load volatile i32* %[[GEP1]], align 4
  ; CHECK-NEXT: %[[INS1:[0-9]+]] = insertelement <4 x i32> %[[INS0]], i32 %[[LD1]], i32 1
  ; CHECK-NEXT: %[[GEP2:[0-9]+]] = getelementptr inbounds i32* %[[BASE]], i32 2
  ; CHECK-NEXT: %[[LD2:[0-9]+]] = load volatile i32* %[[GEP2]], align 4
  ; CHECK-NEXT: %[[INS2:[0-9]+]] = insertelement <4 x i32> %[[INS1]], i32 %[[LD2]], i32 2
  ; CHECK-NEXT: %[[GEP3:[0-9]+]] = getelementptr inbounds i32* %[[BASE]], i32 3
  ; CHECK-NEXT: %[[LD3:[0-9]+]] = load volatile i32* %[[GEP3]], align 4
  ; CHECK-NEXT: %[[INS3:[0-9]+]] = insertelement <4 x i32> %[[INS2]], i32 %[[LD3]], i32 3
  ; CHECK-NEXT: ret <4 x i32> %[[INS3]]
  %loaded = load volatile <4 x i32>* %loc
  ret <4 x i32> %loaded
}

define void @test_volatile_store_4xi32(<4 x i32> %val, <4 x i32>* %loc) {
  ; CHECK-LABEL: test_volatile_store_4xi32
  ; CHECK-NEXT: %[[BASE:[0-9]+]] = bitcast <4 x i32>* %loc to i32*
  ; CHECK-NEXT: %[[GEP0:[0-9]+]] = getelementptr inbounds i32* %[[BASE]], i32 0
  ; CHECK-NEXT: %[[EXT0:[0-9]+]] = extractelement <4 x i32> %val, i32 0
  ; CHECK-NEXT: store volatile i32 %[[EXT0]], i32* %[[GEP0]], align 4
  ; CHECK-NEXT: %[[GEP1:[0-9]+]] = getelementptr inbounds i32* %[[BASE]], i32 1
  ; CHECK-NEXT: %[[EXT1:[0-9]+]] = extractelement <4 x i32> %val, i32 1
  ; CHECK-NEXT: store volatile i32 %[[EXT1]], i32* %[[GEP1]], align 4
  ; CHECK-NEXT: %[[GEP2:[0-9]+]] = getelementptr inbounds i32* %[[BASE]], i32 2
  ; CHECK-NEXT: %[[EXT2:[0-9]+]] = extractelement <4 x i32> %val, i32 2
  ; CHECK-NEXT: store volatile i32 %[[EXT2]], i32* %[[GEP2]], align 4
  ; CHECK-NEXT: %[[GEP3:[0-9]+]] = getelementptr inbounds i32* %[[BASE]], i32 3
  ; CHECK-NEXT: %[[EXT3:[0-9]+]] = extractelement <4 x i32> %val, i32 3
  ; CHECK-NEXT: store volatile i32 %[[EXT3]], i32* %[[GEP3]], align 4
  ; CHECK-NEXT: ret void
  store volatile <4 x i32> %val, <4 x i32>* %loc
  ret void
}
