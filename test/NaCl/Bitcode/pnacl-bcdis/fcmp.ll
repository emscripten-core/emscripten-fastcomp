; Tests the fcmp instruction.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-bccompress --remove-abbreviations \
; RUN:              | pnacl-bcdis | FileCheck %s


; Test fcmp on primitive types.
define void @SimpleFcmpOps(float %p0, double %p1) {

; CHECK:               |                             |  %b0:

  %v0 = fcmp false float %p0, 2.0
  %v1 = fcmp false double %p1, 3.0

; CHECK-NEXT:    {{.*}}|    3: <28, 4, 2, 0>         |    %v0 = fcmp false float %p0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <28, 4, 2, 0>         |    %v1 = fcmp false double %p1, %c1;

  %v2 = fcmp oeq float %p0, 2.0
  %v3 = fcmp oeq double %p1, 3.0

; CHECK-NEXT:    {{.*}}|    3: <28, 6, 4, 1>         |    %v2 = fcmp oeq float %p0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <28, 6, 4, 1>         |    %v3 = fcmp oeq double %p1, %c1;

  %v4 = fcmp ogt float %p0, 2.0
  %v5 = fcmp ogt double %p1, 3.0

; CHECK-NEXT:    {{.*}}|    3: <28, 8, 6, 2>         |    %v4 = fcmp ogt float %p0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <28, 8, 6, 2>         |    %v5 = fcmp ogt double %p1, %c1;

  %v6 = fcmp oge float %p0, 2.0
  %v7 = fcmp oge double %p1, 3.0

; CHECK-NEXT:    {{.*}}|    3: <28, 10, 8, 3>        |    %v6 = fcmp oge float %p0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <28, 10, 8, 3>        |    %v7 = fcmp oge double %p1, %c1;

  %v8 = fcmp olt float %p0, 2.0
  %v9 = fcmp olt double %p1, 3.0

; CHECK-NEXT:    {{.*}}|    3: <28, 12, 10, 4>       |    %v8 = fcmp olt float %p0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <28, 12, 10, 4>       |    %v9 = fcmp olt double %p1, %c1;

  %v10 = fcmp ole float %p0, 2.0
  %v11 = fcmp ole double %p1, 3.0

; CHECK-NEXT:    {{.*}}|    3: <28, 14, 12, 5>       |    %v10 = fcmp ole float %p0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <28, 14, 12, 5>       |    %v11 = fcmp ole double %p1, %c1;

  %v12 = fcmp one float %p0, 2.0
  %v13 = fcmp one double %p1, 3.0

; CHECK-NEXT:    {{.*}}|    3: <28, 16, 14, 6>       |    %v12 = fcmp one float %p0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <28, 16, 14, 6>       |    %v13 = fcmp one double %p1, %c1;

  %v14 = fcmp ord float %p0, 2.0
  %v15 = fcmp ord double %p1, 3.0

; CHECK-NEXT:    {{.*}}|    3: <28, 18, 16, 7>       |    %v14 = fcmp ord float %p0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <28, 18, 16, 7>       |    %v15 = fcmp ord double %p1, %c1;

  %v16 = fcmp ueq float %p0, 2.0
  %v17 = fcmp ueq double %p1, 3.0

; CHECK-NEXT:    {{.*}}|    3: <28, 20, 18, 9>       |    %v16 = fcmp ueq float %p0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <28, 20, 18, 9>       |    %v17 = fcmp ueq double %p1, %c1;

  %v18 = fcmp ugt float %p0, 2.0
  %v19 = fcmp ugt double %p1, 3.0

; CHECK-NEXT:    {{.*}}|    3: <28, 22, 20, 10>      |    %v18 = fcmp ugt float %p0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <28, 22, 20, 10>      |    %v19 = fcmp ugt double %p1, %c1;

  %v20 = fcmp uge float %p0, 2.0
  %v21 = fcmp uge double %p1, 3.0

; CHECK-NEXT:    {{.*}}|    3: <28, 24, 22, 11>      |    %v20 = fcmp uge float %p0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <28, 24, 22, 11>      |    %v21 = fcmp uge double %p1, %c1;

  %v22 = fcmp ult float %p0, 2.0
  %v23 = fcmp ult double %p1, 3.0

; CHECK-NEXT:    {{.*}}|    3: <28, 26, 24, 12>      |    %v22 = fcmp ult float %p0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <28, 26, 24, 12>      |    %v23 = fcmp ult double %p1, %c1;

  %v24 = fcmp ule float %p0, 2.0
  %v25 = fcmp ule double %p1, 3.0

; CHECK-NEXT:    {{.*}}|    3: <28, 28, 26, 13>      |    %v24 = fcmp ule float %p0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <28, 28, 26, 13>      |    %v25 = fcmp ule double %p1, %c1;

  %v26 = fcmp une float %p0, 2.0
  %v27 = fcmp une double %p1, 3.0

; CHECK-NEXT:    {{.*}}|    3: <28, 30, 28, 14>      |    %v26 = fcmp une float %p0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <28, 30, 28, 14>      |    %v27 = fcmp une double %p1, %c1;

  %v28 = fcmp uno float %p0, 2.0
  %v29 = fcmp uno double %p1, 3.0

; CHECK-NEXT:    {{.*}}|    3: <28, 32, 30, 8>       |    %v28 = fcmp uno float %p0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <28, 32, 30, 8>       |    %v29 = fcmp uno double %p1, %c1;

  %v30 = fcmp true float %p0, 2.0
  %v31 = fcmp true double %p1, 3.0

; CHECK-NEXT:    {{.*}}|    3: <28, 34, 32, 15>      |    %v30 = fcmp true float %p0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <28, 34, 32, 15>      |    %v31 = fcmp true double %p1, %c1;

  ; Verifies result is i1.
  %v32 = and i1 %v0, %v16
  %v33 = and i1 %v1, %v17
  %v34 = and i1 %v2, %v18
  %v35 = and i1 %v3, %v19
  %v36 = and i1 %v4, %v20
  %v37 = and i1 %v5, %v21
  %v38 = and i1 %v6, %v22
  %v39 = and i1 %v7, %v23
  %v40 = and i1 %v8, %v24
  %v41 = and i1 %v9, %v25
  %v42 = and i1 %v10, %v26
  %v43 = and i1 %v11, %v27
  %v44 = and i1 %v12, %v28
  %v45 = and i1 %v13, %v29
  %v46 = and i1 %v14, %v30
  %v47 = and i1 %v15, %v31
  ret void

; CHECK-NEXT:    {{.*}}|    3: <2, 32, 16, 10>       |    %v32 = and i1 %v0, %v16;
; CHECK-NEXT:    {{.*}}|    3: <2, 32, 16, 10>       |    %v33 = and i1 %v1, %v17;
; CHECK-NEXT:    {{.*}}|    3: <2, 32, 16, 10>       |    %v34 = and i1 %v2, %v18;
; CHECK-NEXT:    {{.*}}|    3: <2, 32, 16, 10>       |    %v35 = and i1 %v3, %v19;
; CHECK-NEXT:    {{.*}}|    3: <2, 32, 16, 10>       |    %v36 = and i1 %v4, %v20;
; CHECK-NEXT:    {{.*}}|    3: <2, 32, 16, 10>       |    %v37 = and i1 %v5, %v21;
; CHECK-NEXT:    {{.*}}|    3: <2, 32, 16, 10>       |    %v38 = and i1 %v6, %v22;
; CHECK-NEXT:    {{.*}}|    3: <2, 32, 16, 10>       |    %v39 = and i1 %v7, %v23;
; CHECK-NEXT:    {{.*}}|    3: <2, 32, 16, 10>       |    %v40 = and i1 %v8, %v24;
; CHECK-NEXT:    {{.*}}|    3: <2, 32, 16, 10>       |    %v41 = and i1 %v9, %v25;
; CHECK-NEXT:    {{.*}}|    3: <2, 32, 16, 10>       |    %v42 = and i1 %v10, %v26;
; CHECK-NEXT:    {{.*}}|    3: <2, 32, 16, 10>       |    %v43 = and i1 %v11, %v27;
; CHECK-NEXT:    {{.*}}|    3: <2, 32, 16, 10>       |    %v44 = and i1 %v12, %v28;
; CHECK-NEXT:    {{.*}}|    3: <2, 32, 16, 10>       |    %v45 = and i1 %v13, %v29;
; CHECK-NEXT:    {{.*}}|    3: <2, 32, 16, 10>       |    %v46 = and i1 %v14, %v30;
; CHECK-NEXT:    {{.*}}|    3: <2, 32, 16, 10>       |    %v47 = and i1 %v15, %v31;
; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;

}



