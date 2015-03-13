; Test the load instruction.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-bccompress --remove-abbreviations \
; RUN:              | not pnacl-bcdis | FileCheck %s

; Test valid loads.
define void @GoodTests(i32 %p0) {
; CHECK:               |                             |  %b0:

  %ai8 = inttoptr i32 %p0 to i8*
  %v0 = load i8* %ai8, align 1

; CHECK-NEXT:    {{.*}}|    3: <20, 1, 1, 3>         |    %v0 = load i8* %p0, align 1;

  %ai16 = inttoptr i32 %p0 to i16*
  %v1 = load i16* %ai16, align 1

; CHECK-NEXT:    {{.*}}|    3: <20, 2, 1, 4>         |    %v1 = load i16* %p0, align 1;

  %ai32 = inttoptr i32 %p0 to i32*
  %v2 = load i32* %ai32, align 1

; CHECK-NEXT:    {{.*}}|    3: <20, 3, 1, 0>         |    %v2 = load i32* %p0, align 1;

  %ai64 = inttoptr i32 %p0 to i64*
  %v3 = load i64* %ai64, align 1

; CHECK-NEXT:    {{.*}}|    3: <20, 4, 1, 5>         |    %v3 = load i64* %p0, align 1;

  %af = inttoptr i32 %p0 to float*
  %v4 = load float* %af, align 1
  %v5 = load float* %af, align 4

; CHECK-NEXT:    {{.*}}|    3: <20, 5, 1, 1>         |    %v4 = load float* %p0, align 1;
; CHECK-NEXT:    {{.*}}|    3: <20, 6, 3, 1>         |    %v5 = load float* %p0, align 4;

  %ad = inttoptr i32 %p0 to double*
  %v6 = load double* %ad, align 1
  %v7 = load double* %ad, align 8

; CHECK-NEXT:    {{.*}}|    3: <20, 7, 1, 2>         |    %v6 = load double* %p0, align 1;
; CHECK-NEXT:    {{.*}}|    3: <20, 8, 4, 2>         |    %v7 = load double* %p0, align 8;

  %av16_i8 = inttoptr i32 %p0 to <16 x i8>*
  %v8 = load <16 x i8>* %av16_i8, align 1

; CHECK-NEXT:    {{.*}}|    3: <20, 9, 1, 6>         |    %v8 = load <16 x i8>* %p0, align 1;

  %av8_i16 = inttoptr i32 %p0 to <8 x i16>*
  %v9 = load <8 x i16>* %av8_i16, align 2

; CHECK-NEXT:    {{.*}}|    3: <20, 10, 2, 7>        |    %v9 = load <8 x i16>* %p0, align 2;

  %av4_i32 = inttoptr i32 %p0 to <4 x i32>*
  %v10 = load <4 x i32>* %av4_i32, align 4

; CHECK-NEXT:    {{.*}}|    3: <20, 11, 3, 8>        |    %v10 = load <4 x i32>* %p0, 
; CHECK-NEXT:          |                             |        align 4;

  %av4_f = inttoptr i32 %p0 to <4 x float>*
  %v11 = load <4 x float>* %av4_f, align 4

; CHECK-NEXT:    {{.*}}|    3: <20, 12, 3, 9>        |    %v11 = load <4 x float>* %p0, 
; CHECK-NEXT:          |                             |        align 4;

  ; Verify correct type loaded.
  %v12 = add i8 %v0, %v0
  %v13 = add i16 %v1, %v1
  %v14 = add i32 %v2, %v2
  %v15 = add i64 %v3, %v3
  %v16 = fadd float %v4, %v5
  %v17 = fadd double %v6, %v7
  %v18 = add <16 x i8> %v8, %v8
  %v19 = add <8 x i16> %v9, %v9
  %v20 = add <4 x i32> %v10, %v10
  %v21 = fadd <4 x float> %v11, %v11

; CHECK-NEXT:    {{.*}}|    3: <2, 12, 12, 0>        |    %v12 = add i8 %v0, %v0;
; CHECK-NEXT:    {{.*}}|    3: <2, 12, 12, 0>        |    %v13 = add i16 %v1, %v1;
; CHECK-NEXT:    {{.*}}|    3: <2, 12, 12, 0>        |    %v14 = add i32 %v2, %v2;
; CHECK-NEXT:    {{.*}}|    3: <2, 12, 12, 0>        |    %v15 = add i64 %v3, %v3;
; CHECK-NEXT:    {{.*}}|    3: <2, 12, 11, 0>        |    %v16 = fadd float %v4, %v5;
; CHECK-NEXT:    {{.*}}|    3: <2, 11, 10, 0>        |    %v17 = fadd double %v6, %v7;
; CHECK-NEXT:    {{.*}}|    3: <2, 10, 10, 0>        |    %v18 = add <16 x i8> %v8, %v8;
; CHECK-NEXT:    {{.*}}|    3: <2, 10, 10, 0>        |    %v19 = add <8 x i16> %v9, %v9;
; CHECK-NEXT:    {{.*}}|    3: <2, 10, 10, 0>        |    %v20 = add <4 x i32> %v10, %v10;
; CHECK-NEXT:    {{.*}}|    3: <2, 10, 10, 0>        |    %v21 = fadd <4 x float> %v11, %v11;

  ret void
; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;
}



