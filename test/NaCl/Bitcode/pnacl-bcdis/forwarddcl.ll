; Test if forward declare works.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-bccompress --remove-abbreviations \
; RUN:              | pnacl-bcdis | FileCheck %s

; Tests case where forward declare will be added because of loop carried
; dependency.
define void @LoopCarriedDep() {

; CHECK:         {{.*}}|    1: <65535, 11, 2>        |    constants {  // BlockID = 11
; CHECK-NEXT:    {{.*}}|      3: <1, 0>              |      i32:
; CHECK-NEXT:    {{.*}}|      3: <4, 2>              |        %c0 = i32 1;
; CHECK-NEXT:    {{.*}}|      3: <4, 4>              |        %c1 = i32 2;
; CHECK-NEXT:    {{.*}}|    0: <65534>               |      }

b0:
  %v0 = add i32 1, 2
  br label %b1

; CHECK-NEXT:          |                             |  %b0:
; CHECK-NEXT:    {{.*}}|    3: <2, 2, 1, 0>          |    %v0 = add i32 %c0, %c1;
; CHECK-NEXT:    {{.*}}|    3: <11, 1>               |    br label %b1;

b1:
  %v1 = phi i32 [%v0, %b0], [%v2, %b1]
  %v2 = add i32 %v1, 1
  br label %b1

; CHECK-NEXT:          |                             |  %b1:
; CHECK-NEXT:    {{.*}}|    3: <43, 6, 0>            |    declare i32 %v2;
; CHECK-NEXT:    {{.*}}|    3: <16, 0, 2, 0, 3, 1>   |    %v1 = phi i32 [%v0, %b0], 
; CHECK-NEXT:          |                             |        [%v2, %b1];
; CHECK-NEXT:    {{.*}}|    3: <2, 1, 4, 0>          |    %v2 = add i32 %v1, %c0;
; CHECK-NEXT:    {{.*}}|    3: <11, 1>               |    br label %b1;

}

; Test case where we backward branch.
define void @BackBranch(i32 %p0) {
  br label %b4

; CHECK:               |                             |  %b0:
; CHECK-NEXT:    {{.*}}|    3: <11, 4>               |    br label %b4;

b1:
  %v0 = add i32 %p0, %v3
  br label %b6

; CHECK-NEXT:          |                             |  %b1:
; CHECK-NEXT:    {{.*}}|    3: <43, 7, 0>            |    declare i32 %v3;
; CHECK-NEXT:    {{.*}}|    3: <2, 2, 4294967293, 0> |    %v0 = add i32 %p0, %v3;
; CHECK-NEXT:    {{.*}}|    3: <11, 6>               |    br label %b6;

b2:
  %v1 = add i32 %p0, %v4
  br label %b6

; CHECK-NEXT:          |                             |  %b2:
; CHECK-NEXT:    {{.*}}|    3: <43, 8, 0>            |    declare i32 %v4;
; CHECK-NEXT:    {{.*}}|    3: <2, 3, 4294967293, 0> |    %v1 = add i32 %p0, %v4;
; CHECK-NEXT:    {{.*}}|    3: <11, 6>               |    br label %b6;

b3:
  %v2 = add i32 %p0, %v3 ; No forward declare, already done!
  br label %b6

; CHECK-NEXT:          |                             |  %b3:
; CHECK-NEXT:    {{.*}}|    3: <2, 4, 4294967295, 0> |    %v2 = add i32 %p0, %v3;
; CHECK-NEXT:    {{.*}}|    3: <11, 6>               |    br label %b6;

b4:
  %v3 = add i32 %p0, %p0
  br i1 1, label %b1, label %b5

; CHECK-NEXT:          |                             |  %b4:
; CHECK-NEXT:    {{.*}}|    3: <2, 5, 5, 0>          |    %v3 = add i32 %p0, %p0;
; CHECK-NEXT:    {{.*}}|    3: <11, 1, 5, 5>         |    br i1 %c0, label %b1, label %b5;

b5:
  %v4 = add i32 %v3, %p0
  br i1 1, label %b2, label %b3

; CHECK-NEXT:          |                             |  %b5:
; CHECK-NEXT:    {{.*}}|    3: <2, 1, 6, 0>          |    %v4 = add i32 %v3, %p0;
; CHECK-NEXT:    {{.*}}|    3: <11, 2, 3, 6>         |    br i1 %c0, label %b2, label %b3;

b6:
  ret void

; CHECK-NEXT:          |                             |  %b6:
; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;

}

