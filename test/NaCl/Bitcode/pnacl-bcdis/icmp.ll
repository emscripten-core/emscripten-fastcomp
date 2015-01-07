; Tests the icmp instruction.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-bccompress --remove-abbreviations \
; RUN:              | pnacl-bcdis | FileCheck %s

; Test icmp on primitive types.
define void @SimpleIcmpOps() {

; CHECK:         {{.*}}|    1: <65535, 11, 2>        |    constants {  // BlockID = 11
; CHECK-NEXT:    {{.*}}|      3: <1, 0>              |      i1:
; CHECK-NEXT:    {{.*}}|      3: <4, 0>              |        %c0 = i1 0;
; CHECK-NEXT:    {{.*}}|      3: <1, 4>              |      i8:
; CHECK-NEXT:    {{.*}}|      3: <4, 2>              |        %c1 = i8 1;
; CHECK-NEXT:    {{.*}}|      3: <1, 6>              |      i16:
; CHECK-NEXT:    {{.*}}|      3: <4, 4>              |        %c2 = i16 2;
; CHECK-NEXT:    {{.*}}|      3: <1, 8>              |      i32:
; CHECK-NEXT:    {{.*}}|      3: <4, 6>              |        %c3 = i32 3;
; CHECK-NEXT:    {{.*}}|      3: <1, 10>              |      i64:
; CHECK-NEXT:    {{.*}}|      3: <4, 8>              |        %c4 = i64 4;
; CHECK-NEXT:    {{.*}}|    0: <65534>               |      }
; CHECK-NEXT:          |                             |  %b0:

  %v0 = icmp eq i1 0, 0
  %v1 = icmp eq i8 1, 1
  %v2 = icmp eq i16 2, 2
  %v3 = icmp eq i32 3, 3
  %v4 = icmp eq i64 4, 4

; CHECK-NEXT:    {{.*}}|    3: <28, 5, 5, 32>       |    %v0 = icmp eq i1 %c0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <28, 5, 5, 32>       |    %v1 = icmp eq i8 %c1, %c1;
; CHECK-NEXT:    {{.*}}|    3: <28, 5, 5, 32>       |    %v2 = icmp eq i16 %c2, %c2;
; CHECK-NEXT:    {{.*}}|    3: <28, 5, 5, 32>       |    %v3 = icmp eq i32 %c3, %c3;
; CHECK-NEXT:    {{.*}}|    3: <28, 5, 5, 32>       |    %v4 = icmp eq i64 %c4, %c4;

  %v5 = icmp ne i1 0, 0
  %v6 = icmp ne i8 1, 1
  %v7 = icmp ne i16 2, 2
  %v8 = icmp ne i32 3, 3
  %v9 = icmp ne i64 4, 4

; CHECK-NEXT:    {{.*}}|    3: <28, 10, 10, 33>      |    %v5 = icmp ne i1 %c0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <28, 10, 10, 33>      |    %v6 = icmp ne i8 %c1, %c1;
; CHECK-NEXT:    {{.*}}|    3: <28, 10, 10, 33>      |    %v7 = icmp ne i16 %c2, %c2;
; CHECK-NEXT:    {{.*}}|    3: <28, 10, 10, 33>      |    %v8 = icmp ne i32 %c3, %c3;
; CHECK-NEXT:    {{.*}}|    3: <28, 10, 10, 33>      |    %v9 = icmp ne i64 %c4, %c4;

  %v10 = icmp ugt i1 0, 0
  %v11 = icmp ugt i8 1, 1
  %v12 = icmp ugt i16 2, 2
  %v13 = icmp ugt i32 3, 3
  %v14 = icmp ugt i64 4, 4

; CHECK-NEXT:    {{.*}}|    3: <28, 15, 15, 34>      |    %v10 = icmp ugt i1 %c0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <28, 15, 15, 34>      |    %v11 = icmp ugt i8 %c1, %c1;
; CHECK-NEXT:    {{.*}}|    3: <28, 15, 15, 34>      |    %v12 = icmp ugt i16 %c2, %c2;
; CHECK-NEXT:    {{.*}}|    3: <28, 15, 15, 34>      |    %v13 = icmp ugt i32 %c3, %c3;
; CHECK-NEXT:    {{.*}}|    3: <28, 15, 15, 34>      |    %v14 = icmp ugt i64 %c4, %c4;

  %v15 = icmp uge i1 0, 0
  %v16 = icmp uge i8 1, 1
  %v17 = icmp uge i16 2, 2
  %v18 = icmp uge i32 3, 3
  %v19 = icmp uge i64 4, 4

; CHECK-NEXT:    {{.*}}|    3: <28, 20, 20, 35>      |    %v15 = icmp uge i1 %c0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <28, 20, 20, 35>      |    %v16 = icmp uge i8 %c1, %c1;
; CHECK-NEXT:    {{.*}}|    3: <28, 20, 20, 35>      |    %v17 = icmp uge i16 %c2, %c2;
; CHECK-NEXT:    {{.*}}|    3: <28, 20, 20, 35>      |    %v18 = icmp uge i32 %c3, %c3;
; CHECK-NEXT:    {{.*}}|    3: <28, 20, 20, 35>      |    %v19 = icmp uge i64 %c4, %c4;

  %v20 = icmp ult i1 0, 0
  %v21 = icmp ult i8 1, 1
  %v22 = icmp ult i16 2, 2
  %v23 = icmp ult i32 3, 3
  %v24 = icmp ult i64 4, 4

; CHECK-NEXT:    {{.*}}|    3: <28, 25, 25, 36>      |    %v20 = icmp ult i1 %c0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <28, 25, 25, 36>      |    %v21 = icmp ult i8 %c1, %c1;
; CHECK-NEXT:    {{.*}}|    3: <28, 25, 25, 36>      |    %v22 = icmp ult i16 %c2, %c2;
; CHECK-NEXT:    {{.*}}|    3: <28, 25, 25, 36>      |    %v23 = icmp ult i32 %c3, %c3;
; CHECK-NEXT:    {{.*}}|    3: <28, 25, 25, 36>      |    %v24 = icmp ult i64 %c4, %c4;

  %v25 = icmp ule i1 0, 0
  %v26 = icmp ule i8 1, 1
  %v27 = icmp ule i16 2, 2
  %v28 = icmp ule i32 3, 3
  %v29 = icmp ule i64 4, 4

; CHECK-NEXT:    {{.*}}|    3: <28, 30, 30, 37>      |    %v25 = icmp ule i1 %c0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <28, 30, 30, 37>      |    %v26 = icmp ule i8 %c1, %c1;
; CHECK-NEXT:    {{.*}}|    3: <28, 30, 30, 37>      |    %v27 = icmp ule i16 %c2, %c2;
; CHECK-NEXT:    {{.*}}|    3: <28, 30, 30, 37>      |    %v28 = icmp ule i32 %c3, %c3;
; CHECK-NEXT:    {{.*}}|    3: <28, 30, 30, 37>      |    %v29 = icmp ule i64 %c4, %c4;

  %v30 = icmp sgt i1 0, 0
  %v31 = icmp sgt i8 1, 1
  %v32 = icmp sgt i16 2, 2
  %v33 = icmp sgt i32 3, 3
  %v34 = icmp sgt i64 4, 4

; CHECK-NEXT:    {{.*}}|    3: <28, 35, 35, 38>      |    %v30 = icmp sgt i1 %c0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <28, 35, 35, 38>      |    %v31 = icmp sgt i8 %c1, %c1;
; CHECK-NEXT:    {{.*}}|    3: <28, 35, 35, 38>      |    %v32 = icmp sgt i16 %c2, %c2;
; CHECK-NEXT:    {{.*}}|    3: <28, 35, 35, 38>      |    %v33 = icmp sgt i32 %c3, %c3;
; CHECK-NEXT:    {{.*}}|    3: <28, 35, 35, 38>      |    %v34 = icmp sgt i64 %c4, %c4;

  %v35 = icmp sge i1 0, 0
  %v36 = icmp sge i8 1, 1
  %v37 = icmp sge i16 2, 2
  %v38 = icmp sge i32 3, 3
  %v39 = icmp sge i64 4, 4

; CHECK-NEXT:    {{.*}}|    3: <28, 40, 40, 39>      |    %v35 = icmp sge i1 %c0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <28, 40, 40, 39>      |    %v36 = icmp sge i8 %c1, %c1;
; CHECK-NEXT:    {{.*}}|    3: <28, 40, 40, 39>      |    %v37 = icmp sge i16 %c2, %c2;
; CHECK-NEXT:    {{.*}}|    3: <28, 40, 40, 39>      |    %v38 = icmp sge i32 %c3, %c3;
; CHECK-NEXT:    {{.*}}|    3: <28, 40, 40, 39>      |    %v39 = icmp sge i64 %c4, %c4;

  %v40 = icmp slt i1 0, 0
  %v41 = icmp slt i8 1, 1
  %v42 = icmp slt i16 2, 2
  %v43 = icmp slt i32 3, 3
  %v44 = icmp slt i64 4, 4

; CHECK-NEXT:    {{.*}}|    3: <28, 45, 45, 40>      |    %v40 = icmp slt i1 %c0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <28, 45, 45, 40>      |    %v41 = icmp slt i8 %c1, %c1;
; CHECK-NEXT:    {{.*}}|    3: <28, 45, 45, 40>      |    %v42 = icmp slt i16 %c2, %c2;
; CHECK-NEXT:    {{.*}}|    3: <28, 45, 45, 40>      |    %v43 = icmp slt i32 %c3, %c3;
; CHECK-NEXT:    {{.*}}|    3: <28, 45, 45, 40>      |    %v44 = icmp slt i64 %c4, %c4;

  %v45 = icmp sle i1 0, 0
  %v46 = icmp sle i8 1, 1
  %v47 = icmp sle i16 2, 2
  %v48 = icmp sle i32 3, 3
  %v49 = icmp sle i64 4, 4

; CHECK-NEXT:    {{.*}}|    3: <28, 50, 50, 41>      |    %v45 = icmp sle i1 %c0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <28, 50, 50, 41>      |    %v46 = icmp sle i8 %c1, %c1;
; CHECK-NEXT:    {{.*}}|    3: <28, 50, 50, 41>      |    %v47 = icmp sle i16 %c2, %c2;
; CHECK-NEXT:    {{.*}}|    3: <28, 50, 50, 41>      |    %v48 = icmp sle i32 %c3, %c3;
; CHECK-NEXT:    {{.*}}|    3: <28, 50, 50, 41>      |    %v49 = icmp sle i64 %c4, %c4;

  ; Verifies result is i1.
  %v50 = and i1 %v0, %v1
  %v51 = and i1 %v2, %v3
  %v52 = and i1 %v4, %v5
  %v53 = and i1 %v6, %v7
  %v54 = and i1 %v8, %v9
  %v55 = and i1 %v10, %v11
  %v56 = and i1 %v12, %v13
  %v57 = and i1 %v14, %v15
  %v58 = and i1 %v16, %v17
  %v59 = and i1 %v18, %v19
  %v60 = and i1 %v20, %v21
  %v61 = and i1 %v22, %v23
  %v62 = and i1 %v24, %v25
  %v63 = and i1 %v26, %v27
  %v64 = and i1 %v28, %v29
  %v65 = and i1 %v30, %v31
  %v66 = and i1 %v32, %v33
  %v67 = and i1 %v34, %v35
  %v68 = and i1 %v36, %v37
  %v69 = and i1 %v38, %v39
  ret void

; CHECK-NEXT:    {{.*}}|    3: <2, 50, 49, 10>       |    %v50 = and i1 %v0, %v1;
; CHECK-NEXT:    {{.*}}|    3: <2, 49, 48, 10>       |    %v51 = and i1 %v2, %v3;
; CHECK-NEXT:    {{.*}}|    3: <2, 48, 47, 10>       |    %v52 = and i1 %v4, %v5;
; CHECK-NEXT:    {{.*}}|    3: <2, 47, 46, 10>       |    %v53 = and i1 %v6, %v7;
; CHECK-NEXT:    {{.*}}|    3: <2, 46, 45, 10>       |    %v54 = and i1 %v8, %v9;
; CHECK-NEXT:    {{.*}}|    3: <2, 45, 44, 10>       |    %v55 = and i1 %v10, %v11;
; CHECK-NEXT:    {{.*}}|    3: <2, 44, 43, 10>       |    %v56 = and i1 %v12, %v13;
; CHECK-NEXT:    {{.*}}|    3: <2, 43, 42, 10>       |    %v57 = and i1 %v14, %v15;
; CHECK-NEXT:    {{.*}}|    3: <2, 42, 41, 10>       |    %v58 = and i1 %v16, %v17;
; CHECK-NEXT:    {{.*}}|    3: <2, 41, 40, 10>       |    %v59 = and i1 %v18, %v19;
; CHECK-NEXT:    {{.*}}|    3: <2, 40, 39, 10>       |    %v60 = and i1 %v20, %v21;
; CHECK-NEXT:    {{.*}}|    3: <2, 39, 38, 10>       |    %v61 = and i1 %v22, %v23;
; CHECK-NEXT:    {{.*}}|    3: <2, 38, 37, 10>       |    %v62 = and i1 %v24, %v25;
; CHECK-NEXT:    {{.*}}|    3: <2, 37, 36, 10>       |    %v63 = and i1 %v26, %v27;
; CHECK-NEXT:    {{.*}}|    3: <2, 36, 35, 10>       |    %v64 = and i1 %v28, %v29;
; CHECK-NEXT:    {{.*}}|    3: <2, 35, 34, 10>       |    %v65 = and i1 %v30, %v31;
; CHECK-NEXT:    {{.*}}|    3: <2, 34, 33, 10>       |    %v66 = and i1 %v32, %v33;
; CHECK-NEXT:    {{.*}}|    3: <2, 33, 32, 10>       |    %v67 = and i1 %v34, %v35;
; CHECK-NEXT:    {{.*}}|    3: <2, 32, 31, 10>       |    %v68 = and i1 %v36, %v37;
; CHECK-NEXT:    {{.*}}|    3: <2, 31, 30, 10>       |    %v69 = and i1 %v38, %v39;
; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;

}


