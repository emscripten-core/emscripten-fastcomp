; Simple test for phi nodes.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-bccompress --remove-abbreviations \
; RUN:              | pnacl-bcdis | FileCheck %s



; Test phi node introduced by branch merge.
define void @PhiBranchMerge() {


b0:
  br i1 0, label %b1, label %b2

; CHECK:               |                             |  %b0:
; CHECK-NEXT:    {{.*}}|    3: <11, 1, 2, 1>         |    br i1 %c1, label %b1, label %b2;

b1:
  %v0 = add i32 1, 1
  %v1 = sub i32 1, 1
  br label %b3

; CHECK-NEXT:          |                             |  %b1:
; CHECK-NEXT:    {{.*}}|    3: <2, 2, 2, 0>          |    %v0 = add i32 %c0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <2, 3, 3, 1>          |    %v1 = sub i32 %c0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <11, 3>               |    br label %b3;

b2:
  %v2 = mul i32 1, 1
  %v3 = udiv i32 1, 1
  br label %b3

; CHECK-NEXT:          |                             |  %b2:
; CHECK-NEXT:    {{.*}}|    3: <2, 4, 4, 2>          |    %v2 = mul i32 %c0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <2, 5, 5, 3>          |    %v3 = udiv i32 %c0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <11, 3>               |    br label %b3;

b3:
  %v4 = phi i32 [ %v0, %b1 ], [ %v2, %b2 ]
  %v5 = phi i32 [ %v1, %b1 ], [ %v3, %b2 ]
  ret void

; CHECK-NEXT:          |                             |  %b3:
; CHECK-NEXT:    {{.*}}|    3: <16, 0, 8, 1, 4, 2>   |    %v4 = phi i32 [%v0, %b1], 
; CHECK-NEXT:    {{.*}}|                             |        [%v2, %b2];
; CHECK-NEXT:    {{.*}}|    3: <16, 0, 8, 1, 4, 2>   |    %v5 = phi i32 [%v1, %b1], 
; CHECK-NEXT:          |                             |        [%v3, %b2];
; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;

}



; Test phi node introduced by loop.
define void @PhiLoopCarried(i32 %p0) {

b0:
  %v0 = add i32 %p0, 1
  br label %b1

; CHECK:               |                             |  %b0:
; CHECK-NEXT:    {{.*}}|    3: <2, 2, 1, 0>          |    %v0 = add i32 %p0, %c0;
; CHECK-NEXT:    {{.*}}|    3: <11, 1>               |    br label %b1;

b1:
  %v1 = phi i32 [%v0, %b0], [%v2, %b1]
  %v2 = add i32 %v1, 1
  br label %b1

; CHECK-NEXT:          |                             |  %b1:
; CHECK-NEXT:    {{.*}}|    3: <43, 6, 0>            |    declare i32 %v2;
; CHECK-NEXT:    {{.*}}|    3: <16, 0, 2, 0, 3, 1>   |    %v1 = phi i32 [%v0, %b0], 
; CHECK-NEXT:          |                             |        [%v2, %b1];
; CHECK-NEXT:    {{.*}}|    3: <2, 1, 3, 0>          |    %v2 = add i32 %v1, %c0;
; CHECK-NEXT:    {{.*}}|    3: <11, 1>               |    br label %b1;

}
