; Test the select instruction.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-bccompress --remove-abbreviations \
; RUN:              | pnacl-bcdis | FileCheck %s

; Test selecting integer types.
define void @TestIntTypes() {

; CHECK:               |                             |  %b0:

  %v0 = select i1 0, i1 0, i1 0
  %v1 = select i1 %v0, i8 1, i8 1
  %v2 = select i1 %v0, i16 2, i16 2
  %v3 = select i1 %v0, i32 3, i32 3
  %v4 = select i1 %v0, i64 4, i64 4

; CHECK-NEXT:    {{.*}}|    3: <29, 5, 5, 5>         |    %v0 = select i1 %c0, i1 %c0, 
; CHECK-NEXT:          |                             |        i1 %c0;
; CHECK-NEXT:    {{.*}}|    3: <29, 4, 4, 1>         |    %v1 = select i1 %v0, i8 %c2, 
; CHECK-NEXT:          |                             |        i8 %c2;
; CHECK-NEXT:    {{.*}}|    3: <29, 4, 4, 2>         |    %v2 = select i1 %v0, i16 %c3, 
; CHECK-NEXT:          |                             |        i16 %c3;
; CHECK-NEXT:    {{.*}}|    3: <29, 7, 7, 3>         |    %v3 = select i1 %v0, i32 %c1, 
; CHECK-NEXT:          |                             |        i32 %c1;
; CHECK-NEXT:    {{.*}}|    3: <29, 5, 5, 4>         |    %v4 = select i1 %v0, i64 %c4, 
; CHECK-NEXT:          |                             |        i64 %c4;

  ; Verify computed results of right size.
  %v5 = and i1 %v0, %v0
  %v6 = add i8 %v1, %v1
  %v7 = add i16 %v2, %v2
  %v8 = add i32 %v3, %v3
  %v9 = add i64 %v4, %v4
  ret void

; CHECK-NEXT:    {{.*}}|    3: <2, 5, 5, 10>         |    %v5 = and i1 %v0, %v0;
; CHECK-NEXT:    {{.*}}|    3: <2, 5, 5, 0>          |    %v6 = add i8 %v1, %v1;
; CHECK-NEXT:    {{.*}}|    3: <2, 5, 5, 0>          |    %v7 = add i16 %v2, %v2;
; CHECK-NEXT:    {{.*}}|    3: <2, 5, 5, 0>          |    %v8 = add i32 %v3, %v3;
; CHECK-NEXT:    {{.*}}|    3: <2, 5, 5, 0>          |    %v9 = add i64 %v4, %v4;
; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;

}

; Test selecting floating types.
define void @TestFloatTypes(float %p0, double %p1) {

; CHECK:               |                             |  %b0:

  %v0 = select i1 0, float %p0, float %p0
  %v1 = select i1 0, double %p1, double %p1

; CHECK-NEXT:    {{.*}}|    3: <29, 3, 3, 1>         |    %v0 = select i1 %c0, float %p0, 
; CHECK-NEXT:          |                             |        float %p0;
; CHECK-NEXT:    {{.*}}|    3: <29, 3, 3, 2>         |    %v1 = select i1 %c0, double %p1, 
; CHECK-NEXT:          |                             |        double %p1;

  ; Verify computed results are of right size.
  %v2 = fadd float %v0, %v0
  %v3 = fadd double %v1, %v1
  ret void

; CHECK-NEXT:    {{.*}}|    3: <2, 2, 2, 0>          |    %v2 = fadd float %v0, %v0;
; CHECK-NEXT:    {{.*}}|    3: <2, 2, 2, 0>          |    %v3 = fadd double %v1, %v1;
; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;

}

; Test select on integer vectors
define void @TestVecIntTypes(i32 %p0, <16 x i8> %p1, <8 x i16> %p2, <4 x i32> %p3) {

  %v0 = trunc i32 %p0 to i1;
  %v1 = select i1 %v0, <16 x i8> %p1, <16 x i8> %p1
  %v2 = select i1 %v0, <8 x i16> %p2, <8 x i16> %p2
  %v3 = select i1 %v0, <4 x i32> %p3, <4 x i32> %p3

; CHECK:         {{.*}}|    {{.*}}                   |    {{.*}} trunc

; CHECK-NEXT:    {{.*}}|    3: <29, 4, 4, 1>         |    %v1 = select i1 %v0, <16 x i8> %p1,
; CHECK-NEXT:          |                             |        <16 x i8> %p1;
; CHECK-NEXT:    {{.*}}|    3: <29, 4, 4, 2>         |    %v2 = select i1 %v0, <8 x i16> %p2,
; CHECK-NEXT:          |                             |        <8 x i16> %p2;
; CHECK-NEXT:    {{.*}}|    3: <29, 4, 4, 3>         |    %v3 = select i1 %v0, <4 x i32> %p3,
; CHECK-NEXT:          |                             |        <4 x i32> %p3;

  ; Verify computed results are of right type.
  %v4 = and <16 x i8> %v1, %v1
  %v5 = add <8 x i16> %v2, %v2
  %v6 = add <4 x i32> %v3, %v3
  ret void

; CHECK-NEXT:    {{.*}}|    3: <2, 3, 3, 10>         |    %v4 = and <16 x i8> %v1, %v1;
; CHECK-NEXT:    {{.*}}|    3: <2, 3, 3, 0>          |    %v5 = add <8 x i16> %v2, %v2;
; CHECK-NEXT:    {{.*}}|    3: <2, 3, 3, 0>          |    %v6 = add <4 x i32> %v3, %v3;
; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;

}


; Test select on floating vectors
define void @TestVecFloatTypes(i32 %p0, <4 x float> %p1) {

  %v0 = trunc i32 %p0 to i1;  
  %v1 = select i1 %v0, <4 x float> %p1, <4 x float> %p1

; CHECK:         {{.*}}|    {{.*}}                   |    {{.*}} trunc
; CHECK-NEXT:    {{.*}}|    3: <29, 2, 2, 1>         |    %v1 = select i1 %v0, 
; CHECK-NEXT:          |                             |        <4 x float> %p1, 
; CHECK-NEXT:          |                             |        <4 x float> %p1;

  ; Verify computed results are of right type.
  %v2 = fadd <4 x float> %v1, %v1
  ret void

; CHECK-NEXT:    {{.*}}|    3: <2, 1, 1, 0>          |    %v2 = fadd <4 x float> %v1, %v1;
; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;

}