; Tests integer vector compares.
define void @VectorIcmpOps(<16 x i8> %p0, <8 x i16> %p1, <4 x i32> %p2,
                           <16 x i1> %p3, <8 x i1> %p4, <4 x i1> %p5) {

; CHECK:         {{.*}}|    3: <1, 1>                |    blocks 1;
; CHECK-NEXT:          |                             |  %b0:

  %v0 = icmp eq <16 x i8> %p0, %p0
  %v1 = icmp eq <8 x i16> %p1, %p1
  %v2 = icmp eq <4 x i32> %p2, %p2

; CHECK-NEXT:    {{.*}}|    3: <28, 6, 6, 32>        |    %v0 = icmp eq <16 x i8> %p0, %p0;
; CHECK-NEXT:    {{.*}}|    3: <28, 6, 6, 32>        |    %v1 = icmp eq <8 x i16> %p1, %p1;
; CHECK-NEXT:    {{.*}}|    3: <28, 6, 6, 32>        |    %v2 = icmp eq <4 x i32> %p2, %p2;

  %v3 = icmp ne <16 x i8> %p0, %p0
  %v4 = icmp ne <8 x i16> %p1, %p1
  %v5 = icmp ne <4 x i32> %p2, %p2

; CHECK-NEXT:    {{.*}}|    3: <28, 9, 9, 33>        |    %v3 = icmp ne <16 x i8> %p0, %p0;
; CHECK-NEXT:    {{.*}}|    3: <28, 9, 9, 33>        |    %v4 = icmp ne <8 x i16> %p1, %p1;
; CHECK-NEXT:    {{.*}}|    3: <28, 9, 9, 33>        |    %v5 = icmp ne <4 x i32> %p2, %p2;

  %v6 = icmp ugt <16 x i8> %p0, %p0
  %v7 = icmp ugt <8 x i16> %p1, %p1
  %v8 = icmp ugt <4 x i32> %p2, %p2

; CHECK-NEXT:    {{.*}}|    3: <28, 12, 12, 34>      |    %v6 = icmp ugt <16 x i8> %p0, %p0;
; CHECK-NEXT:    {{.*}}|    3: <28, 12, 12, 34>      |    %v7 = icmp ugt <8 x i16> %p1, %p1;
; CHECK-NEXT:    {{.*}}|    3: <28, 12, 12, 34>      |    %v8 = icmp ugt <4 x i32> %p2, %p2;

  %v9 = icmp uge <16 x i8> %p0, %p0
  %v10 = icmp uge <8 x i16> %p1, %p1
  %v11 = icmp uge <4 x i32> %p2, %p2

; CHECK-NEXT:    {{.*}}|    3: <28, 15, 15, 35>      |    %v9 = icmp uge <16 x i8> %p0, %p0;
; CHECK-NEXT:    {{.*}}|    3: <28, 15, 15, 35>      |    %v10 = icmp uge <8 x i16> %p1, %p1;
; CHECK-NEXT:    {{.*}}|    3: <28, 15, 15, 35>      |    %v11 = icmp uge <4 x i32> %p2, %p2;

  %v12 = icmp ult <16 x i8> %p0, %p0
  %v13 = icmp ult <8 x i16> %p1, %p1
  %v14 = icmp ult <4 x i32> %p2, %p2

; CHECK-NEXT:    {{.*}}|    3: <28, 18, 18, 36>      |    %v12 = icmp ult <16 x i8> %p0, %p0;
; CHECK-NEXT:    {{.*}}|    3: <28, 18, 18, 36>      |    %v13 = icmp ult <8 x i16> %p1, %p1;
; CHECK-NEXT:    {{.*}}|    3: <28, 18, 18, 36>      |    %v14 = icmp ult <4 x i32> %p2, %p2;

  %v15 = icmp ule <16 x i8> %p0, %p0
  %v16 = icmp ule <8 x i16> %p1, %p1
  %v17 = icmp ule <4 x i32> %p2, %p2

; CHECK-NEXT:    {{.*}}|    3: <28, 21, 21, 37>      |    %v15 = icmp ule <16 x i8> %p0, %p0;
; CHECK-NEXT:    {{.*}}|    3: <28, 21, 21, 37>      |    %v16 = icmp ule <8 x i16> %p1, %p1;
; CHECK-NEXT:    {{.*}}|    3: <28, 21, 21, 37>      |    %v17 = icmp ule <4 x i32> %p2, %p2;

  %v18 = icmp sgt <16 x i8> %p0, %p0
  %v19 = icmp sgt <8 x i16> %p1, %p1
  %v20 = icmp sgt <4 x i32> %p2, %p2

; CHECK-NEXT:    {{.*}}|    3: <28, 24, 24, 38>      |    %v18 = icmp sgt <16 x i8> %p0, %p0;
; CHECK-NEXT:    {{.*}}|    3: <28, 24, 24, 38>      |    %v19 = icmp sgt <8 x i16> %p1, %p1;
; CHECK-NEXT:    {{.*}}|    3: <28, 24, 24, 38>      |    %v20 = icmp sgt <4 x i32> %p2, %p2;

  %v21 = icmp sge <16 x i8> %p0, %p0
  %v22 = icmp sge <8 x i16> %p1, %p1
  %v23 = icmp sge <4 x i32> %p2, %p2

; CHECK-NEXT:    {{.*}}|    3: <28, 27, 27, 39>      |    %v21 = icmp sge <16 x i8> %p0, %p0;
; CHECK-NEXT:    {{.*}}|    3: <28, 27, 27, 39>      |    %v22 = icmp sge <8 x i16> %p1, %p1;
; CHECK-NEXT:    {{.*}}|    3: <28, 27, 27, 39>      |    %v23 = icmp sge <4 x i32> %p2, %p2;

  %v24 = icmp slt <16 x i8> %p0, %p0
  %v25 = icmp slt <8 x i16> %p1, %p1
  %v26 = icmp slt <4 x i32> %p2, %p2

; CHECK-NEXT:    {{.*}}|    3: <28, 30, 30, 40>      |    %v24 = icmp slt <16 x i8> %p0, %p0;
; CHECK-NEXT:    {{.*}}|    3: <28, 30, 30, 40>      |    %v25 = icmp slt <8 x i16> %p1, %p1;
; CHECK-NEXT:    {{.*}}|    3: <28, 30, 30, 40>      |    %v26 = icmp slt <4 x i32> %p2, %p2;

  %v27 = icmp sle <16 x i8> %p0, %p0
  %v28 = icmp sle <8 x i16> %p1, %p1
  %v29 = icmp sle <4 x i32> %p2, %p2

; CHECK-NEXT:    {{.*}}|    3: <28, 33, 33, 41>      |    %v27 = icmp sle <16 x i8> %p0, %p0;
; CHECK-NEXT:    {{.*}}|    3: <28, 33, 33, 41>      |    %v28 = icmp sle <8 x i16> %p1, %p1;
; CHECK-NEXT:    {{.*}}|    3: <28, 33, 33, 41>      |    %v29 = icmp sle <4 x i32> %p2, %p2;

  ; Verify result types are vectors of right size.
  %v30 = and <16 x i1> %v0, %v3
  %v31 = and <16 x i1> %v6, %v9
  %v32 = and <16 x i1> %v12, %v15
  %v33 = and <16 x i1> %v18, %v21
  %v34 = and <16 x i1> %v24, %v27

; CHECK-NEXT:    {{.*}}|    3: <2, 30, 27, 10>       |    %v30 = and <16 x i1> %v0, %v3;
; CHECK-NEXT:    {{.*}}|    3: <2, 25, 22, 10>       |    %v31 = and <16 x i1> %v6, %v9;
; CHECK-NEXT:    {{.*}}|    3: <2, 20, 17, 10>       |    %v32 = and <16 x i1> %v12, %v15;
; CHECK-NEXT:    {{.*}}|    3: <2, 15, 12, 10>       |    %v33 = and <16 x i1> %v18, %v21;
; CHECK-NEXT:    {{.*}}|    3: <2, 10, 7, 10>        |    %v34 = and <16 x i1> %v24, %v27;

  %v35 = and <8 x i1> %v1, %v4
  %v36 = and <8 x i1> %v7, %v10
  %v37 = and <8 x i1> %v13, %v16
  %v38 = and <8 x i1> %v19, %v22
  %v39 = and <8 x i1> %v25, %v28

; CHECK-NEXT:    {{.*}}|    3: <2, 34, 31, 10>       |    %v35 = and <8 x i1> %v1, %v4;
; CHECK-NEXT:    {{.*}}|    3: <2, 29, 26, 10>       |    %v36 = and <8 x i1> %v7, %v10;
; CHECK-NEXT:    {{.*}}|    3: <2, 24, 21, 10>       |    %v37 = and <8 x i1> %v13, %v16;
; CHECK-NEXT:    {{.*}}|    3: <2, 19, 16, 10>       |    %v38 = and <8 x i1> %v19, %v22;
; CHECK-NEXT:    {{.*}}|    3: <2, 14, 11, 10>       |    %v39 = and <8 x i1> %v25, %v28;


  %v40 = and <4 x i1> %v2, %v5
  %v41 = and <4 x i1> %v8, %v11
  %v42 = and <4 x i1> %v14, %v17
  %v43 = and <4 x i1> %v20, %v23
  %v44 = and <4 x i1> %v26, %v29
  ret void

; CHECK-NEXT:    {{.*}}|    3: <2, 38, 35, 10>       |    %v40 = and <4 x i1> %v2, %v5;
; CHECK-NEXT:    {{.*}}|    3: <2, 33, 30, 10>       |    %v41 = and <4 x i1> %v8, %v11;
; CHECK-NEXT:    {{.*}}|    3: <2, 28, 25, 10>       |    %v42 = and <4 x i1> %v14, %v17;
; CHECK-NEXT:    {{.*}}|    3: <2, 23, 20, 10>       |    %v43 = and <4 x i1> %v20, %v23;
; CHECK-NEXT:    {{.*}}|    3: <2, 18, 15, 10>       |    %v44 = and <4 x i1> %v26, %v29;
; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;

}
