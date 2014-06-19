; Test dumping binary operations.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-bccompress --remove-abbreviations \
; RUN:              | pnacl-bcdis | FileCheck %s

; Test integer binary operators.
define internal void @IntOps(i64 %p0) {
  ; Define different sized integer ops.
  %v0 = trunc i64 %p0 to i8
  %v1 = trunc i64 %p0 to i16
  %v2 = trunc i64 %p0 to i32
  %v3 = zext i32 %v2 to i64
  %v4 = trunc i32 %v2 to i1

  %v5 = add i8 %v0, 1
  %v6 = add i16 %v1, 2
  %v7 = add i32 %v2, 3
  %v8 = add i64 %v3, 4

; CHECK:         {{.*}}|    3: <2, 5, 8, 0>          |    %v5 = add i8 %v0, %c2;
; CHECK-NEXT:    {{.*}}|    3: <2, 5, 8, 0>          |    %v6 = add i16 %v1, %c3;
; CHECK-NEXT:    {{.*}}|    3: <2, 5, 11, 0>         |    %v7 = add i32 %v2, %c1;
; CHECK-NEXT:    {{.*}}|    3: <2, 5, 13, 0>         |    %v8 = add i64 %v3, %c0;

  %v9 = sub i8 1, %v0
  %v10 = sub i16 2, %v1
  %v11 = sub i32 3, %v2
  %v12 = sub i64 4, %v3

; CHECK-NEXT:    {{.*}}|    3: <2, 12, 9, 1>         |    %v9 = sub i8 %c2, %v0;
; CHECK-NEXT:    {{.*}}|    3: <2, 12, 9, 1>         |    %v10 = sub i16 %c3, %v1;
; CHECK-NEXT:    {{.*}}|    3: <2, 15, 9, 1>         |    %v11 = sub i32 %c1, %v2;
; CHECK-NEXT:    {{.*}}|    3: <2, 17, 9, 1>         |    %v12 = sub i64 %c0, %v3;

  %v13 = mul i8 %v0, 1
  %v14 = mul i16 %v1, 2
  %v15 = mul i32 %v2, 3
  %v16 = mul i64 %v3, 4

; CHECK-NEXT:    {{.*}}|    3: <2, 13, 16, 2>        |    %v13 = mul i8 %v0, %c2;
; CHECK-NEXT:    {{.*}}|    3: <2, 13, 16, 2>        |    %v14 = mul i16 %v1, %c3;
; CHECK-NEXT:    {{.*}}|    3: <2, 13, 19, 2>        |    %v15 = mul i32 %v2, %c1;
; CHECK-NEXT:    {{.*}}|    3: <2, 13, 21, 2>        |    %v16 = mul i64 %v3, %c0;

  %v17 = udiv i8 %v0, 1
  %v18 = udiv i16 %v1, 2
  %v19 = udiv i32 %v2, 3
  %v20 = udiv i64 %v3, 4

; CHECK-NEXT:    {{.*}}|    3: <2, 17, 20, 3>        |    %v17 = udiv i8 %v0, %c2;
; CHECK-NEXT:    {{.*}}|    3: <2, 17, 20, 3>        |    %v18 = udiv i16 %v1, %c3;
; CHECK-NEXT:    {{.*}}|    3: <2, 17, 23, 3>        |    %v19 = udiv i32 %v2, %c1;
; CHECK-NEXT:    {{.*}}|    3: <2, 17, 25, 3>        |    %v20 = udiv i64 %v3, %c0;

  %v21 = sdiv i8 1, %v0
  %v22 = sdiv i16 2, %v1
  %v23 = sdiv i32 3, %v2
  %v24 = sdiv i64 4, %v3

; CHECK-NEXT:    {{.*}}|    3: <2, 24, 21, 4>        |    %v21 = sdiv i8 %c2, %v0;
; CHECK-NEXT:    {{.*}}|    3: <2, 24, 21, 4>        |    %v22 = sdiv i16 %c3, %v1;
; CHECK-NEXT:    {{.*}}|    3: <2, 27, 21, 4>        |    %v23 = sdiv i32 %c1, %v2;
; CHECK-NEXT:    {{.*}}|    3: <2, 29, 21, 4>        |    %v24 = sdiv i64 %c0, %v3;

  %v25 = urem i8 1, %v0
  %v26 = urem i16 2, %v1
  %v27 = urem i32 3, %v2
  %v28 = urem i64 4, %v3

; CHECK-NEXT:    {{.*}}|    3: <2, 28, 25, 5>        |    %v25 = urem i8 %c2, %v0;
; CHECK-NEXT:    {{.*}}|    3: <2, 28, 25, 5>        |    %v26 = urem i16 %c3, %v1;
; CHECK-NEXT:    {{.*}}|    3: <2, 31, 25, 5>        |    %v27 = urem i32 %c1, %v2;
; CHECK-NEXT:    {{.*}}|    3: <2, 33, 25, 5>        |    %v28 = urem i64 %c0, %v3;

  %v29 = srem i8 %v0, 1
  %v30 = srem i16 %v1, 2
  %v31 = srem i32 %v2, 3
  %v32 = srem i64 %v3, 4

; CHECK-NEXT:    {{.*}}|    3: <2, 29, 32, 6>        |    %v29 = srem i8 %v0, %c2;
; CHECK-NEXT:    {{.*}}|    3: <2, 29, 32, 6>        |    %v30 = srem i16 %v1, %c3;
; CHECK-NEXT:    {{.*}}|    3: <2, 29, 35, 6>        |    %v31 = srem i32 %v2, %c1;
; CHECK-NEXT:    {{.*}}|    3: <2, 29, 37, 6>        |    %v32 = srem i64 %v3, %c0;

  %v33 = shl i8 1, %v0
  %v34 = shl i16 2, %v1
  %v35 = shl i32 3, %v2
  %v36 = shl i64 4, %v3

; CHECK-NEXT:    {{.*}}|    3: <2, 36, 33, 7>        |    %v33 = shl i8 %c2, %v0;
; CHECK-NEXT:    {{.*}}|    3: <2, 36, 33, 7>        |    %v34 = shl i16 %c3, %v1;
; CHECK-NEXT:    {{.*}}|    3: <2, 39, 33, 7>        |    %v35 = shl i32 %c1, %v2;
; CHECK-NEXT:    {{.*}}|    3: <2, 41, 33, 7>        |    %v36 = shl i64 %c0, %v3;

  %v37 = lshr i8 %v0, 1
  %v38 = lshr i16 %v1, 2
  %v39 = lshr i32 %v2, 3
  %v40 = lshr i64 %v3, 4

; CHECK-NEXT:    {{.*}}|    3: <2, 37, 40, 8>        |    %v37 = lshr i8 %v0, %c2;
; CHECK-NEXT:    {{.*}}|    3: <2, 37, 40, 8>        |    %v38 = lshr i16 %v1, %c3;
; CHECK-NEXT:    {{.*}}|    3: <2, 37, 43, 8>        |    %v39 = lshr i32 %v2, %c1;
; CHECK-NEXT:    {{.*}}|    3: <2, 37, 45, 8>        |    %v40 = lshr i64 %v3, %c0;

  %v41 = ashr i8 %v0, 1
  %v42 = ashr i16 %v1, 2
  %v43 = ashr i32 %v2, 3
  %v44 = ashr i64 %v3, 4

; CHECK-NEXT:    {{.*}}|    3: <2, 41, 44, 9>        |    %v41 = ashr i8 %v0, %c2;
; CHECK-NEXT:    {{.*}}|    3: <2, 41, 44, 9>        |    %v42 = ashr i16 %v1, %c3;
; CHECK-NEXT:    {{.*}}|    3: <2, 41, 47, 9>        |    %v43 = ashr i32 %v2, %c1;
; CHECK-NEXT:    {{.*}}|    3: <2, 41, 49, 9>        |    %v44 = ashr i64 %v3, %c0;

  %v45 = and i1 %v4, 0
  %v46 = and i8 %v0, 1
  %v47 = and i16 %v1, 2
  %v48 = and i32 %v2, 3
  %v49 = and i64 %v3, 4

; CHECK-NEXT:    {{.*}}|    3: <2, 41, 46, 10>       |    %v45 = and i1 %v4, %c4;
; CHECK-NEXT:    {{.*}}|    3: <2, 46, 49, 10>       |    %v46 = and i8 %v0, %c2;
; CHECK-NEXT:    {{.*}}|    3: <2, 46, 49, 10>       |    %v47 = and i16 %v1, %c3;
; CHECK-NEXT:    {{.*}}|    3: <2, 46, 52, 10>       |    %v48 = and i32 %v2, %c1;
; CHECK-NEXT:    {{.*}}|    3: <2, 46, 54, 10>       |    %v49 = and i64 %v3, %c0;

  %v50 = or i1 0, %v4
  %v51 = or i8 1, %v0
  %v52 = or i16 2, %v1
  %v53 = or i32 3, %v2
  %v54 = or i64 4, %v3

; CHECK-NEXT:    {{.*}}|    3: <2, 51, 46, 11>       |    %v50 = or i1 %c4, %v4;
; CHECK-NEXT:    {{.*}}|    3: <2, 54, 51, 11>       |    %v51 = or i8 %c2, %v0;
; CHECK-NEXT:    {{.*}}|    3: <2, 54, 51, 11>       |    %v52 = or i16 %c3, %v1;
; CHECK-NEXT:    {{.*}}|    3: <2, 57, 51, 11>       |    %v53 = or i32 %c1, %v2;
; CHECK-NEXT:    {{.*}}|    3: <2, 59, 51, 11>       |    %v54 = or i64 %c0, %v3;

  %v55 = xor i1 %v4, 0
  %v56 = xor i8 %v0, 1
  %v57 = xor i16 %v1, 2
  %v58 = xor i32 %v2, 3
  %v59 = xor i64 %v3, 4

; CHECK-NEXT:    {{.*}}|    3: <2, 51, 56, 12>       |    %v55 = xor i1 %v4, %c4;
; CHECK-NEXT:    {{.*}}|    3: <2, 56, 59, 12>       |    %v56 = xor i8 %v0, %c2;
; CHECK-NEXT:    {{.*}}|    3: <2, 56, 59, 12>       |    %v57 = xor i16 %v1, %c3;
; CHECK-NEXT:    {{.*}}|    3: <2, 56, 62, 12>       |    %v58 = xor i32 %v2, %c1;
; CHECK-NEXT:    {{.*}}|    3: <2, 56, 64, 12>       |    %v59 = xor i64 %v3, %c0;

  ret void
}