; Test invalid loads.
define void @BadTests(i32 %p0) {
; CHECK:               |                             |  %b0:

  %ai1 = inttoptr i32 %p0 to i1*
  %v0 = load i1* %ai1

; CHECK-NEXT:    {{.*}}|    3: <20, 1, 0, 10>        |    %v0 = load i1* %p0, align 0;
; CHECK-NEXT:Error({{.*}}): load: Illegal alignment for i1. Expects: 1

  %ai8 = inttoptr i32 %p0 to i8*
  %v1 = load i8* %ai8
; CHECK-NEXT:    {{.*}}|    3: <20, 2, 0, 3>         |    %v1 = load i8* %p0, align 0;
; CHECK-NEXT:Error({{.*}}): load: Illegal alignment for i8. Expects: 1

  %ai16 = inttoptr i32 %p0 to i16*
  %v2 = load i16* %ai16

; CHECK-NEXT:    {{.*}}|    3: <20, 3, 0, 4>         |    %v2 = load i16* %p0, align 0;
; CHECK-NEXT:Error({{.*}}): load: Illegal alignment for i16. Expects: 1

  %ai32 = inttoptr i32 %p0 to i32*
  %v3 = load i32* %ai32

; CHECK-NEXT:    {{.*}}|    3: <20, 4, 0, 0>         |    %v3 = load i32* %p0, align 0;
; CHECK-NEXT:Error({{.*}}): load: Illegal alignment for i32. Expects: 1

  %ai64 = inttoptr i32 %p0 to i64*
  %v4 = load i64* %ai64

; CHECK-NEXT:    {{.*}}|    3: <20, 5, 0, 5>         |    %v4 = load i64* %p0, align 0;
; CHECK-NEXT:Error({{.*}}): load: Illegal alignment for i64. Expects: 1

  %af = inttoptr i32 %p0 to float*
  %v5 = load float* %af

; CHECK-NEXT:    {{.*}}|    3: <20, 6, 0, 1>         |    %v5 = load float* %p0, align 0;
; CHECK-NEXT:Error({{.*}}): load: Illegal alignment for float. Expects: 1 or 4

  %ad = inttoptr i32 %p0 to double*
  %v6 = load double* %ad, align 4

; CHECK-NEXT:    {{.*}}|    3: <20, 7, 3, 2>         |    %v6 = load double* %p0, align 4;
; CHECK-NEXT:Error({{.*}}): load: Illegal alignment for double. Expects: 1 or 8

  %av16_i8 = inttoptr i32 %p0 to <16 x i8>*
  %v7 = load <16 x i8>* %av16_i8, align 8

; CHECK-NEXT:    {{.*}}|    3: <20, 8, 4, 6>         |    %v7 = load <16 x i8>* %p0, align 8;
; CHECK-NEXT:Error({{.*}}): load: Illegal alignment for <16 x i8>. Expects: 1

  %av8_i16 = inttoptr i32 %p0 to <8 x i16>*
  %v8 = load <8 x i16>* %av8_i16, align 1

; CHECK-NEXT:    {{.*}}|    3: <20, 9, 1, 7>         |    %v8 = load <8 x i16>* %p0, align 1;
; CHECK-NEXT:Error({{.*}}): load: Illegal alignment for <8 x i16>. Expects: 2

  %av4_i32 = inttoptr i32 %p0 to <4 x i32>*
  %v9 = load <4 x i32>* %av4_i32, align 1

; CHECK-NEXT:    {{.*}}|    3: <20, 10, 1, 8>        |    %v9 = load <4 x i32>* %p0, align 1;
; CHECK-NEXT:Error({{.*}}): load: Illegal alignment for <4 x i32>. Expects: 4

  %av4_f = inttoptr i32 %p0 to <4 x float>*
  %v10 = load <4 x float>* %av4_f, align 1

; CHECK-NEXT:    {{.*}}|    3: <20, 11, 1, 9>        |    %v10 = load <4 x float>* %p0, 
; CHECK-NEXT:          |                             |        align 1;
; CHECK-NEXT:Error({{.*}}): load: Illegal alignment for <4 x float>. Expects: 4

  ; Note: vectors of form <N x i1> can't be loaded because no alignment
  ; value is valid.

  %av4_i1 = inttoptr i32 %p0 to <4 x i1>*
  %v11 = load <4 x i1>* %av4_i1, align 1

; CHECK-NEXT:    {{.*}}|    3: <20, 12, 1, 13>       |    %v11 = load <4 x i1>* %p0, align 1;
; CHECK-NEXT:Error({{.*}}): load: Not allowed for type: <4 x i1>

  %av8_i1 = inttoptr i32 %p0 to <8 x i1>*
  %v12 = load <8 x i1>* %av8_i1, align 1

; CHECK-NEXT:    {{.*}}|    3: <20, 13, 1, 14>       |    %v12 = load <8 x i1>* %p0, align 1;
; CHECK-NEXT:Error({{.*}}): load: Not allowed for type: <8 x i1>

  %av16_i1 = inttoptr i32 %p0 to <16 x i1>*
  %v13 = load <16 x i1>* %av16_i1, align 1

; CHECK-NEXT:    {{.*}}|    3: <20, 14, 1, 15>       |    %v13 = load <16 x i1>* %p0, 
; CHECK-NEXT:          |                             |        align 1;
; CHECK-NEXT:Error({{.*}}): load: Not allowed for type: <16 x i1>

  ret void
; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;

}

