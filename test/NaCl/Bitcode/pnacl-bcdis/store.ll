; Test the store instruction.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-bccompress --remove-abbreviations \
; RUN:              | not pnacl-bcdis | FileCheck %s

; Test valid stores.
define void @GoodTests(i32 %p0) {

; CHECK:         {{.*}}|    1: <65535, 11, 2>        |    constants {  // BlockID = 11
; CHECK-NEXT:    {{.*}}|      3: <1, 1>              |      i32:
; CHECK-NEXT:    {{.*}}|      3: <4, 6>              |        %c0 = i32 3;
; CHECK-NEXT:    {{.*}}|      3: <1, 10>             |      <4 x i32>:
; CHECK-NEXT:    {{.*}}|      3: <3>                 |        %c1 = <4 x i32> undef;
; CHECK-NEXT:    {{.*}}|      3: <1, 9>              |      <8 x i16>:
; CHECK-NEXT:    {{.*}}|      3: <3>                 |        %c2 = <8 x i16> undef;
; CHECK-NEXT:    {{.*}}|      3: <1, 8>              |      <16 x i8>:
; CHECK-NEXT:    {{.*}}|      3: <3>                 |        %c3 = <16 x i8> undef;
; CHECK-NEXT:    {{.*}}|      3: <1, 7>              |      i64:
; CHECK-NEXT:    {{.*}}|      3: <4, 8>              |        %c4 = i64 4;
; CHECK-NEXT:    {{.*}}|      3: <1, 4>              |      i8:
; CHECK-NEXT:    {{.*}}|      3: <4, 4>              |        %c5 = i8 2;
; CHECK-NEXT:    {{.*}}|      3: <1, 5>              |      i16:
; CHECK-NEXT:    {{.*}}|      3: <4, 4>              |        %c6 = i16 2;
; CHECK-NEXT:    {{.*}}|      3: <1, 3>              |      double:
; CHECK-NEXT:    {{.*}}|      3: <6,                 |        %c7 = double 2;
; CHECK-NEXT:          |        4611686018427387904> |
; CHECK-NEXT:    {{.*}}|      3: <6,                 |        %c8 = double 1;
; CHECK-NEXT:          |        4607182418800017408> |
; CHECK-NEXT:    {{.*}}|      3: <1, 2>              |      float:
; CHECK-NEXT:    {{.*}}|      3: <6, 1073741824>     |        %c9 = float 2;
; CHECK-NEXT:    {{.*}}|      3: <6, 1065353216>     |        %c10 = float 1;
; CHECK-NEXT:    {{.*}}|      3: <1, 11>             |      <4 x float>:
; CHECK-NEXT:    {{.*}}|      3: <3>                 |        %c11 = <4 x float> undef;
; CHECK-NEXT:    {{.*}}|    0: <65534>               |      }
; CHECK-NEXT:          |                             |  %b0:

  %ai8 = inttoptr i32 %p0 to i8*
  store i8 2, i8* %ai8, align 1

; CHECK-NEXT:    {{.*}}|    3: <24, 13, 7, 1>        |    store i8 %c5, i8* %p0, align 1;

  %ai16 = inttoptr i32 %p0 to i16*
  store i16 2, i16* %ai16, align 1

; CHECK-NEXT:    {{.*}}|    3: <24, 13, 6, 1>        |    store i16 %c6, i16* %p0, align 1;

  %ai32 = inttoptr i32 %p0 to i32*
  store i32 3, i32* %ai32, align 1

; CHECK-NEXT:    {{.*}}|    3: <24, 13, 12, 1>       |    store i32 %c0, i32* %p0, align 1;

  %ai64 = inttoptr i32 %p0 to i64*
  store i64 4, i64* %ai64, align 1

; CHECK-NEXT:    {{.*}}|    3: <24, 13, 8, 1>        |    store i64 %c4, i64* %p0, align 1;

  %af = inttoptr i32 %p0 to float*
  store float 1.0, float* %af, align 1
  store float 2.0, float* %af, align 4

; CHECK-NEXT:    {{.*}}|    3: <24, 13, 2, 1>        |    store float %c10, float* %p0, 
; CHECK-NEXT:          |                             |        align 1;
; CHECK-NEXT:    {{.*}}|    3: <24, 13, 3, 3>        |    store float %c9, float* %p0, 
; CHECK-NEXT:          |                             |        align 4;

  %ad = inttoptr i32 %p0 to double*
  store double 1.0, double* %ad, align 1
  store double 2.0, double* %ad, align 8

; CHECK-NEXT:    {{.*}}|    3: <24, 13, 4, 1>        |    store double %c8, double* %p0, 
; CHECK-NEXT:          |                             |        align 1;
; CHECK-NEXT:    {{.*}}|    3: <24, 13, 5, 4>        |    store double %c7, double* %p0, 
; CHECK-NEXT:          |                             |        align 8;

  %av16_i8 = inttoptr i32 %p0 to <16 x i8>*
  store <16 x i8> undef, <16 x i8>* %av16_i8, align 1

; CHECK-NEXT:    {{.*}}|    3: <24, 13, 9, 1>        |    store <16 x i8> %c3, 
; CHECK-NEXT:          |                             |        <16 x i8>* %p0, align 1;

  %av8_i16 = inttoptr i32 %p0 to <8 x i16>*
  store <8 x i16> undef, <8 x i16>* %av8_i16, align 2

; CHECK-NEXT:    {{.*}}|    3: <24, 13, 10, 2>       |    store <8 x i16> %c2, 
; CHECK-NEXT:          |                             |        <8 x i16>* %p0, align 2;

  %av4_i32 = inttoptr i32 %p0 to <4 x i32>*
  store <4 x i32> undef,  <4 x i32>* %av4_i32, align 4

; CHECK-NEXT:    {{.*}}|    3: <24, 13, 11, 3>       |    store <4 x i32> %c1, 
; CHECK-NEXT:          |                             |        <4 x i32>* %p0, align 4;

  %av4_f = inttoptr i32 %p0 to <4 x float>*
  store <4 x float> undef,  <4 x float>* %av4_f, align 4
; CHECK-NEXT:    {{.*}}|    3: <24, 13, 1, 3>        |    store <4 x float> %c11, 
; CHECK-NEXT:          |                             |        <4 x float>* %p0, align 4;

  ret void

; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;

}


