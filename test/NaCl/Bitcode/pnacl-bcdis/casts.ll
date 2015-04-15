; Test dumping cast operations.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-bccompress --remove-abbreviations \
; RUN:              | pnacl-bcdis | FileCheck %s


; Test integer to integer casts.
define internal void @Int2IntCasts() {

; CHECK:               |                             |  %b0:

  ; Test truncation to i1.
  %v0 = trunc i8 1 to i1
  %v1 = trunc i16 2 to i1
  %v2 = trunc i32 3 to i1
  %v3 = trunc i64 4 to i1

; CHECK-NEXT:    {{.*}}|    3: <3, 3, 6, 0>          |    %v0 = trunc i8 %c2 to i1;
; CHECK-NEXT:    {{.*}}|    3: <3, 3, 6, 0>          |    %v1 = trunc i16 %c3 to i1;
; CHECK-NEXT:    {{.*}}|    3: <3, 7, 6, 0>          |    %v2 = trunc i32 %c0 to i1;
; CHECK-NEXT:    {{.*}}|    3: <3, 7, 6, 0>          |    %v3 = trunc i64 %c1 to i1;

  ; Verify i1 generated.
  %v4 = and i1 %v0, %v1
  %v5 = and i1 %v2, %v3

; CHECK-NEXT:    {{.*}}|    3: <2, 4, 3, 10>         |    %v4 = and i1 %v0, %v1;
; CHECK-NEXT:    {{.*}}|    3: <2, 3, 2, 10>         |    %v5 = and i1 %v2, %v3;

  ; Test truncation to i8.
  %v6 = trunc i16 2 to i8
  %v7 = trunc i32 3 to i8
  %v8 = trunc i64 4 to i8

; CHECK-NEXT:    {{.*}}|    3: <3, 8, 4, 0>          |    %v6 = trunc i16 %c3 to i8;
; CHECK-NEXT:    {{.*}}|    3: <3, 12, 4, 0>         |    %v7 = trunc i32 %c0 to i8;
; CHECK-NEXT:    {{.*}}|    3: <3, 12, 4, 0>         |    %v8 = trunc i64 %c1 to i8;

  ; Verify i8 generated.
  %v9 = add i8 %v6, %v7
  %v10 = add i8 %v8, %v8

; CHECK-NEXT:    {{.*}}|    3: <2, 3, 2, 0>          |    %v9 = add i8 %v6, %v7;
; CHECK-NEXT:    {{.*}}|    3: <2, 2, 2, 0>          |    %v10 = add i8 %v8, %v8;

  ; Test trunction to i16.
  %v11 = trunc i32 3 to i16
  %v12 = trunc i64 4 to i16

; CHECK-NEXT:    {{.*}}|    3: <3, 16, 5, 0>         |    %v11 = trunc i32 %c0 to i16;
; CHECK-NEXT:    {{.*}}|    3: <3, 16, 5, 0>         |    %v12 = trunc i64 %c1 to i16;

  ; Verify i16 generated.
  %v13 = add i16 %v11, %v12

; CHECK-NEXT:    {{.*}}|    3: <2, 2, 1, 0>          |    %v13 = add i16 %v11, %v12;
  
  ; Test truncation to i32.
  %v14 = trunc i64 4 to i32

; CHECK-NEXT:    {{.*}}|    3: <3, 18, 2, 0>         |    %v14 = trunc i64 %c1 to i32;

  ; Verify i32 generated.
  %v15 = add i32 %v14, %v14

; CHECK-NEXT:    {{.*}}|    3: <2, 1, 1, 0>          |    %v15 = add i32 %v14, %v14;

  ; Test zero extend to i8.
  %v16 = zext i1 0 to i8

; CHECK-NEXT:    {{.*}}|    3: <3, 17, 4, 1>         |    %v16 = zext i1 %c4 to i8;

  ; Verify i8 generated.
  %v17 = add i8 %v16, %v16

; CHECK-NEXT:    {{.*}}|    3: <2, 1, 1, 0>          |    %v17 = add i8 %v16, %v16;

  ; Test zero extend to i16.
  %v18 = zext i1 0 to i16  
  %v19 = zext i8 1 to i16

; CHECK-NEXT:    {{.*}}|    3: <3, 19, 5, 1>         |    %v18 = zext i1 %c4 to i16;
; CHECK-NEXT:    {{.*}}|    3: <3, 22, 5, 1>         |    %v19 = zext i8 %c2 to i16;

  ; Verify i16 generated.
  %v20 = add i16 %v18, %v19

; CHECK-NEXT:    {{.*}}|    3: <2, 2, 1, 0>          |    %v20 = add i16 %v18, %v19;

  ; Test zero extend to i32.
  %v21 = zext i1 0 to i32
  %v22 = zext i8 1 to i32
  %v23 = zext i16 2 to i32

; CHECK-NEXT:    {{.*}}|    3: <3, 22, 2, 1>         |    %v21 = zext i1 %c4 to i32;
; CHECK-NEXT:    {{.*}}|    3: <3, 25, 2, 1>         |    %v22 = zext i8 %c2 to i32;
; CHECK-NEXT:    {{.*}}|    3: <3, 25, 2, 1>         |    %v23 = zext i16 %c3 to i32;

  ; Verify i32 generated.
  %v24 = add i32 %v21, %v22
  %v25 = add i32 %v23, %v24

; CHECK-NEXT:    {{.*}}|    3: <2, 3, 2, 0>          |    %v24 = add i32 %v21, %v22;
; CHECK-NEXT:    {{.*}}|    3: <2, 2, 1, 0>          |    %v25 = add i32 %v23, %v24;

  ; Test zero extend to i64.
  %v26 = zext i1 0 to i64
  %v27 = zext i8 1 to i64
  %v28 = zext i16 2 to i64
  %v29 = zext i32 3 to i64

; CHECK-NEXT:    {{.*}}|    3: <3, 27, 3, 1>         |    %v26 = zext i1 %c4 to i64;
; CHECK-NEXT:    {{.*}}|    3: <3, 30, 3, 1>         |    %v27 = zext i8 %c2 to i64;
; CHECK-NEXT:    {{.*}}|    3: <3, 30, 3, 1>         |    %v28 = zext i16 %c3 to i64;
; CHECK-NEXT:    {{.*}}|    3: <3, 34, 3, 1>         |    %v29 = zext i32 %c0 to i64;

  ; Verify i64 generated.
  %v30 = add i64 %v26, %v27
  %v31 = add i64 %v28, %v29

; CHECK-NEXT:    {{.*}}|    3: <2, 4, 3, 0>          |    %v30 = add i64 %v26, %v27;
; CHECK-NEXT:    {{.*}}|    3: <2, 3, 2, 0>          |    %v31 = add i64 %v28, %v29;

  ; Test sign extend to i8.
  %v32 = sext i1 0 to i8

; CHECK-NEXT:    {{.*}}|    3: <3, 33, 4, 2>         |    %v32 = sext i1 %c4 to i8;

  ; Verify i8 generated.
  %v33 = add i8 %v32, %v32

; CHECK-NEXT:    {{.*}}|    3: <2, 1, 1, 0>          |    %v33 = add i8 %v32, %v32;

  ; Test sign extend to i16
  %v34 = sext i1 0 to i16
  %v35 = sext i8 1 to i16

; CHECK-NEXT:    {{.*}}|    3: <3, 35, 5, 2>         |    %v34 = sext i1 %c4 to i16;
; CHECK-NEXT:    {{.*}}|    3: <3, 38, 5, 2>         |    %v35 = sext i8 %c2 to i16;

  ; Verify i16 generated.
  %v36 = add i16 %v34, %v35

; CHECK-NEXT:    {{.*}}|    3: <2, 2, 1, 0>          |    %v36 = add i16 %v34, %v35;

  ; Test sign extend to i32.
  %v37 = sext i1 0 to i32
  %v38 = sext i8 1 to i32
  %v39 = sext i16 2 to i32

; CHECK-NEXT:    {{.*}}|    3: <3, 38, 2, 2>         |    %v37 = sext i1 %c4 to i32;
; CHECK-NEXT:    {{.*}}|    3: <3, 41, 2, 2>         |    %v38 = sext i8 %c2 to i32;
; CHECK-NEXT:    {{.*}}|    3: <3, 41, 2, 2>         |    %v39 = sext i16 %c3 to i32;

  ; Verify i32 generated.
  %v40 = add i32 %v37, %v38
  %v41 = add i32 %v39, %v40

; CHECK-NEXT:    {{.*}}|    3: <2, 3, 2, 0>          |    %v40 = add i32 %v37, %v38;
; CHECK-NEXT:    {{.*}}|    3: <2, 2, 1, 0>          |    %v41 = add i32 %v39, %v40;

  ; Test sign extend to i64.
  %v42 = sext i1 0 to i64
  %v43 = sext i8 1 to i64
  %v44 = sext i16 2 to i64
  %v45 = sext i32 3 to i64

; CHECK-NEXT:    {{.*}}|    3: <3, 43, 3, 2>         |    %v42 = sext i1 %c4 to i64;
; CHECK-NEXT:    {{.*}}|    3: <3, 46, 3, 2>         |    %v43 = sext i8 %c2 to i64;
; CHECK-NEXT:    {{.*}}|    3: <3, 46, 3, 2>         |    %v44 = sext i16 %c3 to i64;
; CHECK-NEXT:    {{.*}}|    3: <3, 50, 3, 2>         |    %v45 = sext i32 %c0 to i64;

  ; Verify i64 generated.
  %v46 = add i64 %v42, %v43
  %v47 = add i64 %v44, %v45
  ret void

; CHECK-NEXT:    {{.*}}|    3: <2, 4, 3, 0>          |    %v46 = add i64 %v42, %v43;
; CHECK-NEXT:    {{.*}}|    3: <2, 3, 2, 0>          |    %v47 = add i64 %v44, %v45;
; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;

}