; Tests floating vector compares
define internal void @VecFloatOps(<4 x float> %p0) {

; CHECK:               |                             |  %b0:

  %v0 = fcmp false <4 x float> %p0, %p0
  %v1 = fcmp oeq <4 x float> %p0, %p0
  %v2 = fcmp ogt <4 x float> %p0, %p0
  %v3 = fcmp oge <4 x float> %p0, %p0
  %v4 = fcmp olt <4 x float> %p0, %p0
  %v5 = fcmp ole <4 x float> %p0, %p0
  %v6 = fcmp one <4 x float> %p0, %p0
  %v7 = fcmp ord <4 x float> %p0, %p0
  %v8 = fcmp ueq <4 x float> %p0, %p0
  %v9 = fcmp ugt <4 x float> %p0, %p0
  %v10 = fcmp uge <4 x float> %p0, %p0
  %v11 = fcmp ult <4 x float> %p0, %p0
  %v12 = fcmp ule <4 x float> %p0, %p0
  %v13 = fcmp une <4 x float> %p0, %p0
  %v14 = fcmp uno <4 x float> %p0, %p0
  %v15 = fcmp true <4 x float> %p0, %p0

; CHECK-NEXT:    {{.*}}|    3: <28, 1, 1, 0>         |    %v0 = fcmp false <4 x float> %p0, 
; CHECK-NEXT:          |                             |        %p0;
; CHECK-NEXT:    {{.*}}|    3: <28, 2, 2, 1>         |    %v1 = fcmp oeq <4 x float> %p0, 
; CHECK-NEXT:          |                             |        %p0;
; CHECK-NEXT:    {{.*}}|    3: <28, 3, 3, 2>         |    %v2 = fcmp ogt <4 x float> %p0, 
; CHECK-NEXT:          |                             |        %p0;
; CHECK-NEXT:    {{.*}}|    3: <28, 4, 4, 3>         |    %v3 = fcmp oge <4 x float> %p0, 
; CHECK-NEXT:          |                             |        %p0;
; CHECK-NEXT:    {{.*}}|    3: <28, 5, 5, 4>         |    %v4 = fcmp olt <4 x float> %p0, 
; CHECK-NEXT:          |                             |        %p0;
; CHECK-NEXT:    {{.*}}|    3: <28, 6, 6, 5>         |    %v5 = fcmp ole <4 x float> %p0, 
; CHECK-NEXT:          |                             |        %p0;
; CHECK-NEXT:    {{.*}}|    3: <28, 7, 7, 6>         |    %v6 = fcmp one <4 x float> %p0, 
; CHECK-NEXT:          |                             |        %p0;
; CHECK-NEXT:    {{.*}}|    3: <28, 8, 8, 7>         |    %v7 = fcmp ord <4 x float> %p0, 
; CHECK-NEXT:          |                             |        %p0;
; CHECK-NEXT:    {{.*}}|    3: <28, 9, 9, 9>         |    %v8 = fcmp ueq <4 x float> %p0, 
; CHECK-NEXT:          |                             |        %p0;
; CHECK-NEXT:    {{.*}}|    3: <28, 10, 10, 10>      |    %v9 = fcmp ugt <4 x float> %p0, 
; CHECK-NEXT:          |                             |        %p0;
; CHECK-NEXT:    {{.*}}|    3: <28, 11, 11, 11>      |    %v10 = fcmp uge <4 x float> %p0, 
; CHECK-NEXT:          |                             |        %p0;
; CHECK-NEXT:    {{.*}}|    3: <28, 12, 12, 12>      |    %v11 = fcmp ult <4 x float> %p0, 
; CHECK-NEXT:          |                             |        %p0;
; CHECK-NEXT:    {{.*}}|    3: <28, 13, 13, 13>      |    %v12 = fcmp ule <4 x float> %p0, 
; CHECK-NEXT:          |                             |        %p0;
; CHECK-NEXT:    {{.*}}|    3: <28, 14, 14, 14>      |    %v13 = fcmp une <4 x float> %p0, 
; CHECK-NEXT:          |                             |        %p0;
; CHECK-NEXT:    {{.*}}|    3: <28, 15, 15, 8>       |    %v14 = fcmp uno <4 x float> %p0, 
; CHECK-NEXT:          |                             |        %p0;
; CHECK-NEXT:    {{.*}}|    3: <28, 16, 16, 15>      |    %v15 = fcmp true <4 x float> %p0, 
; CHECK-NEXT:          |                             |        %p0;

  ; Verify the result is a vector of the right size.
  %v16 = and <4 x i1> %v0, %v1
  %v17 = and <4 x i1> %v2, %v3
  %v18 = and <4 x i1> %v4, %v5
  %v19 = and <4 x i1> %v6, %v7
  %v20 = and <4 x i1> %v8, %v9
  %v21 = and <4 x i1> %v10, %v11
  %v22 = and <4 x i1> %v12, %v13
  %v23 = and <4 x i1> %v14, %v15
  ret void

; CHECK-NEXT:    {{.*}}|    3: <2, 16, 15, 10>       |    %v16 = and <4 x i1> %v0, %v1;
; CHECK-NEXT:    {{.*}}|    3: <2, 15, 14, 10>       |    %v17 = and <4 x i1> %v2, %v3;
; CHECK-NEXT:    {{.*}}|    3: <2, 14, 13, 10>       |    %v18 = and <4 x i1> %v4, %v5;
; CHECK-NEXT:    {{.*}}|    3: <2, 13, 12, 10>       |    %v19 = and <4 x i1> %v6, %v7;
; CHECK-NEXT:    {{.*}}|    3: <2, 12, 11, 10>       |    %v20 = and <4 x i1> %v8, %v9;
; CHECK-NEXT:    {{.*}}|    3: <2, 11, 10, 10>       |    %v21 = and <4 x i1> %v10, %v11;
; CHECK-NEXT:    {{.*}}|    3: <2, 10, 9, 10>        |    %v22 = and <4 x i1> %v12, %v13;
; CHECK-NEXT:    {{.*}}|    3: <2, 9, 8, 10>         |    %v23 = and <4 x i1> %v14, %v15;
; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;

}