; Tests integer vector binary operations.
define internal void @IntVecOps(<16 x i8> %p0, <8 x i16> %p1, <4 x i32> %p2,
                                 <4 x i1> %p3, <8 x i1> %p4, <16 x i1> %p5) {

; CHECK:               |                             |  %b0:

  %v0 = add <16 x i8> %p0, %p0
  %v1 = add <8 x i16> %p1, %p1
  %v2 = add <4 x i32> %p2, %p2

; CHECK-NEXT:    {{.*}}|    3: <2, 6, 6, 0>          |    %v0 = add <16 x i8> %p0, %p0;
; CHECK-NEXT:    {{.*}}|    3: <2, 6, 6, 0>          |    %v1 = add <8 x i16> %p1, %p1;
; CHECK-NEXT:    {{.*}}|    3: <2, 6, 6, 0>          |    %v2 = add <4 x i32> %p2, %p2;

  %v3 = sub <16 x i8> %p0, %p0
  %v4 = sub <8 x i16> %p1, %p1
  %v5 = sub <4 x i32> %p2, %p2

; CHECK-NEXT:    {{.*}}|    3: <2, 9, 9, 1>          |    %v3 = sub <16 x i8> %p0, %p0;
; CHECK-NEXT:    {{.*}}|    3: <2, 9, 9, 1>          |    %v4 = sub <8 x i16> %p1, %p1;
; CHECK-NEXT:    {{.*}}|    3: <2, 9, 9, 1>          |    %v5 = sub <4 x i32> %p2, %p2;

  %v6 = mul <16 x i8> %p0, %p0
  %v7 = mul <8 x i16> %p1, %p1
  %v8 = mul <4 x i32> %p2, %p2

; CHECK-NEXT:    {{.*}}|    3: <2, 12, 12, 2>        |    %v6 = mul <16 x i8> %p0, %p0;
; CHECK-NEXT:    {{.*}}|    3: <2, 12, 12, 2>        |    %v7 = mul <8 x i16> %p1, %p1;
; CHECK-NEXT:    {{.*}}|    3: <2, 12, 12, 2>        |    %v8 = mul <4 x i32> %p2, %p2;

  %v9 = sdiv <16 x i8> %p0, %p0
  %v10 = sdiv <8 x i16> %p1, %p1
  %v11 = sdiv <4 x i32> %p2, %p2

; CHECK-NEXT:    {{.*}}|    3: <2, 15, 15, 4>        |    %v9 = sdiv <16 x i8> %p0, %p0;
; CHECK-NEXT:    {{.*}}|    3: <2, 15, 15, 4>        |    %v10 = sdiv <8 x i16> %p1, %p1;
; CHECK-NEXT:    {{.*}}|    3: <2, 15, 15, 4>        |    %v11 = sdiv <4 x i32> %p2, %p2;

  %v12 = udiv <16 x i8> %p0, %p0
  %v13 = udiv <8 x i16> %p1, %p1
  %v14 = udiv <4 x i32> %p2, %p2

; CHECK-NEXT:    {{.*}}|    3: <2, 18, 18, 3>        |    %v12 = udiv <16 x i8> %p0, %p0;
; CHECK-NEXT:    {{.*}}|    3: <2, 18, 18, 3>        |    %v13 = udiv <8 x i16> %p1, %p1;
; CHECK-NEXT:    {{.*}}|    3: <2, 18, 18, 3>        |    %v14 = udiv <4 x i32> %p2, %p2;

  %v15 = srem <16 x i8> %p0, %p0
  %v16 = srem <8 x i16> %p1, %p1
  %v17 = srem <4 x i32> %p2, %p2

; CHECK-NEXT:    {{.*}}|    3: <2, 21, 21, 6>        |    %v15 = srem <16 x i8> %p0, %p0;
; CHECK-NEXT:    {{.*}}|    3: <2, 21, 21, 6>        |    %v16 = srem <8 x i16> %p1, %p1;
; CHECK-NEXT:    {{.*}}|    3: <2, 21, 21, 6>        |    %v17 = srem <4 x i32> %p2, %p2;

  %v18 = urem <16 x i8> %p0, %p0
  %v19 = urem <8 x i16> %p1, %p1
  %v20 = urem <4 x i32> %p2, %p2

; CHECK-NEXT:    {{.*}}|    3: <2, 24, 24, 5>        |    %v18 = urem <16 x i8> %p0, %p0;
; CHECK-NEXT:    {{.*}}|    3: <2, 24, 24, 5>        |    %v19 = urem <8 x i16> %p1, %p1;
; CHECK-NEXT:    {{.*}}|    3: <2, 24, 24, 5>        |    %v20 = urem <4 x i32> %p2, %p2;

  %v21 = shl <16 x i8> %p0, %p0
  %v22 = shl <8 x i16> %p1, %p1
  %v23 = shl <4 x i32> %p2, %p2

; CHECK-NEXT:    {{.*}}|    3: <2, 27, 27, 7>        |    %v21 = shl <16 x i8> %p0, %p0;
; CHECK-NEXT:    {{.*}}|    3: <2, 27, 27, 7>        |    %v22 = shl <8 x i16> %p1, %p1;
; CHECK-NEXT:    {{.*}}|    3: <2, 27, 27, 7>        |    %v23 = shl <4 x i32> %p2, %p2;

  %v24 = lshr <16 x i8> %p0, %p0
  %v25 = lshr <8 x i16> %p1, %p1
  %v26 = lshr <4 x i32> %p2, %p2

; CHECK-NEXT:    {{.*}}|    3: <2, 30, 30, 8>        |    %v24 = lshr <16 x i8> %p0, %p0;
; CHECK-NEXT:    {{.*}}|    3: <2, 30, 30, 8>        |    %v25 = lshr <8 x i16> %p1, %p1;
; CHECK-NEXT:    {{.*}}|    3: <2, 30, 30, 8>        |    %v26 = lshr <4 x i32> %p2, %p2;

  %v27 = ashr <16 x i8> %p0, %p0
  %v28 = ashr <8 x i16> %p1, %p1
  %v29 = ashr <4 x i32> %p2, %p2

; CHECK-NEXT:    {{.*}}|    3: <2, 33, 33, 9>        |    %v27 = ashr <16 x i8> %p0, %p0;
; CHECK-NEXT:    {{.*}}|    3: <2, 33, 33, 9>        |    %v28 = ashr <8 x i16> %p1, %p1;
; CHECK-NEXT:    {{.*}}|    3: <2, 33, 33, 9>        |    %v29 = ashr <4 x i32> %p2, %p2;

  %v30 = and <16 x i8> %p0, %p0
  %v31 = and <8 x i16> %p1, %p1
  %v32 = and <4 x i32> %p2, %p2
  %v34 = and <4 x i1> %p3, %p3
  %v35 = and <8 x i1> %p4, %p4
  %v36 = and <16 x i1> %p5, %p5

; CHECK-NEXT:    {{.*}}|    3: <2, 36, 36, 10>       |    %v30 = and <16 x i8> %p0, %p0;
; CHECK-NEXT:    {{.*}}|    3: <2, 36, 36, 10>       |    %v31 = and <8 x i16> %p1, %p1;
; CHECK-NEXT:    {{.*}}|    3: <2, 36, 36, 10>       |    %v32 = and <4 x i32> %p2, %p2;
; CHECK-NEXT:    {{.*}}|    3: <2, 36, 36, 10>       |    %v33 = and <4 x i1> %p3, %p3;
; CHECK-NEXT:    {{.*}}|    3: <2, 36, 36, 10>       |    %v34 = and <8 x i1> %p4, %p4;
; CHECK-NEXT:    {{.*}}|    3: <2, 36, 36, 10>       |    %v35 = and <16 x i1> %p5, %p5;

  %v37 = or <16 x i8> %p0, %p0
  %v38 = or <8 x i16> %p1, %p1
  %v39 = or <4 x i32> %p2, %p2
  %v41 = or <4 x i1> %p3, %p3
  %v42 = or <8 x i1> %p4, %p4
  %v43 = or <16 x i1> %p5, %p5

; CHECK-NEXT:    {{.*}}|    3: <2, 42, 42, 11>       |    %v36 = or <16 x i8> %p0, %p0;
; CHECK-NEXT:    {{.*}}|    3: <2, 42, 42, 11>       |    %v37 = or <8 x i16> %p1, %p1;
; CHECK-NEXT:    {{.*}}|    3: <2, 42, 42, 11>       |    %v38 = or <4 x i32> %p2, %p2;
; CHECK-NEXT:    {{.*}}|    3: <2, 42, 42, 11>       |    %v39 = or <4 x i1> %p3, %p3;
; CHECK-NEXT:    {{.*}}|    3: <2, 42, 42, 11>       |    %v40 = or <8 x i1> %p4, %p4;
; CHECK-NEXT:    {{.*}}|    3: <2, 42, 42, 11>       |    %v41 = or <16 x i1> %p5, %p5;

  %v44 = xor <16 x i8> %p0, %p0
  %v45 = xor <8 x i16> %p1, %p1
  %v46 = xor <4 x i32> %p2, %p2
  %v48 = xor <4 x i1> %p3, %p3
  %v49 = xor <8 x i1> %p4, %p4
  %v50 = xor <16 x i1> %p5, %p5
  ret void

; CHECK-NEXT:    {{.*}}|    3: <2, 48, 48, 12>       |    %v42 = xor <16 x i8> %p0, %p0;
; CHECK-NEXT:    {{.*}}|    3: <2, 48, 48, 12>       |    %v43 = xor <8 x i16> %p1, %p1;
; CHECK-NEXT:    {{.*}}|    3: <2, 48, 48, 12>       |    %v44 = xor <4 x i32> %p2, %p2;
; CHECK-NEXT:    {{.*}}|    3: <2, 48, 48, 12>       |    %v45 = xor <4 x i1> %p3, %p3;
; CHECK-NEXT:    {{.*}}|    3: <2, 48, 48, 12>       |    %v46 = xor <8 x i1> %p4, %p4;
; CHECK-NEXT:    {{.*}}|    3: <2, 48, 48, 12>       |    %v47 = xor <16 x i1> %p5, %p5;
; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;

}