; Test float to float casts.
define internal void @Float2FloatCasts(float %p0, double %p1) {

; CHECK:               |                             |  %b0:

  ; Test and verify truncation to float.
  %v0 = fptrunc double %p1 to float
  %v1 = fadd float %v0, %v0

; CHECK-NEXT:    {{.*}}|    3: <3, 1, 0, 7>          |    %v0 = fptrunc double %p1 to float;
; CHECK-NEXT:    {{.*}}|    3: <2, 1, 1, 0>          |    %v1 = fadd float %v0, %v0;

  ; Test and verify extending to double.
  %v2 = fpext float %p0 to double
  %v3 = fadd double %v2, %v2
  ret void

; CHECK-NEXT:    {{.*}}|    3: <3, 4, 1, 8>          |    %v2 = fpext float %p0 to double;
; CHECK-NEXT:    {{.*}}|    3: <2, 1, 1, 0>          |    %v3 = fadd double %v2, %v2;
; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;

}



; Test float to int casts.
define internal void @Float2IntCasts(float %p0, double %p1) {

; CHECK:               |                             |  %b0:

  ; Test and verify fptoui for i1.
  %v0 = fptoui float %p0 to i1
  %v1 = fptoui double %p1 to i1
  %v2 = and i1 %v0, %v1

; CHECK-NEXT:    {{.*}}|    3: <3, 2, 6, 3>          |    %v0 = fptoui float %p0 to i1;
; CHECK-NEXT:    {{.*}}|    3: <3, 2, 6, 3>          |    %v1 = fptoui double %p1 to i1;
; CHECK-NEXT:    {{.*}}|    3: <2, 2, 1, 10>         |    %v2 = and i1 %v0, %v1;

  ; Test and verify fptoui for i8.
  %v3 = fptoui float %p0 to i8
  %v4 = fptoui double %p1 to i8
  %v5 = add i8 %v3, %v4

; CHECK-NEXT:    {{.*}}|    3: <3, 5, 4, 3>          |    %v3 = fptoui float %p0 to i8;
; CHECK-NEXT:    {{.*}}|    3: <3, 5, 4, 3>          |    %v4 = fptoui double %p1 to i8;
; CHECK-NEXT:    {{.*}}|    3: <2, 2, 1, 0>          |    %v5 = add i8 %v3, %v4;

  ; Test and verify fptoui for i16.
  %v6 = fptoui float %p0 to i16
  %v7 = fptoui double %p1 to i16
  %v8 = add i16 %v6, %v7

; CHECK-NEXT:    {{.*}}|    3: <3, 8, 5, 3>          |    %v6 = fptoui float %p0 to i16;
; CHECK-NEXT:    {{.*}}|    3: <3, 8, 5, 3>          |    %v7 = fptoui double %p1 to i16;
; CHECK-NEXT:    {{.*}}|    3: <2, 2, 1, 0>          |    %v8 = add i16 %v6, %v7;

  ; Test and verify fptoui for i32.
  %v9 = fptoui float %p0 to i32
  %v10 = fptoui double %p1 to i32
  %v11 = and i32 %v9, %v10

; CHECK-NEXT:    {{.*}}|    3: <3, 11, 2, 3>         |    %v9 = fptoui float %p0 to i32;
; CHECK-NEXT:    {{.*}}|    3: <3, 11, 2, 3>         |    %v10 = fptoui double %p1 to i32;
; CHECK-NEXT:    {{.*}}|    3: <2, 2, 1, 10>         |    %v11 = and i32 %v9, %v10;

  ; Test and verify fptoui for i64
  %v12 = fptoui float %p0 to i64
  %v13 = fptoui double %p1 to i64
  %v14 = and i64 %v12, %v13

; CHECK-NEXT:    {{.*}}|    3: <3, 14, 3, 3>         |    %v12 = fptoui float %p0 to i64;
; CHECK-NEXT:    {{.*}}|    3: <3, 14, 3, 3>         |    %v13 = fptoui double %p1 to i64;
; CHECK-NEXT:    {{.*}}|    3: <2, 2, 1, 10>         |    %v14 = and i64 %v12, %v13;

  ; Test and verify fptosi for i1.
  %v15 = fptosi float %p0 to i1
  %v16 = fptosi double %p1 to i1
  %v17 = and i1 %v15, %v16

; CHECK-NEXT:    {{.*}}|    3: <3, 17, 6, 4>         |    %v15 = fptosi float %p0 to i1;
; CHECK-NEXT:    {{.*}}|    3: <3, 17, 6, 4>         |    %v16 = fptosi double %p1 to i1;
; CHECK-NEXT:    {{.*}}|    3: <2, 2, 1, 10>         |    %v17 = and i1 %v15, %v16;

  ; Test and verify fptosi for i8.
  %v18 = fptosi float %p0 to i8
  %v19 = fptosi double %p1 to i8
  %v20 = add i8 %v18, %v19

; CHECK-NEXT:    {{.*}}|    3: <3, 20, 4, 4>         |    %v18 = fptosi float %p0 to i8;
; CHECK-NEXT:    {{.*}}|    3: <3, 20, 4, 4>         |    %v19 = fptosi double %p1 to i8;
; CHECK-NEXT:    {{.*}}|    3: <2, 2, 1, 0>          |    %v20 = add i8 %v18, %v19;

  ; Test and verify fptosi for i16.
  %v21 = fptosi float %p0 to i16
  %v22 = fptosi double %p1 to i16
  %v23 = add i16 %v21, %v22

; CHECK-NEXT:    {{.*}}|    3: <3, 23, 5, 4>         |    %v21 = fptosi float %p0 to i16;
; CHECK-NEXT:    {{.*}}|    3: <3, 23, 5, 4>         |    %v22 = fptosi double %p1 to i16;
; CHECK-NEXT:    {{.*}}|    3: <2, 2, 1, 0>          |    %v23 = add i16 %v21, %v22;

  ; Test and verify fptosi for i32.
  %v24 = fptosi float %p0 to i32
  %v25 = fptosi double %p1 to i32
  %v26 = and i32 %v24, %v25

; CHECK-NEXT:    {{.*}}|    3: <3, 26, 2, 4>         |    %v24 = fptosi float %p0 to i32;
; CHECK-NEXT:    {{.*}}|    3: <3, 26, 2, 4>         |    %v25 = fptosi double %p1 to i32;
; CHECK-NEXT:    {{.*}}|    3: <2, 2, 1, 10>         |    %v26 = and i32 %v24, %v25;

  ; Test and verify fptosi for i64
  %v27 = fptosi float %p0 to i64
  %v28 = fptosi double %p1 to i64
  %v29 = and i64 %v27, %v28

; CHECK-NEXT:    {{.*}}|    3: <3, 29, 3, 4>         |    %v27 = fptosi float %p0 to i64;
; CHECK-NEXT:    {{.*}}|    3: <3, 29, 3, 4>         |    %v28 = fptosi double %p1 to i64;
; CHECK-NEXT:    {{.*}}|    3: <2, 2, 1, 10>         |    %v29 = and i64 %v27, %v28;

  ; Test and verify bitcast to i32.
  %v30 = bitcast float %p0 to i32
  %v31 = add i32 %v30, %v30

; CHECK-NEXT:    {{.*}}|    3: <3, 32, 2, 11>        |    %v30 = bitcast float %p0 to i32;
; CHECK-NEXT:    {{.*}}|    3: <2, 1, 1, 0>          |    %v31 = add i32 %v30, %v30;

  ; Test and verify bitcast to i64.
  %v32 = bitcast double %p1 to i64
  %v33 = add i64 %v32, %v32
  ret void

; CHECK-NEXT:    {{.*}}|    3: <3, 33, 3, 11>        |    %v32 = bitcast double %p1 to i64;
; CHECK-NEXT:    {{.*}}|    3: <2, 1, 1, 0>          |    %v33 = add i64 %v32, %v32;
; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;

}



