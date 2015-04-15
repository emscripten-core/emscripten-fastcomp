; Test branch instructions.

; RUN: llvm-as < %s | pnacl-freeze | pnacl-bccompress --remove-abbreviations \
; RUN:              | pnacl-bcdis | FileCheck %s


define void @Ex1() {

; CHECK:               |                             |  %b0:

  br label %b3

; CHECK-NEXT:    {{.*}}|    3: <11, 3>               |    br label %b3;

b1:
  br i1 1, label %b2, label %b4

; CHECK-NEXT:          |                             |  %b1:
; CHECK-NEXT:    {{.*}}|    3: <11, 2, 4, 1>         |    br i1 %c0, label %b2, label %b4;

b2:
  br label %b1

; CHECK-NEXT:          |                             |  %b2:
; CHECK-NEXT:    {{.*}}|    3: <11, 1>               |    br label %b1;

b3:
  ret void

; CHECK-NEXT:          |                             |  %b3:
; CHECK-NEXT:    {{.*}}|    3: <10>                  |    ret void;

b4:
  br label %b3
}

; CHECK-NEXT:          |                             |  %b4:
; CHECK-NEXT:    {{.*}}|    3: <11, 3>               |    br label %b3;
; CHECK-NEXT:    {{.*}}|  0: <65534>                 |  }