; Test floating point binary operations.
define internal void @FloatOps(float %p0, double %p1) {

; CHECK:               |                             |  %b0:

  %v0 = fadd float %p0, 1.0
  %v1 = fadd double %p1, 2.0

; CHECK-NEXT:    {{.*}}|    3: <2, 4, 2, 0>          |    %v0 = fadd float %p0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <2, 4, 2, 0>          |    %v1 = fadd double %p1, %c1;

  %v2 = fsub float %p0, 1.0
  %v3 = fsub double %p1, 2.0

; CHECK-NEXT:    {{.*}}|    3: <2, 6, 4, 1>          |    %v2 = fsub float %p0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <2, 6, 4, 1>          |    %v3 = fsub double %p1, %c1;

  %v4 = fmul float %p0, 1.0
  %v5 = fmul double %p1, 2.0

; CHECK-NEXT:    {{.*}}|    3: <2, 8, 6, 2>          |    %v4 = fmul float %p0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <2, 8, 6, 2>          |    %v5 = fmul double %p1, %c1;

  %v6 = fdiv float %p0, 1.0
  %v7 = fdiv double %p1, 2.0

; CHECK-NEXT:    {{.*}}|    3: <2, 10, 8, 4>         |    %v6 = fdiv float %p0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <2, 10, 8, 4>         |    %v7 = fdiv double %p1, %c1;

  %v8 = frem float %p0, 1.0
  %v9 = frem double %p1, 2.0
  ret void

; CHECK-NEXT:    {{.*}}|    3: <2, 12, 10, 6>        |    %v8 = frem float %p0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <2, 12, 10, 6>        |    %v9 = frem double %p1, %c1;
; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;

}



; Tests floating point vector binary operations.
define internal void @VecFloatOps(<4 x float> %p0) {

; CHECK:               |                             |  %b0:

  %v0 = fadd <4 x float> %p0, %p0
  %v2 = fsub <4 x float> %p0, %p0
  %v4 = fmul <4 x float> %p0, %p0
  %v6 = fdiv <4 x float> %p0, %p0
  %v8 = frem <4 x float> %p0, %p0
  ret void

; CHECK-NEXT:    {{.*}}|    3: <2, 1, 1, 0>          |    %v0 = fadd <4 x float> %p0, %p0;
; CHECK-NEXT:    {{.*}}|    3: <2, 2, 2, 1>          |    %v1 = fsub <4 x float> %p0, %p0;
; CHECK-NEXT:    {{.*}}|    3: <2, 3, 3, 2>          |    %v2 = fmul <4 x float> %p0, %p0;
; CHECK-NEXT:    {{.*}}|    3: <2, 4, 4, 4>          |    %v3 = fdiv <4 x float> %p0, %p0;
; CHECK-NEXT:    {{.*}}|    3: <2, 5, 5, 6>          |    %v4 = frem <4 x float> %p0, %p0;
; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;

}