; Test int to float casts.
define internal void @Int2FloatCasts() {

; CHECK:               |                             |  %b0:

  ; Test uitofp conversions to float.
  %v0 = uitofp i1 0 to float
  %v1 = uitofp i8 1 to float
  %v2 = uitofp i16 2 to float
  %v3 = uitofp i32 3 to float
  %v4 = uitofp i64 4 to float

; CHECK-NEXT:    {{.*}}|    3: <3, 1, 0, 5>          |    %v0 = uitofp i1 %c4 to float;
; CHECK-NEXT:    {{.*}}|    3: <3, 4, 0, 5>          |    %v1 = uitofp i8 %c2 to float;
; CHECK-NEXT:    {{.*}}|    3: <3, 4, 0, 5>          |    %v2 = uitofp i16 %c3 to float;
; CHECK-NEXT:    {{.*}}|    3: <3, 8, 0, 5>          |    %v3 = uitofp i32 %c0 to float;
; CHECK-NEXT:    {{.*}}|    3: <3, 8, 0, 5>          |    %v4 = uitofp i64 %c1 to float;

  ; Verify floats generated.
  %v5 = fadd float %v0, %v1
  %v6 = fadd float %v2, %v3
  %v7 = fadd float %v4, %v5

; CHECK-NEXT:    {{.*}}|    3: <2, 5, 4, 0>          |    %v5 = fadd float %v0, %v1;
; CHECK-NEXT:    {{.*}}|    3: <2, 4, 3, 0>          |    %v6 = fadd float %v2, %v3;
; CHECK-NEXT:    {{.*}}|    3: <2, 3, 2, 0>          |    %v7 = fadd float %v4, %v5;

  ; Test uitofp conversions to double
  %v8 = uitofp i1 0 to double
  %v9 = uitofp i8 1 to double
  %v10 = uitofp i16 2 to double
  %v11 = uitofp i32 3 to double
  %v12 = uitofp i64 4 to double

; CHECK-NEXT:    {{.*}}|    3: <3, 9, 1, 5>          |    %v8 = uitofp i1 %c4 to double;
; CHECK-NEXT:    {{.*}}|    3: <3, 12, 1, 5>         |    %v9 = uitofp i8 %c2 to double;
; CHECK-NEXT:    {{.*}}|    3: <3, 12, 1, 5>         |    %v10 = uitofp i16 %c3 to double;
; CHECK-NEXT:    {{.*}}|    3: <3, 16, 1, 5>         |    %v11 = uitofp i32 %c0 to double;
; CHECK-NEXT:    {{.*}}|    3: <3, 16, 1, 5>         |    %v12 = uitofp i64 %c1 to double;

  ; Verify doubles generated.
  %v13 = fadd double %v8, %v9
  %v14 = fadd double %v10, %v11
  %v15 = fadd double %v12, %v13

; CHECK-NEXT:    {{.*}}|    3: <2, 5, 4, 0>          |    %v13 = fadd double %v8, %v9;
; CHECK-NEXT:    {{.*}}|    3: <2, 4, 3, 0>          |    %v14 = fadd double %v10, %v11;
; CHECK-NEXT:    {{.*}}|    3: <2, 3, 2, 0>          |    %v15 = fadd double %v12, %v13;

  ; Test sitofp conversions to float.
  %v16 = sitofp i1 0 to float
  %v17 = sitofp i8 1 to float
  %v18 = sitofp i16 2 to float
  %v19 = sitofp i32 3 to float
  %v20 = sitofp i64 4 to float

; CHECK-NEXT:    {{.*}}|    3: <3, 17, 0, 6>         |    %v16 = sitofp i1 %c4 to float;
; CHECK-NEXT:    {{.*}}|    3: <3, 20, 0, 6>         |    %v17 = sitofp i8 %c2 to float;
; CHECK-NEXT:    {{.*}}|    3: <3, 20, 0, 6>         |    %v18 = sitofp i16 %c3 to float;
; CHECK-NEXT:    {{.*}}|    3: <3, 24, 0, 6>         |    %v19 = sitofp i32 %c0 to float;
; CHECK-NEXT:    {{.*}}|    3: <3, 24, 0, 6>         |    %v20 = sitofp i64 %c1 to float;

  ; Verify floats generated.
  %v21 = fadd float %v16, %v17
  %v22 = fadd float %v18, %v19
  %v23 = fadd float %v20, %v21

; CHECK-NEXT:    {{.*}}|    3: <2, 5, 4, 0>          |    %v21 = fadd float %v16, %v17;
; CHECK-NEXT:    {{.*}}|    3: <2, 4, 3, 0>          |    %v22 = fadd float %v18, %v19;
; CHECK-NEXT:    {{.*}}|    3: <2, 3, 2, 0>          |    %v23 = fadd float %v20, %v21;

  ; Test sitofp conversions to double
  %v24 = sitofp i1 0 to double
  %v25 = sitofp i8 1 to double
  %v26 = sitofp i16 2 to double
  %v27 = sitofp i32 3 to double
  %v28 = sitofp i64 4 to double

; CHECK-NEXT:    {{.*}}|    3: <3, 25, 1, 6>         |    %v24 = sitofp i1 %c4 to double;
; CHECK-NEXT:    {{.*}}|    3: <3, 28, 1, 6>         |    %v25 = sitofp i8 %c2 to double;
; CHECK-NEXT:    {{.*}}|    3: <3, 28, 1, 6>         |    %v26 = sitofp i16 %c3 to double;
; CHECK-NEXT:    {{.*}}|    3: <3, 32, 1, 6>         |    %v27 = sitofp i32 %c0 to double;
; CHECK-NEXT:    {{.*}}|    3: <3, 32, 1, 6>         |    %v28 = sitofp i64 %c1 to double;

  ; Verify doubles generated.
  %v29 = fadd double %v24, %v25
  %v30 = fadd double %v26, %v27
  %v31 = fadd double %v29, %v30
  ret void

; CHECK-NEXT:    {{.*}}|    3: <2, 5, 4, 0>          |    %v29 = fadd double %v24, %v25;
; CHECK-NEXT:    {{.*}}|    3: <2, 4, 3, 0>          |    %v30 = fadd double %v26, %v27;
; CHECK-NEXT:    {{.*}}|    3: <2, 2, 1, 0>          |    %v31 = fadd double %v29, %v30;
; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;

}