; Test invalid stores.
define void @BadTests(i32 %p0) {

; CHECK:         {{.*}}|    1: <65535, 11, 2>        |    constants {  // BlockID = 11
; CHECK-NEXT:    {{.*}}|      3: <1, 1>              |      i32:
; CHECK-NEXT:    {{.*}}|      3: <4, 6>              |        %c0 = i32 3;
; CHECK-NEXT:    {{.*}}|      3: <1, 15>             |      <16 x i1>:
; CHECK-NEXT:    {{.*}}|      3: <3>                 |        %c1 = <16 x i1> undef;
; CHECK-NEXT:    {{.*}}|      3: <1, 14>             |      <8 x i1>:
; CHECK-NEXT:    {{.*}}|      3: <3>                 |        %c2 = <8 x i1> undef;
; CHECK-NEXT:    {{.*}}|      3: <1, 4>              |      i8:
; CHECK-NEXT:    {{.*}}|      3: <4, 2>              |        %c3 = i8 1;
; CHECK-NEXT:    {{.*}}|      3: <1, 5>              |      i16:
; CHECK-NEXT:    {{.*}}|      3: <4, 4>              |        %c4 = i16 2;
; CHECK-NEXT:    {{.*}}|      3: <1, 6>              |      i1:
; CHECK-NEXT:    {{.*}}|      3: <4, 0>              |        %c5 = i1 0;
; CHECK-NEXT:    {{.*}}|      3: <1, 7>              |      i64:
; CHECK-NEXT:    {{.*}}|      3: <4, 8>              |        %c6 = i64 4;
; CHECK-NEXT:    {{.*}}|      3: <1, 8>              |      <16 x i8>:
; CHECK-NEXT:    {{.*}}|      3: <3>                 |        %c7 = <16 x i8> undef;
; CHECK-NEXT:    {{.*}}|      3: <1, 9>              |      <8 x i16>:
; CHECK-NEXT:    {{.*}}|      3: <3>                 |        %c8 = <8 x i16> undef;
; CHECK-NEXT:    {{.*}}|      3: <1, 10>             |      <4 x i32>:
; CHECK-NEXT:    {{.*}}|      3: <3>                 |        %c9 = <4 x i32> undef;
; CHECK-NEXT:    {{.*}}|      3: <1, 13>             |      <4 x i1>:
; CHECK-NEXT:    {{.*}}|      3: <3>                 |        %c10 = <4 x i1> undef;
; CHECK-NEXT:    {{.*}}|      3: <1, 11>             |      <4 x float>:
; CHECK-NEXT:    {{.*}}|      3: <3>                 |        %c11 = <4 x float> undef;
; CHECK-NEXT:    {{.*}}|      3: <1, 3>              |      double:
; CHECK-NEXT:    {{.*}}|      3: <6,                 |        %c12 = double 2;
; CHECK-NEXT:          |        4611686018427387904> |
; CHECK-NEXT:    {{.*}}|      3: <1, 2>              |      float:
; CHECK-NEXT:    {{.*}}|      3: <6, 1065353216>     |        %c13 = float 1;
; CHECK-NEXT:    {{.*}}|    0: <65534>               |      }
; CHECK-NEXT:          |                             |  %b0:

  %ai1 = inttoptr i32 %p0 to i1*
  store i1 0, i1* %ai1

; CHECK-NEXT:    {{.*}}|    3: <24, 15, 9, 0>        |    store i1 %c5, i1* %p0, align 0;
; CHECK-NEXT:Error({{.*}}): store: Illegal alignment for i1. Expects: 1

  %ai8 = inttoptr i32 %p0 to i8*
  store i8 1, i8* %ai8

; CHECK-NEXT:    {{.*}}|    3: <24, 15, 11, 0>        |    store i8 %c3, i8* %p0, align 0;
; CHECK-NEXT:Error({{.*}}): store: Illegal alignment for i8. Expects: 1

  %ai16 = inttoptr i32 %p0 to i16*
  store i16 2, i16* %ai16

; CHECK-NEXT:    {{.*}}|    3: <24, 15, 10, 0>       |    store i16 %c4, i16* %p0, align 0;
; CHECK-NEXT:Error({{.*}}): store: Illegal alignment for i16. Expects: 1

  %ai32 = inttoptr i32 %p0 to i32*
  store i32 3, i32* %ai32

; CHECK-NEXT:    {{.*}}|    3: <24, 15, 14, 0>      |    store i32 %c0, i32* %p0, align 0;
; CHECK-NEXT:Error({{.*}}): store: Illegal alignment for i32. Expects: 1

  %ai64 = inttoptr i32 %p0 to i64*
  store i64 4, i64* %ai64

; CHECK-NEXT:    {{.*}}|    3: <24, 15, 8, 0>        |    store i64 %c6, i64* %p0, align 0;
; CHECK-NEXT:Error({{.*}}): store: Illegal alignment for i64. Expects: 1

  %af = inttoptr i32 %p0 to float*
  store float 1.0, float* %af

; CHECK-NEXT:    {{.*}}|    3: <24, 15, 1, 0>        |    store float %c13, float* %p0, 
; CHECK-NEXT:          |                             |        align 0;
; CHECK-NEXT:Error({{.*}}): store: Illegal alignment for float. Expects: 1 or 4

  %ad = inttoptr i32 %p0 to double*
  store double 2.0, double* %ad, align 4

; CHECK-NEXT:    {{.*}}|    3: <24, 15, 2, 3>        |    store double %c12, double* %p0, 
; CHECK-NEXT:          |                             |        align 4;
; CHECK-NEXT:Error({{.*}}): store: Illegal alignment for double. Expects: 1 or 8

  %av16_i8 = inttoptr i32 %p0 to <16 x i8>*
  store <16 x i8> undef, <16 x i8>* %av16_i8, align 4

; CHECK-NEXT:    {{.*}}|    3: <24, 15, 7, 3>        |    store <16 x i8> %c7, 
; CHECK-NEXT:          |                             |        <16 x i8>* %p0, align 4;
; CHECK-NEXT:Error({{.*}}): store: Illegal alignment for <16 x i8>. Expects: 1

  %av8_i16 = inttoptr i32 %p0 to <8 x i16>*
  store <8 x i16> undef, <8 x i16>* %av8_i16, align 1

; CHECK-NEXT:    {{.*}}|    3: <24, 15, 6, 1>        |    store <8 x i16> %c8, 
; CHECK-NEXT:          |                             |        <8 x i16>* %p0, align 1;
; CHECK-NEXT:Error({{.*}}): store: Illegal alignment for <8 x i16>. Expects: 2

  %av4_i32 = inttoptr i32 %p0 to <4 x i32>*
  store <4 x i32> undef,  <4 x i32>* %av4_i32, align 1

; CHECK-NEXT:    {{.*}}|    3: <24, 15, 5, 1>        |    store <4 x i32> %c9, 
; CHECK-NEXT:          |                             |        <4 x i32>* %p0, align 1;
; CHECK-NEXT:Error({{.*}}): store: Illegal alignment for <4 x i32>. Expects: 4

  %av4_f = inttoptr i32 %p0 to <4 x float>*
  store <4 x float> undef,  <4 x float>* %av4_f, align 1

; CHECK-NEXT:    {{.*}}|    3: <24, 15, 3, 1>        |    store <4 x float> %c11, 
; CHECK-NEXT:          |                             |        <4 x float>* %p0, align 1;
; CHECK-NEXT:Error({{.*}}): store: Illegal alignment for <4 x float>. Expects: 4

  ; Note: vectors of form <N x i1> can't be stored because no alignment
  ; value is valid.

  %av4_i1 = inttoptr i32 %p0 to <4 x i1>*
  store <4 x i1> undef,  <4 x i1>* %av4_i1, align 1

; CHECK-NEXT:    {{.*}}|    3: <24, 15, 4, 1>        |    store <4 x i1> %c10, <4 x i1>* %p0,
; CHECK-NEXT:          |                             |        align 1;
; CHECK-NEXT:Error({{.*}}): store: Not allowed for type: <4 x i1>

  %av8_i1 = inttoptr i32 %p0 to <8 x i1>*
  store <8 x i1> undef,  <8 x i1>* %av8_i1, align 1

; CHECK-NEXT:    {{.*}}|    3: <24, 15, 12, 1>       |    store <8 x i1> %c2, <8 x i1>* %p0, 
; CHECK-NEXT:          |                             |        align 1;
; CHECK-NEXT:Error({{.*}}): store: Not allowed for type: <8 x i1>

  %av16_i1 = inttoptr i32 %p0 to <16 x i1>*
  store <16 x i1> undef,  <16 x i1>* %av16_i1, align 1

; CHECK-NEXT:    {{.*}}|    3: <24, 15, 13, 1>       |    store <16 x i1> %c1, 
; CHECK-NEXT:          |                             |        <16 x i1>* %p0, align 1;
; CHECK-NEXT:Error({{.*}}): store: Not allowed for type: <16 x i1>

  ret void

; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;

}

