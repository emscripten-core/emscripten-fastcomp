; Tests vector inserts/extracts.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-bccompress --remove-abbreviations \
; RUN:              | not pnacl-bcdis | FileCheck %s

define void @GoodVectorOps() {

; CHECK:         {{.*}}|    1: <65535, 11, 2>        |    constants {  // BlockID = 11
; CHECK-NEXT:    {{.*}}|      3: <1, 0>              |      i32:
; CHECK-NEXT:    {{.*}}|      3: <4, 2>              |        %c0 = i32 1;
; CHECK-NEXT:    {{.*}}|      3: <4, 0>              |        %c1 = i32 0;
; CHECK-NEXT:    {{.*}}|      3: <4, 20>             |        %c2 = i32 10;
; CHECK-NEXT:    {{.*}}|      3: <1, 1>              |      i1:
; CHECK-NEXT:    {{.*}}|      3: <4, 3>              |        %c3 = i1 1;
; CHECK-NEXT:    {{.*}}|      3: <4, 0>              |        %c4 = i1 0;
; CHECK-NEXT:    {{.*}}|      3: <1, 11>             |      <4 x i32>:
; CHECK-NEXT:    {{.*}}|      3: <3>                 |        %c5 = <4 x i32> undef;
; CHECK-NEXT:    {{.*}}|      3: <1, 3>              |      i8:
; CHECK-NEXT:    {{.*}}|      3: <4, 0>              |        %c6 = i8 0;
; CHECK-NEXT:    {{.*}}|      3: <4, 20>             |        %c7 = i8 10;
; CHECK-NEXT:    {{.*}}|      3: <1, 4>              |      i16:
; CHECK-NEXT:    {{.*}}|      3: <4, 2>              |        %c8 = i16 1;
; CHECK-NEXT:    {{.*}}|      3: <4, 0>              |        %c9 = i16 0;
; CHECK-NEXT:    {{.*}}|      3: <1, 10>             |      <8 x i16>:
; CHECK-NEXT:    {{.*}}|      3: <3>                 |        %c10 = <8 x i16> undef;
; CHECK-NEXT:    {{.*}}|      3: <1, 6>              |      <4 x i1>:
; CHECK-NEXT:    {{.*}}|      3: <3>                 |        %c11 = <4 x i1> undef;
; CHECK-NEXT:    {{.*}}|      3: <1, 7>              |      <8 x i1>:
; CHECK-NEXT:    {{.*}}|      3: <3>                 |        %c12 = <8 x i1> undef;
; CHECK-NEXT:    {{.*}}|      3: <1, 8>              |      <16 x i1>:
; CHECK-NEXT:    {{.*}}|      3: <3>                 |        %c13 = <16 x i1> undef;
; CHECK-NEXT:    {{.*}}|      3: <1, 9>              |      <16 x i8>:
; CHECK-NEXT:    {{.*}}|      3: <3>                 |        %c14 = <16 x i8> undef;
; CHECK-NEXT:    {{.*}}|      3: <1, 5>              |      <4 x float>:
; CHECK-NEXT:    {{.*}}|      3: <3>                 |        %c15 = <4 x float> undef;
; CHECK-NEXT:    {{.*}}|      3: <1, 2>              |      float:
; CHECK-NEXT:    {{.*}}|      3: <6, 1065353216>     |        %c16 = float 1;
; CHECK-NEXT:    {{.*}}|    0: <65534>               |      }
; CHECK-NEXT:          |                             |  %b0:

  %v0 = insertelement <4 x i1> undef, i1 1, i32 0
  %v1 = insertelement <4 x i1> %v0, i1 0, i32 1
  %v2 = extractelement <4 x i1> %v1, i32 0
  %v3 = and i1 %v2, 1

; CHECK-NEXT:    {{.*}}|    3: <7, 6, 14, 16>        |    %v0  =  
; CHECK-NEXT:          |                             |        insertelement <4 x i1> %c11, 
; CHECK-NEXT:          |                             |        i1 %c3, i32 %c1;
; CHECK-NEXT:    {{.*}}|    3: <7, 1, 14, 18>        |    %v1  =  insertelement <4 x i1> %v0,
; CHECK-NEXT:          |                             |        i1 %c4, i32 %c0;
; CHECK-NEXT:    {{.*}}|    3: <6, 1, 18>            |    %v2  =  
; CHECK-NEXT:          |                             |        extractelement <4 x i1> %v1, 
; CHECK-NEXT:          |                             |        i32 %c1;
; CHECK-NEXT:    {{.*}}|    3: <2, 1, 17, 10>        |    %v3 = and i1 %v2, %c3;

  %v4 = insertelement <8 x i1> undef, i1 1, i32 1
  %v5 = insertelement <8 x i1> %v4, i1 0, i32 0
  %v6 = extractelement <8 x i1> %v5, i32 1
  %v7 = and i1 %v6, 1

; CHECK-NEXT:    {{.*}}|    3: <7, 9, 18, 21>        |    %v4  =  
; CHECK-NEXT:          |                             |        insertelement <8 x i1> %c12, 
; CHECK-NEXT:          |                             |        i1 %c3, i32 %c0;
; CHECK-NEXT:    {{.*}}|    3: <7, 1, 18, 21>        |    %v5  =  insertelement <8 x i1> %v4,
; CHECK-NEXT:          |                             |        i1 %c4, i32 %c1;
; CHECK-NEXT:    {{.*}}|    3: <6, 1, 23>            |    %v6  =  
; CHECK-NEXT:          |                             |        extractelement <8 x i1> %v5, 
; CHECK-NEXT:          |                             |        i32 %c0;
; CHECK-NEXT:    {{.*}}|    3: <2, 1, 21, 10>        |    %v7 = and i1 %v6, %c3;

  %v8 = insertelement <16 x i1> undef, i1 1, i32 0
  %v9 = insertelement <16 x i1> %v8, i1 0, i32 1
  %v10 = extractelement <16 x i1> %v9, i32 1
  %v11 = and i1 %v10, 1

; CHECK-NEXT:    {{.*}}|    3: <7, 12, 22, 24>       |    %v8  =  
; CHECK-NEXT:          |                             |        insertelement <16 x i1> %c13, 
; CHECK-NEXT:          |                             |        i1 %c3, i32 %c1;
; CHECK-NEXT:    {{.*}}|    3: <7, 1, 22, 26>        |    %v9  =  
; CHECK-NEXT:          |                             |        insertelement <16 x i1> %v8, 
; CHECK-NEXT:          |                             |        i1 %c4, i32 %c0;
; CHECK-NEXT:    {{.*}}|    3: <6, 1, 27>            |    %v10  =  
; CHECK-NEXT:          |                             |        extractelement <16 x i1> %v9, 
; CHECK-NEXT:          |                             |        i32 %c0;
; CHECK-NEXT:    {{.*}}|    3: <2, 1, 25, 10>        |    %v11 = and i1 %v10, %c3;
  
  %v12 = insertelement <16 x i8> undef, i8 0, i32 1
  %v13 = insertelement <16 x i8> %v12, i8 10, i32 10
  %v14 = extractelement <16 x i8> %v13, i32 1
  %v15 = add i8 %v14, 0

; CHECK-NEXT:    {{.*}}|    3: <7, 15, 23, 29>       |    %v12  =  
; CHECK-NEXT:          |                             |        insertelement <16 x i8> %c14, 
; CHECK-NEXT:          |                             |        i8 %c6, i32 %c0;
; CHECK-NEXT:    {{.*}}|    3: <7, 1, 23, 28>        |    %v13  =  
; CHECK-NEXT:          |                             |        insertelement <16 x i8> %v12, 
; CHECK-NEXT:          |                             |        i8 %c7, i32 %c2;
; CHECK-NEXT:    {{.*}}|    3: <6, 1, 31>            |    %v14  =  
; CHECK-NEXT:          |                             |        extractelement <16 x i8> %v13, 
; CHECK-NEXT:          |                             |        i32 %c0;
; CHECK-NEXT:    {{.*}}|    3: <2, 1, 26, 0>         |    %v15 = add i8 %v14, %c6;

  %v16 = insertelement <8 x i16> undef, i16 1, i32 1
  %v17 = insertelement <8 x i16> %v16, i16 0, i32 0
  %v18 = extractelement <8 x i16> %v17, i32 1
  %v19 = add i16 %v18, 1

; CHECK-NEXT:    {{.*}}|    3: <7, 23, 25, 33>       |    %v16  =  
; CHECK-NEXT:          |                             |        insertelement <8 x i16> %c10, 
; CHECK-NEXT:          |                             |        i16 %c8, i32 %c0;
; CHECK-NEXT:    {{.*}}|    3: <7, 1, 25, 33>        |    %v17  =  
; CHECK-NEXT:          |                             |        insertelement <8 x i16> %v16, 
; CHECK-NEXT:          |                             |        i16 %c9, i32 %c1;
; CHECK-NEXT:    {{.*}}|    3: <6, 1, 35>            |    %v18  =  
; CHECK-NEXT:          |                             |        extractelement <8 x i16> %v17, 
; CHECK-NEXT:          |                             |        i32 %c0;
; CHECK-NEXT:    {{.*}}|    3: <2, 1, 28, 0>         |    %v19 = add i16 %v18, %c8;

  %v20 = insertelement <4 x i32> undef, i32 1, i32 0
  %v21 = insertelement <4 x i32> %v20, i32 0, i32 1
  %v22 = extractelement <4 x i32> %v21, i32 0
  %v23 = add i32 %v22, 1

; CHECK-NEXT:    {{.*}}|    3: <7, 32, 37, 36>       |    %v20  =  
; CHECK-NEXT:          |                             |        insertelement <4 x i32> %c5, 
; CHECK-NEXT:          |                             |        i32 %c0, i32 %c1;
; CHECK-NEXT:    {{.*}}|    3: <7, 1, 37, 38>        |    %v21  =  
; CHECK-NEXT:          |                             |        insertelement <4 x i32> %v20, 
; CHECK-NEXT:          |                             |        i32 %c1, i32 %c0;
; CHECK-NEXT:    {{.*}}|    3: <6, 1, 38>            |    %v22  =  
; CHECK-NEXT:          |                             |        extractelement <4 x i32> %v21, 
; CHECK-NEXT:          |                             |        i32 %c1;
; CHECK-NEXT:    {{.*}}|    3: <2, 1, 40, 0>         |    %v23 = add i32 %v22, %c0;

  %v24 = insertelement <4 x float> undef, float 1.0, i32 0
  %v25 = insertelement <4 x float> %v24, float 1.0, i32 1
  %v26 = extractelement <4 x float> %v25, i32 0
  %v27 = fadd float %v26, 1.0

; CHECK-NEXT:    {{.*}}|    3: <7, 26, 25, 40>       |    %v24  =  
; CHECK-NEXT:          |                             |        insertelement <4 x float> %c15,
; CHECK-NEXT:          |                             |        float %c16, i32 %c1;
; CHECK-NEXT:    {{.*}}|    3: <7, 1, 26, 42>        |    %v25  =  
; CHECK-NEXT:          |                             |        insertelement <4 x float> %v24,
; CHECK-NEXT:          |                             |        float %c16, i32 %c0;
; CHECK-NEXT:    {{.*}}|    3: <6, 1, 42>            |    %v26  =  
; CHECK-NEXT:          |                             |        extractelement <4 x float> %v25
; CHECK-NEXT:          |                             |        , i32 %c1;
; CHECK-NEXT:    {{.*}}|    3: <2, 1, 28, 0>         |    %v27 = fadd float %v26, %c16;

  ret void
; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;

}

define void @BadVectorOps(i32 %p0) {
; CHECK:               |                             |  %b0:

  %v0 = insertelement <4 x float> undef, float 1.0, i32 %p0

; CHECK-NEXT:    {{.*}}|    3: <7, 1, 2, 3>          |    %v0  =  
; CHECK-NEXT:          |                             |        insertelement <4 x float> %c1, 
; CHECK-NEXT:          |                             |        float %c0, i32 %p0;
; CHECK-NEXT:Error({{.*}}): insertelement: Vector index not constant: %p0

  ret void
; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;
}

