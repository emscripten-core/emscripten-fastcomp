; Test switch statements.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-bccompress --remove-abbreviations \
; RUN:              | pnacl-bcdis | FileCheck %s


; Test case where we switch on a variable.
define void @SwitchVariable(i32 %p0) {

; CHECK:               |                             |  %b0:

  switch i32 %p0, label %b1 [
    i32 1, label %b2
    i32 2, label %b2
    i32 4, label %b3
    i32 5, label %b3
  ]

; CHECK-NEXT:    {{.*}}|    3: <12, 1, 1, 1, 4, 1, 1,|    switch i32 %p0 {
; CHECK-NEXT:          |        2, 2, 1, 1, 4, 2, 1, |      default: br label %b1;
; CHECK-NEXT:          |        1, 8, 3, 1, 1, 10, 3>|      i32 1: br label %b2;
; CHECK-NEXT:          |                             |      i32 2: br label %b2;
; CHECK-NEXT:          |                             |      i32 4: br label %b3;
; CHECK-NEXT:          |                             |      i32 5: br label %b3;
; CHECK-NEXT:          |                             |    }

b1:
  br label %b4


b2:
  br label %b4

b3:
  br label %b4

b4:
  ret void
}

; Test case where we switch on a constant.
define void @SwitchConstant() {

; CHECK:               |                             |  %b0:

  switch i32 3, label %b1 [
    i32 2, label %b2
    i32 1, label %b2
    i32 5, label %b3
    i32 4, label %b3
  ]

; CHECK-NEXT:    {{.*}}|    3: <12, 1, 1, 1, 4, 1, 1,|    switch i32 %c0 {
; CHECK-NEXT:          |        4, 2, 1, 1, 2, 2, 1, |      default: br label %b1;
; CHECK-NEXT:          |        1, 10, 3, 1, 1, 8, 3>|      i32 2: br label %b2;
; CHECK-NEXT:          |                             |      i32 1: br label %b2;
; CHECK-NEXT:          |                             |      i32 5: br label %b3;
; CHECK-NEXT:          |                             |      i32 4: br label %b3;
; CHECK-NEXT:          |                             |    }

b1:
  br label %b4

b2:
  br label %b4

b3:
  br label %b4

b4:
  ret void
}

